/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Cluster Pub/Sub Manager Implementation */

#include "pubsub_cluster.hpp"
#include "cluster_node.hpp"
#include "shard_hash.hpp"
#include "../core/ctx.hpp"
#include "../core/socket_base.hpp"
#include "../msg/msg.hpp"
#include "../util/err.hpp"
#include "../util/constants.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <chrono>

namespace slk
{

pubsub_cluster_t::pubsub_cluster_t (ctx_t *ctx)
    : _ctx (ctx)
    , _local_pub (nullptr)
{
    assert (ctx);

    // Create local XPUB socket for publishing
    // Other nodes in the cluster will subscribe to this
    _local_pub = _ctx->create_socket (SL_XPUB);
    if (_local_pub) {
        // Generate unique local endpoint
        auto now = std::chrono::high_resolution_clock::now ();
        auto timestamp = now.time_since_epoch ().count ();
        _local_pub_endpoint = "inproc://cluster-pub-" + std::to_string (timestamp);

        // Bind local publisher
        int linger = 0;
        _local_pub->setsockopt (SL_LINGER, &linger, sizeof (linger));
        _local_pub->bind (_local_pub_endpoint.c_str ());
    }
}

pubsub_cluster_t::~pubsub_cluster_t ()
{
    // Disconnect all nodes
    {
        std::unique_lock<std::shared_mutex> lock (_mutex);
        _nodes.clear ();
    }

    // Destroy local publisher
    if (_local_pub) {
        _ctx->destroy_socket (_local_pub);
        _local_pub = nullptr;
    }
}

int pubsub_cluster_t::add_node (const std::string &endpoint)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check if node already exists
    if (find_node (endpoint) != nullptr) {
        errno = EEXIST;
        return -1;
    }

    // Create new node
    auto node = std::make_unique<cluster_node_t> (_ctx, endpoint);

    // Connect to the node
    int rc = node->connect ();
    if (rc < 0) {
        return -1;
    }

    // Subscribe node to all existing pattern subscriptions
    for (const auto &pattern : _pattern_subscriptions) {
        node->add_pattern_subscription (pattern);
    }

    // Add to node list
    _nodes.push_back (std::move (node));

    // Rebuild slot mapping
    rebuild_slot_mapping ();

    return 0;
}

int pubsub_cluster_t::remove_node (const std::string &endpoint)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Find and remove node
    auto it = std::find_if (_nodes.begin (), _nodes.end (),
                           [&endpoint] (const auto &node) {
                               return node->get_endpoint () == endpoint;
                           });

    if (it == _nodes.end ()) {
        errno = ENOENT;
        return -1;
    }

    // Remove node (automatically disconnects via destructor)
    _nodes.erase (it);

    // Rebuild slot mapping
    rebuild_slot_mapping ();

    return 0;
}

int pubsub_cluster_t::publish (const std::string &channel,
                               const void *data,
                               size_t len)
{
    if (!_local_pub) {
        errno = ENOTSOCK;
        return -1;
    }

    // Verify we have nodes available
    {
        std::shared_lock<std::shared_mutex> lock (_mutex);

        if (_nodes.empty ()) {
            errno = EHOSTUNREACH;
            return -1;
        }
    }

    // Send channel frame
    msg_t channel_msg;
    channel_msg.init_size (channel.size ());
    std::memcpy (channel_msg.data (), channel.data (), channel.size ());
    channel_msg.set_flags (msg_t::more);

    int rc = _local_pub->send (&channel_msg, 0);
    channel_msg.close ();

    if (rc < 0) {
        return -1;
    }

    // Send data frame
    msg_t data_msg;
    data_msg.init_size (len);
    if (len > 0) {
        std::memcpy (data_msg.data (), data, len);
    }

    rc = _local_pub->send (&data_msg, 0);
    data_msg.close ();

    return rc;
}

int pubsub_cluster_t::subscribe (const std::string &channel)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Add to subscription set
    _subscriptions.insert (channel);

    if (_nodes.empty ()) {
        // No nodes yet, subscription will be applied when nodes are added
        return 0;
    }

    // Route to specific node based on hash
    size_t node_idx = get_node_for_channel (channel);

    if (node_idx < _nodes.size ()) {
        _nodes[node_idx]->add_subscription (channel);
    }

    return 0;
}

int pubsub_cluster_t::psubscribe (const std::string &pattern)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Add to pattern subscription set
    _pattern_subscriptions.insert (pattern);

    // Propagate to ALL nodes
    for (auto &node : _nodes) {
        node->add_pattern_subscription (pattern);
    }

    return 0;
}

int pubsub_cluster_t::unsubscribe (const std::string &channel)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Remove from subscription set
    _subscriptions.erase (channel);

    if (_nodes.empty ()) {
        return 0;
    }

    // Route to specific node
    size_t node_idx = get_node_for_channel (channel);

    if (node_idx < _nodes.size ()) {
        _nodes[node_idx]->remove_subscription (channel);
    }

    return 0;
}

int pubsub_cluster_t::punsubscribe (const std::string &pattern)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Remove from pattern subscription set
    _pattern_subscriptions.erase (pattern);

    // Remove from ALL nodes
    for (auto &node : _nodes) {
        node->remove_pattern_subscription (pattern);
    }

    return 0;
}

int pubsub_cluster_t::recv (std::string &channel,
                           std::vector<uint8_t> &data,
                           int flags)
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    if (_nodes.empty ()) {
        errno = EAGAIN;
        return -1;
    }

    // Try to receive from any connected node
    // In a real implementation, you would use a poller to wait on all nodes
    // For simplicity, we'll try each node in sequence with DONTWAIT

    for (auto &node : _nodes) {
        if (!node->is_connected ()) {
            continue;
        }

        int rc = node->recv (channel, data, flags | SL_DONTWAIT);
        if (rc >= 0) {
            return rc;
        }

        // If error is not EAGAIN, propagate it
        if (errno != EAGAIN && errno != EINTR) {
            return -1;
        }
    }

    // No messages available from any node
    if ((flags & SL_DONTWAIT) == 0) {
        // Blocking mode requested but not implemented properly
        // Would need to use poller to wait on all node sockets
        errno = ENOTSUP;
        return -1;
    }

    errno = EAGAIN;
    return -1;
}

std::vector<std::string> pubsub_cluster_t::get_nodes () const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    std::vector<std::string> endpoints;
    endpoints.reserve (_nodes.size ());

    for (const auto &node : _nodes) {
        endpoints.push_back (node->get_endpoint ());
    }

    return endpoints;
}

size_t pubsub_cluster_t::get_node_count () const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);
    return _nodes.size ();
}

size_t pubsub_cluster_t::get_subscription_count () const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);
    return _subscriptions.size ();
}

size_t pubsub_cluster_t::get_pattern_subscription_count () const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);
    return _pattern_subscriptions.size ();
}

size_t pubsub_cluster_t::get_node_for_channel (const std::string &channel) const
{
    // Must be called with lock held

    if (_nodes.empty ()) {
        return 0;
    }

    // Get hash slot using CRC16 (same as Redis Cluster)
    int slot = shard_hash_t::get_slot (channel);

    // Map slot to node index
    auto it = _slot_to_node.find (slot);
    if (it != _slot_to_node.end ()) {
        return it->second;
    }

    // Fallback: simple modulo
    return slot % _nodes.size ();
}

void pubsub_cluster_t::rebuild_slot_mapping ()
{
    // Must be called with lock held

    _slot_to_node.clear ();

    if (_nodes.empty ()) {
        return;
    }

    // Simple consistent hashing: distribute 16384 slots evenly
    size_t node_count = _nodes.size ();
    size_t slots_per_node = shard_hash_t::SLOT_COUNT / node_count;
    size_t extra_slots = shard_hash_t::SLOT_COUNT % node_count;

    size_t slot = 0;
    for (size_t node_idx = 0; node_idx < node_count; ++node_idx) {
        size_t node_slots = slots_per_node + (node_idx < extra_slots ? 1 : 0);

        for (size_t i = 0; i < node_slots; ++i) {
            _slot_to_node[slot++] = node_idx;
        }
    }
}

cluster_node_t *pubsub_cluster_t::find_node (const std::string &endpoint)
{
    // Must be called with lock held

    auto it = std::find_if (_nodes.begin (), _nodes.end (),
                           [&endpoint] (const auto &node) {
                               return node->get_endpoint () == endpoint;
                           });

    return (it != _nodes.end ()) ? it->get () : nullptr;
}

const cluster_node_t *pubsub_cluster_t::find_node (const std::string &endpoint) const
{
    // Must be called with lock held

    auto it = std::find_if (_nodes.begin (), _nodes.end (),
                           [&endpoint] (const auto &node) {
                               return node->get_endpoint () == endpoint;
                           });

    return (it != _nodes.end ()) ? it->get () : nullptr;
}

} // namespace slk

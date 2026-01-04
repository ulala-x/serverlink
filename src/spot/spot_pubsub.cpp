/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - SPOT PUB/SUB implementation (Scalable Partitioned Ordered Topics) */

#include "spot_pubsub.hpp"
#include "spot_node.hpp"
#include "topic_registry.hpp"
#include "subscription_manager.hpp"
#include "../core/ctx.hpp"
#include "../core/socket_base.hpp"
#include "../msg/msg.hpp"
#include "../util/err.hpp"
#include "../util/constants.hpp"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace slk
{

spot_pubsub_t::spot_pubsub_t (ctx_t *ctx_)
    : _ctx (ctx_)
    , _registry (new topic_registry_t ())
    , _sub_manager (new subscription_manager_t ())
    , _recv_socket (nullptr)
    , _server_socket (nullptr)
    , _sndhwm (1000)
    , _rcvhwm (1000)
    , _topic_counter (0)
{
    if (!_ctx) {
        throw std::invalid_argument ("Context pointer is null");
    }

    // Create receive socket (XSUB) for all subscriptions
    // Use create_socket() to properly initialize the context (reaper, I/O threads)
    _recv_socket = _ctx->create_socket (SL_XSUB);
    if (!_recv_socket) {
        throw std::runtime_error ("Failed to create XSUB socket");
    }

    // Set HWM for receive socket
    _recv_socket->setsockopt (SL_RCVHWM, &_rcvhwm, sizeof (_rcvhwm));
}

spot_pubsub_t::~spot_pubsub_t ()
{
    // Destroy all local publisher sockets
    for (auto &kv : _local_publishers) {
        if (kv.second) {
            kv.second->close ();
        }
    }
    _local_publishers.clear ();

    // Destroy all remote nodes
    _remote_topic_nodes.clear ();
    _nodes.clear ();

    // Destroy receive socket
    if (_recv_socket) {
        _recv_socket->close ();
        _recv_socket = nullptr;
    }

    // Destroy server socket
    if (_server_socket) {
        _server_socket->close ();
        _server_socket = nullptr;
    }
}

// ============================================================================
// Topic Ownership
// ============================================================================

int spot_pubsub_t::topic_create (const std::string &topic_id)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check if topic already exists
    if (_registry->has_topic (topic_id)) {
        errno = EEXIST;
        return -1;
    }

    // Register in topic registry (generates unique inproc endpoint)
    if (_registry->register_local (topic_id) != 0) {
        return -1; // errno already set by registry
    }

    // Get the generated endpoint
    const auto *entry = _registry->lookup (topic_id);
    assert (entry);
    assert (entry->location == topic_registry_t::topic_location_t::LOCAL);

    // Create XPUB socket for this topic
    socket_base_t *xpub = _ctx->create_socket (SL_XPUB);
    if (!xpub) {
        _registry->unregister (topic_id);
        errno = ENOMEM;
        return -1;
    }

    // Set HWM for publisher socket
    xpub->setsockopt (SL_SNDHWM, &_sndhwm, sizeof (_sndhwm));

    // Bind to inproc endpoint
    if (xpub->bind (entry->endpoint.c_str ()) != 0) {
        xpub->close ();
        _registry->unregister (topic_id);
        return -1;
    }

    // Store in local publishers map
    _local_publishers[topic_id] = xpub;

    return 0;
}

int spot_pubsub_t::topic_destroy (const std::string &topic_id)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Find topic
    auto it = _local_publishers.find (topic_id);
    if (it == _local_publishers.end ()) {
        errno = ENOENT;
        return -1;
    }

    // Close and remove XPUB socket
    socket_base_t *xpub = it->second;
    if (xpub) {
        xpub->close ();
    }
    _local_publishers.erase (it);

    // Unregister from topic registry
    _registry->unregister (topic_id);

    return 0;
}

int spot_pubsub_t::topic_route (const std::string &topic_id,
                                 const std::string &endpoint)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check if topic already exists
    if (_registry->has_topic (topic_id)) {
        errno = EEXIST;
        return -1;
    }

    // Find or create spot_node_t for this endpoint
    spot_node_t *node = nullptr;
    auto node_it = _nodes.find (endpoint);

    if (node_it == _nodes.end ()) {
        // Create new spot_node_t
        auto new_node = std::make_unique<spot_node_t> (_ctx, endpoint);

        // Connect to remote endpoint
        if (new_node->connect () != 0) {
            // Connection failed, errno already set (EHOSTUNREACH)
            return -1;
        }

        node = new_node.get ();
        _nodes[endpoint] = std::move (new_node);
    } else {
        node = node_it->second.get ();
    }

    // Register topic as REMOTE in registry
    if (_registry->register_remote (topic_id, endpoint) != 0) {
        // If this was a new node and registration failed, remove it
        if (_nodes.find (endpoint) != _nodes.end () &&
            _remote_topic_nodes.empty ()) {
            _nodes.erase (endpoint);
        }
        return -1; // errno already set by registry
    }

    // Map topic to node
    _remote_topic_nodes[topic_id] = node;

    return 0;
}

// ============================================================================
// Subscription API
// ============================================================================

int spot_pubsub_t::subscribe (const std::string &topic_id)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Lookup topic in registry
    const auto *entry = _registry->lookup (topic_id);
    if (!entry) {
        errno = ENOENT;
        return -1;
    }

    if (entry->location == topic_registry_t::topic_location_t::LOCAL) {
        // LOCAL topic: Connect XSUB to inproc endpoint
        if (_recv_socket->connect (entry->endpoint.c_str ()) != 0) {
            return -1;
        }

        // Add LOCAL subscription to manager
        subscription_manager_t::subscriber_t sub;
        sub.type = subscription_manager_t::subscriber_type_t::LOCAL;
        sub.socket = _recv_socket;

        if (_sub_manager->add_subscription (topic_id, sub) != 0) {
            // Already subscribed - not an error, just idempotent
            if (errno == EEXIST) {
                return 0;
            }
            return -1;
        }

        // Send subscription message to XPUB (topic filter)
        msg_t msg;
        if (msg.init_subscribe (topic_id.size (),
                                reinterpret_cast<const unsigned char *> (topic_id.data ())) != 0) {
            return -1;
        }

        int rc = _recv_socket->send (&msg, 0);
        msg.close ();

        return rc;

    } else {
        // REMOTE topic: Send SUBSCRIBE to spot_node_t
        auto node_it = _remote_topic_nodes.find (topic_id);
        if (node_it == _remote_topic_nodes.end ()) {
            errno = ENOENT;
            return -1;
        }

        spot_node_t *node = node_it->second;

        // Send SUBSCRIBE command to remote node
        if (node->send_subscribe (topic_id) != 0) {
            return -1;
        }

        // Add REMOTE subscription to manager
        subscription_manager_t::subscriber_t sub;
        sub.type = subscription_manager_t::subscriber_type_t::REMOTE;
        sub.socket = nullptr; // REMOTE subscriptions don't use socket

        if (_sub_manager->add_subscription (topic_id, sub) != 0) {
            // Already subscribed - not an error, just idempotent
            if (errno == EEXIST) {
                return 0;
            }
            return -1;
        }

        return 0;
    }
}

int spot_pubsub_t::subscribe_pattern (const std::string &pattern)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Pattern subscriptions are LOCAL only
    subscription_manager_t::subscriber_t sub;
    sub.type = subscription_manager_t::subscriber_type_t::LOCAL;
    sub.socket = _recv_socket;

    if (_sub_manager->add_pattern_subscription (pattern, sub) != 0) {
        return -1;
    }

    // For pattern subscriptions, we need to subscribe to all matching topics
    // that are currently registered
    std::vector<std::string> local_topics = _registry->get_local_topics ();

    for (const auto &topic_id : local_topics) {
        // Check if pattern matches this topic
        // Pattern matching logic is in subscription_manager_t
        // For now, we subscribe to the pattern filter directly
        // The actual filtering happens during recv()
    }

    return 0;
}

int spot_pubsub_t::subscribe_many (const std::vector<std::string> &topics)
{
    int failed_count = 0;

    for (const auto &topic_id : topics) {
        if (subscribe (topic_id) != 0) {
            failed_count++;
        }
    }

    if (failed_count > 0) {
        // Partial failure - errno from last failed subscribe
        return -1;
    }

    return 0;
}

int spot_pubsub_t::unsubscribe (const std::string &topic_id)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Lookup topic in registry
    const auto *entry = _registry->lookup (topic_id);
    if (!entry) {
        errno = ENOENT;
        return -1;
    }

    if (entry->location == topic_registry_t::topic_location_t::LOCAL) {
        // LOCAL topic: Remove local subscription
        subscription_manager_t::subscriber_t sub;
        sub.type = subscription_manager_t::subscriber_type_t::LOCAL;
        sub.socket = _recv_socket;

        if (_sub_manager->remove_subscription (topic_id, sub) != 0) {
            return -1; // errno already set
        }

        // Send unsubscription message to XPUB
        msg_t msg;
        if (msg.init_cancel (topic_id.size (),
                             reinterpret_cast<const unsigned char *> (topic_id.data ())) != 0) {
            return -1;
        }

        int rc = _recv_socket->send (&msg, 0);
        msg.close ();

        return rc;

    } else {
        // REMOTE topic: Send UNSUBSCRIBE to spot_node_t
        auto node_it = _remote_topic_nodes.find (topic_id);
        if (node_it == _remote_topic_nodes.end ()) {
            errno = ENOENT;
            return -1;
        }

        spot_node_t *node = node_it->second;

        // Send UNSUBSCRIBE command to remote node
        if (node->send_unsubscribe (topic_id) != 0) {
            return -1;
        }

        // Remove REMOTE subscription from manager
        subscription_manager_t::subscriber_t sub;
        sub.type = subscription_manager_t::subscriber_type_t::REMOTE;
        sub.socket = nullptr;

        if (_sub_manager->remove_subscription (topic_id, sub) != 0) {
            return -1; // errno already set
        }

        return 0;
    }
}

// ============================================================================
// Publishing API
// ============================================================================

int spot_pubsub_t::publish (const std::string &topic_id,
                             const void *data,
                             size_t len)
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    // Lookup topic in registry
    const auto *entry = _registry->lookup (topic_id);
    if (!entry) {
        errno = ENOENT;
        return -1;
    }

    if (entry->location == topic_registry_t::topic_location_t::LOCAL) {
        // LOCAL topic: Send to XPUB socket
        auto it = _local_publishers.find (topic_id);
        if (it == _local_publishers.end ()) {
            errno = ENOENT;
            return -1;
        }

        socket_base_t *xpub = it->second;

        // Send message in two parts: [topic_id][data]
        // Frame 1: Topic ID
        msg_t topic_msg;
        if (topic_msg.init_buffer (topic_id.data (), topic_id.size ()) != 0) {
            return -1;
        }

        if (xpub->send (&topic_msg, SL_SNDMORE) != 0) {
            topic_msg.close ();
            return -1;
        }
        topic_msg.close ();

        // Frame 2: Data
        msg_t data_msg;
        if (data_msg.init_buffer (data, len) != 0) {
            return -1;
        }

        int rc = xpub->send (&data_msg, 0);
        data_msg.close ();

        return rc;

    } else {
        // REMOTE topic: Send PUBLISH to spot_node_t
        auto node_it = _remote_topic_nodes.find (topic_id);
        if (node_it == _remote_topic_nodes.end ()) {
            errno = ENOENT;
            return -1;
        }

        spot_node_t *node = node_it->second;
        return node->send_publish (topic_id, data, len);
    }
}

// ============================================================================
// Receiving API
// ============================================================================

int spot_pubsub_t::recv (char *topic_buf, size_t topic_buf_size,
                          size_t *topic_len,
                          void *data_buf, size_t data_buf_size,
                          size_t *data_len,
                          int flags)
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    // Process incoming QUERY requests from server socket (non-blocking)
    // This allows the node to respond to cluster sync requests
    const_cast<spot_pubsub_t *> (this)->process_incoming_messages ();

    // Try LOCAL topics first (XSUB socket)
    msg_t topic_msg;
    if (topic_msg.init () != 0) {
        return -1;
    }

    int rc = _recv_socket->recv (&topic_msg, flags | SL_DONTWAIT);
    if (rc == 0) {
        // Got message from LOCAL topic
        // Copy topic to buffer
        size_t topic_size = topic_msg.size ();
        if (topic_size > topic_buf_size) {
            topic_msg.close ();
            errno = EMSGSIZE;
            return -1;
        }

        memcpy (topic_buf, topic_msg.data (), topic_size);
        *topic_len = topic_size;
        topic_msg.close ();

        // Receive Frame 2: Data
        msg_t data_msg;
        if (data_msg.init () != 0) {
            return -1;
        }

        if (_recv_socket->recv (&data_msg, flags) != 0) {
            data_msg.close ();
            return -1;
        }

        // Copy data to buffer
        size_t data_size = data_msg.size ();
        if (data_size > data_buf_size) {
            data_msg.close ();
            errno = EMSGSIZE;
            return -1;
        }

        memcpy (data_buf, data_msg.data (), data_size);
        *data_len = data_size;
        data_msg.close ();

        return 0;
    }

    topic_msg.close ();

    // No LOCAL message, try REMOTE topics
    for (auto &kv : _nodes) {
        spot_node_t *node = kv.second.get ();

        std::string remote_topic_id;
        std::vector<uint8_t> remote_data;

        rc = node->recv (remote_topic_id, remote_data, flags | SL_DONTWAIT);
        if (rc == 0) {
            // Got message from REMOTE topic
            // Copy topic to buffer
            if (remote_topic_id.size () > topic_buf_size) {
                errno = EMSGSIZE;
                return -1;
            }

            memcpy (topic_buf, remote_topic_id.data (), remote_topic_id.size ());
            *topic_len = remote_topic_id.size ();

            // Copy data to buffer
            if (remote_data.size () > data_buf_size) {
                errno = EMSGSIZE;
                return -1;
            }

            memcpy (data_buf, remote_data.data (), remote_data.size ());
            *data_len = remote_data.size ();

            return 0;
        }
    }

    // No message available from LOCAL or REMOTE
    if (flags & SL_DONTWAIT) {
        errno = EAGAIN;
        return -1;
    }

    // Blocking mode: wait on LOCAL socket
    // TODO: Implement proper polling across LOCAL + REMOTE sources
    if (topic_msg.init () != 0) {
        return -1;
    }

    if (_recv_socket->recv (&topic_msg, flags) != 0) {
        topic_msg.close ();
        return -1;
    }

    // Copy topic to buffer
    size_t topic_size = topic_msg.size ();
    if (topic_size > topic_buf_size) {
        topic_msg.close ();
        errno = EMSGSIZE;
        return -1;
    }

    memcpy (topic_buf, topic_msg.data (), topic_size);
    *topic_len = topic_size;
    topic_msg.close ();

    // Receive Frame 2: Data
    msg_t data_msg;
    if (data_msg.init () != 0) {
        return -1;
    }

    if (_recv_socket->recv (&data_msg, flags) != 0) {
        data_msg.close ();
        return -1;
    }

    // Copy data to buffer
    size_t data_size = data_msg.size ();
    if (data_size > data_buf_size) {
        data_msg.close ();
        errno = EMSGSIZE;
        return -1;
    }

    memcpy (data_buf, data_msg.data (), data_size);
    *data_len = data_size;
    data_msg.close ();

    return 0;
}

// ============================================================================
// Introspection API
// ============================================================================

std::vector<std::string> spot_pubsub_t::list_topics () const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);
    return _registry->get_all_topics ();
}

bool spot_pubsub_t::topic_exists (const std::string &topic_id) const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);
    return _registry->has_topic (topic_id);
}

bool spot_pubsub_t::topic_is_local (const std::string &topic_id) const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    const auto *entry = _registry->lookup (topic_id);
    if (!entry) {
        return false;
    }

    return entry->location == topic_registry_t::topic_location_t::LOCAL;
}

// ============================================================================
// Configuration API
// ============================================================================

int spot_pubsub_t::set_hwm (int sndhwm, int rcvhwm)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    _sndhwm = sndhwm;
    _rcvhwm = rcvhwm;

    // Apply to receive socket
    if (_recv_socket) {
        if (_recv_socket->setsockopt (SL_RCVHWM, &_rcvhwm, sizeof (_rcvhwm)) != 0) {
            return -1;
        }
    }

    // Apply to all existing publisher sockets
    for (auto &kv : _local_publishers) {
        if (kv.second) {
            if (kv.second->setsockopt (SL_SNDHWM, &_sndhwm, sizeof (_sndhwm)) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

// ============================================================================
// Cluster Management
// ============================================================================

int spot_pubsub_t::bind (const std::string &endpoint)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check if already bound
    if (_server_socket) {
        errno = EEXIST;
        return -1;
    }

    // Create ROUTER socket for server mode
    _server_socket = _ctx->create_socket (SL_ROUTER);
    if (!_server_socket) {
        errno = ENOMEM;
        return -1;
    }

    // Bind to endpoint
    if (_server_socket->bind (endpoint.c_str ()) != 0) {
        _server_socket->close ();
        _server_socket = nullptr;
        return -1;
    }

    return 0;
}

int spot_pubsub_t::cluster_add (const std::string &endpoint)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check if node already exists
    if (_nodes.find (endpoint) != _nodes.end ()) {
        errno = EEXIST;
        return -1;
    }

    // Create new spot_node_t
    auto new_node = std::make_unique<spot_node_t> (_ctx, endpoint);

    // Connect to remote endpoint
    if (new_node->connect () != 0) {
        // Connection failed, errno already set (EHOSTUNREACH)
        return -1;
    }

    _nodes[endpoint] = std::move (new_node);
    return 0;
}

int spot_pubsub_t::cluster_remove (const std::string &endpoint)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    auto it = _nodes.find (endpoint);
    if (it == _nodes.end ()) {
        errno = ENOENT;
        return -1;
    }

    // Remove all remote topics associated with this node
    auto topic_it = _remote_topic_nodes.begin ();
    while (topic_it != _remote_topic_nodes.end ()) {
        if (topic_it->second == it->second.get ()) {
            // Unregister topic from registry
            _registry->unregister (topic_it->first);
            topic_it = _remote_topic_nodes.erase (topic_it);
        } else {
            ++topic_it;
        }
    }

    // Remove node
    _nodes.erase (it);
    return 0;
}

int spot_pubsub_t::cluster_sync (int timeout_ms)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    if (_nodes.empty ()) {
        // No cluster nodes to sync with
        return 0;
    }

    // Send QUERY to all cluster nodes
    for (auto &kv : _nodes) {
        spot_node_t *node = kv.second.get ();
        if (node->send_query () != 0) {
            // Failed to send query to this node, continue with others
            continue;
        }
    }

    // Receive QUERY_RESP from all nodes (with timeout)
    // Note: This is a simplified implementation that waits for all responses
    // A more robust implementation would use poller with timeout
    for (auto &kv : _nodes) {
        const std::string &endpoint = kv.first;
        spot_node_t *node = kv.second.get ();

        std::vector<std::string> topics;
        int rc = node->recv_query_response (topics, SL_DONTWAIT);

        if (rc == 0) {
            // Successfully received topic list
            // Register each topic as REMOTE in registry
            for (const auto &topic_id : topics) {
                // Check if topic already exists
                if (!_registry->has_topic (topic_id)) {
                    // Register as REMOTE topic
                    if (_registry->register_remote (topic_id, endpoint) == 0) {
                        // Map topic to node
                        _remote_topic_nodes[topic_id] = node;
                    }
                }
            }
        }
        // If recv failed (EAGAIN or other error), skip this node
    }

    return 0;
}

// ============================================================================
// Internal Helper Methods
// ============================================================================

void spot_pubsub_t::process_incoming_messages ()
{
    if (!_server_socket) {
        return;
    }

    // Try to receive messages from server socket (QUERY requests)
    msg_t routing_id_msg;
    if (routing_id_msg.init () != 0) {
        return;
    }

    int rc = _server_socket->recv (&routing_id_msg, SL_DONTWAIT);
    if (rc != 0) {
        routing_id_msg.close ();
        return; // No message available
    }

    // Extract routing ID
    std::string routing_id (static_cast<const char *> (routing_id_msg.data ()),
                            routing_id_msg.size ());
    routing_id_msg.close ();

    // Receive empty delimiter frame
    msg_t delimiter_msg;
    if (delimiter_msg.init () != 0) {
        return;
    }
    _server_socket->recv (&delimiter_msg, 0);
    delimiter_msg.close ();

    // Receive command frame
    msg_t cmd_msg;
    if (cmd_msg.init () != 0) {
        return;
    }

    if (_server_socket->recv (&cmd_msg, 0) != 0) {
        cmd_msg.close ();
        return;
    }

    if (cmd_msg.size () != 1) {
        cmd_msg.close ();
        return;
    }

    uint8_t cmd = *static_cast<const uint8_t *> (cmd_msg.data ());
    cmd_msg.close ();

    // Handle QUERY command
    if (cmd == static_cast<uint8_t> (spot_command_t::QUERY)) {
        handle_query_request (routing_id);
    }
    // Other commands (SUBSCRIBE, UNSUBSCRIBE, PUBLISH) can be handled here in the future
}

int spot_pubsub_t::handle_query_request (const std::string &routing_id)
{
    if (!_server_socket) {
        errno = EINVAL;
        return -1;
    }

    // Get list of local topics
    std::vector<std::string> local_topics = _registry->get_local_topics ();

    // Send QUERY_RESP message
    // Frame 0: Routing ID
    msg_t rid_msg;
    if (rid_msg.init_buffer (routing_id.data (), routing_id.size ()) != 0) {
        return -1;
    }

    if (_server_socket->send (&rid_msg, SL_SNDMORE) != 0) {
        rid_msg.close ();
        return -1;
    }
    rid_msg.close ();

    // Frame 1: Empty delimiter
    msg_t delim_msg;
    if (delim_msg.init_buffer ("", 0) != 0) {
        return -1;
    }

    if (_server_socket->send (&delim_msg, SL_SNDMORE) != 0) {
        delim_msg.close ();
        return -1;
    }
    delim_msg.close ();

    // Frame 2: Command (QUERY_RESP)
    msg_t cmd_msg;
    uint8_t cmd = static_cast<uint8_t> (spot_command_t::QUERY_RESP);
    if (cmd_msg.init_buffer (&cmd, sizeof (cmd)) != 0) {
        return -1;
    }

    if (_server_socket->send (&cmd_msg, SL_SNDMORE) != 0) {
        cmd_msg.close ();
        return -1;
    }
    cmd_msg.close ();

    // Frame 3: Topic count (uint32_t)
    msg_t count_msg;
    uint32_t topic_count = static_cast<uint32_t> (local_topics.size ());
    if (count_msg.init_buffer (&topic_count, sizeof (topic_count)) != 0) {
        return -1;
    }

    if (_server_socket->send (&count_msg, local_topics.empty () ? 0 : SL_SNDMORE) != 0) {
        count_msg.close ();
        return -1;
    }
    count_msg.close ();

    // Frame 4+: Topic IDs
    for (size_t i = 0; i < local_topics.size (); i++) {
        msg_t topic_msg;
        if (topic_msg.init_buffer (local_topics[i].data (),
                                    local_topics[i].size ()) != 0) {
            return -1;
        }

        int flags = (i == local_topics.size () - 1) ? 0 : SL_SNDMORE;
        if (_server_socket->send (&topic_msg, flags) != 0) {
            topic_msg.close ();
            return -1;
        }
        topic_msg.close ();
    }

    return 0;
}

// ============================================================================
// Event Loop Integration
// ============================================================================

int spot_pubsub_t::fd (int *fd) const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    if (!_recv_socket) {
        errno = EINVAL;
        return -1;
    }

    // Get file descriptor from receive socket
    size_t fd_size = sizeof (*fd);
    return _recv_socket->getsockopt (SL_FD, fd, &fd_size);
}

} // namespace slk

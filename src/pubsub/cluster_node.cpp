/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Cluster Node Connection Manager */

#include "cluster_node.hpp"
#include "../core/ctx.hpp"
#include "../core/socket_base.hpp"
#include "../msg/msg.hpp"
#include "../util/err.hpp"
#include "../util/constants.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace slk
{

cluster_node_t::cluster_node_t (ctx_t *ctx, const std::string &endpoint)
    : _ctx (ctx)
    , _endpoint (endpoint)
    , _socket (nullptr)
    , _state (node_state_t::DISCONNECTED)
    , _reconnect_attempts (0)
{
    assert (ctx);
    _last_heartbeat = std::chrono::steady_clock::now ();
    _last_reconnect_attempt = std::chrono::steady_clock::now ();
}

cluster_node_t::~cluster_node_t ()
{
    disconnect ();
}

int cluster_node_t::connect ()
{
    std::lock_guard<std::mutex> lock (_mutex);

    // Already connected or connecting
    if (_socket != nullptr) {
        return 0;
    }

    // Create SUB socket for receiving messages from this node
    _socket = _ctx->create_socket (SL_SUB);
    if (!_socket) {
        errno = ENOMEM;
        return -1;
    }

    // Subscribe to all messages (empty subscription = all)
    int rc = _socket->setsockopt (SL_SUBSCRIBE, "", 0);
    if (rc < 0) {
        _ctx->destroy_socket (_socket);
        _socket = nullptr;
        return -1;
    }

    // Set linger to 0 for fast shutdown
    int linger = 0;
    _socket->setsockopt (SL_LINGER, &linger, sizeof (linger));

    // Connect to remote node
    rc = _socket->connect (_endpoint.c_str ());
    if (rc < 0) {
        _ctx->destroy_socket (_socket);
        _socket = nullptr;
        return -1;
    }

    _state.store (node_state_t::CONNECTED);
    _last_heartbeat = std::chrono::steady_clock::now ();
    _reconnect_attempts = 0;

    // Restore subscriptions
    restore_subscriptions ();

    return 0;
}

int cluster_node_t::disconnect ()
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (_socket) {
        _ctx->destroy_socket (_socket);
        _socket = nullptr;
    }

    _state.store (node_state_t::DISCONNECTED);
    return 0;
}

int cluster_node_t::publish (const std::string &channel,
                             const void *data,
                             size_t len)
{
    std::lock_guard<std::mutex> lock (_mutex);

    // Cannot publish through a SUB socket
    // This method is not used in the current design
    // Publishing happens through the cluster manager's XPUB sockets
    errno = ENOTSUP;
    return -1;
}

int cluster_node_t::recv (std::string &channel,
                         std::vector<uint8_t> &data,
                         int flags)
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (!_socket) {
        errno = ENOTCONN;
        return -1;
    }

    // Receive channel frame
    msg_t channel_msg;
    channel_msg.init ();

    int rc = _socket->recv (&channel_msg, flags);
    if (rc < 0) {
        channel_msg.close ();
        return -1;
    }

    // Extract channel name
    channel.assign (static_cast<const char *> (channel_msg.data ()),
                   channel_msg.size ());

    int more = 0;
    size_t more_size = sizeof (more);
    _socket->getsockopt (SL_RCVMORE, &more, &more_size);

    channel_msg.close ();

    if (!more) {
        // No data frame, this is an error
        errno = EPROTO;
        return -1;
    }

    // Receive data frame
    msg_t data_msg;
    data_msg.init ();

    rc = _socket->recv (&data_msg, flags);
    if (rc < 0) {
        data_msg.close ();
        return -1;
    }

    // Extract data
    size_t data_len = data_msg.size ();
    data.resize (data_len);
    if (data_len > 0) {
        std::memcpy (data.data (), data_msg.data (), data_len);
    }

    data_msg.close ();

    // Update heartbeat on successful receive
    _last_heartbeat = std::chrono::steady_clock::now ();

    return static_cast<int> (data_len);
}

void cluster_node_t::add_subscription (const std::string &channel)
{
    std::lock_guard<std::mutex> lock (_mutex);

    // Add to subscription list if not already present
    auto it = std::find (_subscriptions.begin (), _subscriptions.end (), channel);
    if (it == _subscriptions.end ()) {
        _subscriptions.push_back (channel);
    }

    // If connected, send subscription now
    if (_socket) {
        _socket->setsockopt (SL_SUBSCRIBE, channel.c_str (), channel.size ());
    }
}

void cluster_node_t::add_pattern_subscription (const std::string &pattern)
{
    std::lock_guard<std::mutex> lock (_mutex);

    // Add to pattern subscription list if not already present
    auto it = std::find (_pattern_subscriptions.begin (),
                        _pattern_subscriptions.end (),
                        pattern);
    if (it == _pattern_subscriptions.end ()) {
        _pattern_subscriptions.push_back (pattern);
    }

    // If connected, send pattern subscription now
    if (_socket) {
        _socket->setsockopt (SL_PSUBSCRIBE, pattern.c_str (), pattern.size ());
    }
}

void cluster_node_t::remove_subscription (const std::string &channel)
{
    std::lock_guard<std::mutex> lock (_mutex);

    // Remove from subscription list
    auto it = std::find (_subscriptions.begin (), _subscriptions.end (), channel);
    if (it != _subscriptions.end ()) {
        _subscriptions.erase (it);
    }

    // If connected, send unsubscription now
    if (_socket) {
        _socket->setsockopt (SL_UNSUBSCRIBE, channel.c_str (), channel.size ());
    }
}

void cluster_node_t::remove_pattern_subscription (const std::string &pattern)
{
    std::lock_guard<std::mutex> lock (_mutex);

    // Remove from pattern subscription list
    auto it = std::find (_pattern_subscriptions.begin (),
                        _pattern_subscriptions.end (),
                        pattern);
    if (it != _pattern_subscriptions.end ()) {
        _pattern_subscriptions.erase (it);
    }

    // If connected, send pattern unsubscription now
    if (_socket) {
        _socket->setsockopt (SL_PUNSUBSCRIBE, pattern.c_str (), pattern.size ());
    }
}

node_state_t cluster_node_t::get_state () const
{
    return _state.load ();
}

bool cluster_node_t::is_connected () const
{
    return _state.load () == node_state_t::CONNECTED;
}

void cluster_node_t::update_heartbeat ()
{
    _last_heartbeat = std::chrono::steady_clock::now ();
}

bool cluster_node_t::is_heartbeat_timeout (int timeout_ms) const
{
    auto now = std::chrono::steady_clock::now ();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds> (
        now - _last_heartbeat);
    return elapsed.count () > timeout_ms;
}

socket_base_t *cluster_node_t::get_socket ()
{
    std::lock_guard<std::mutex> lock (_mutex);
    return _socket;
}

int cluster_node_t::restore_subscriptions ()
{
    // Must be called with _mutex held

    if (!_socket) {
        return -1;
    }

    // Restore exact subscriptions
    for (const auto &channel : _subscriptions) {
        int rc = _socket->setsockopt (SL_SUBSCRIBE,
                                     channel.c_str (),
                                     channel.size ());
        if (rc < 0) {
            return -1;
        }
    }

    // Restore pattern subscriptions
    for (const auto &pattern : _pattern_subscriptions) {
        int rc = _socket->setsockopt (SL_PSUBSCRIBE,
                                     pattern.c_str (),
                                     pattern.size ());
        if (rc < 0) {
            return -1;
        }
    }

    return 0;
}

} // namespace slk

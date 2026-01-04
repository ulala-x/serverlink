/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - SPOT Node (Remote Node Connection) */

#include "spot_node.hpp"
#include "../core/ctx.hpp"
#include "../core/socket_base.hpp"
#include "../msg/msg.hpp"
#include "../util/err.hpp"
#include "../util/constants.hpp"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>

namespace slk
{

// Generate unique node ID for DEALER socket identity
static std::string generate_node_id ()
{
    // Use counter-based unique ID generation
    static std::atomic<uint64_t> counter {0};
    char buf[64];
    snprintf (buf, sizeof (buf), "spot-node-%llu",
              static_cast<unsigned long long> (counter.fetch_add (1, std::memory_order_relaxed)));
    return buf;
}

spot_node_t::spot_node_t (ctx_t *ctx, const std::string &endpoint)
    : _ctx (ctx)
    , _endpoint (endpoint)
    , _socket (nullptr)
    , _connected (false)
    , _reconnect_ivl (100)
    , _reconnect_ivl_max (5000)
{
    assert (ctx);
    assert (!endpoint.empty ());
}

spot_node_t::~spot_node_t ()
{
    // Use single lock to avoid race condition between is_connected() and disconnect()
    std::lock_guard<std::mutex> lock (_mutex);

    if (_connected && _socket) {
        _socket->close ();
        _socket = nullptr;
        _connected = false;
    }
}

int spot_node_t::connect ()
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (_connected) {
        return 0; // Already connected
    }

    // Create DEALER socket for DEALER-to-ROUTER communication
    _socket = _ctx->create_socket (SL_DEALER);
    if (!_socket) {
        errno = ENOMEM;
        return -1;
    }

    // Generate unique node ID if not already set
    if (_node_id.empty ()) {
        _node_id = generate_node_id ();
    }

    // Set CONNECT_ROUTING_ID to identify this DEALER to the remote ROUTER
    // The ROUTER will use this as the routing ID when sending messages back
    int rc = _socket->setsockopt (SL_CONNECT_ROUTING_ID, _node_id.data (), _node_id.size ());
    if (rc != 0) {
        // Note: If this fails, we continue anyway as it's not critical for basic operation
        // The connection will still work with an auto-generated routing ID
    }

    // Set reconnection parameters
    _socket->setsockopt (SL_RECONNECT_IVL, &_reconnect_ivl, sizeof (_reconnect_ivl));
    _socket->setsockopt (SL_RECONNECT_IVL_MAX, &_reconnect_ivl_max, sizeof (_reconnect_ivl_max));

    // Connect to remote endpoint
    if (_socket->connect (_endpoint.c_str ()) != 0) {
        _socket->close ();
        _socket = nullptr;
        return -1;
    }

    _connected = true;
    return 0;
}

int spot_node_t::disconnect ()
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (!_connected) {
        return 0; // Already disconnected
    }

    if (_socket) {
        _socket->close ();
        _socket = nullptr;
    }

    _connected = false;
    return 0;
}

bool spot_node_t::is_connected () const
{
    std::lock_guard<std::mutex> lock (_mutex);
    return _connected;
}

int spot_node_t::send_publish (const std::string &topic_id,
                                const void *data,
                                size_t len)
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (!_connected || !_socket) {
        errno = EHOSTUNREACH;
        return -1;
    }

    // DEALER→ROUTER: Send frames directly (no routing ID needed)

    // Frame 0: Command (PUBLISH)
    msg_t cmd_msg;
    uint8_t cmd = static_cast<uint8_t> (spot_command_t::PUBLISH);
    if (cmd_msg.init_buffer (&cmd, sizeof (cmd)) != 0) {
        return -1;
    }

    if (_socket->send (&cmd_msg, SL_SNDMORE) != 0) {
        cmd_msg.close ();
        return -1;
    }
    cmd_msg.close ();

    // Frame 1: Origin ID (for loop prevention)
    msg_t origin_msg;
    if (origin_msg.init_buffer (_node_id.data (), _node_id.size ()) != 0) {
        return -1;
    }

    if (_socket->send (&origin_msg, SL_SNDMORE) != 0) {
        origin_msg.close ();
        return -1;
    }
    origin_msg.close ();

    // Frame 2: Topic ID
    msg_t topic_msg;
    if (topic_msg.init_buffer (topic_id.data (), topic_id.size ()) != 0) {
        return -1;
    }

    if (_socket->send (&topic_msg, SL_SNDMORE) != 0) {
        topic_msg.close ();
        return -1;
    }
    topic_msg.close ();

    // Frame 3: Data
    msg_t data_msg;
    if (data_msg.init_buffer (data, len) != 0) {
        return -1;
    }

    int rc = _socket->send (&data_msg, 0);
    data_msg.close ();

    return rc;
}

int spot_node_t::send_subscribe (const std::string &topic_id)
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (!_connected || !_socket) {
        errno = EHOSTUNREACH;
        return -1;
    }

    // DEALER→ROUTER: Send frames directly (no routing ID needed)
    // The ROUTER will automatically prepend the DEALER's routing ID

    // Frame 0: Command (SUBSCRIBE)
    msg_t cmd_msg;
    uint8_t cmd = static_cast<uint8_t> (spot_command_t::SUBSCRIBE);
    if (cmd_msg.init_buffer (&cmd, sizeof (cmd)) != 0) {
        return -1;
    }

    if (_socket->send (&cmd_msg, SL_SNDMORE) != 0) {
        cmd_msg.close ();
        return -1;
    }
    cmd_msg.close ();

    // Frame 1: Topic ID
    msg_t topic_msg;
    if (topic_msg.init_buffer (topic_id.data (), topic_id.size ()) != 0) {
        return -1;
    }

    int rc = _socket->send (&topic_msg, 0);
    topic_msg.close ();

    return rc;
}

int spot_node_t::send_unsubscribe (const std::string &topic_id)
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (!_connected || !_socket) {
        errno = EHOSTUNREACH;
        return -1;
    }

    // DEALER→ROUTER: Send frames directly (no routing ID needed)

    // Frame 0: Command (UNSUBSCRIBE)
    msg_t cmd_msg;
    uint8_t cmd = static_cast<uint8_t> (spot_command_t::UNSUBSCRIBE);
    if (cmd_msg.init_buffer (&cmd, sizeof (cmd)) != 0) {
        return -1;
    }

    if (_socket->send (&cmd_msg, SL_SNDMORE) != 0) {
        cmd_msg.close ();
        return -1;
    }
    cmd_msg.close ();

    // Frame 1: Topic ID
    msg_t topic_msg;
    if (topic_msg.init_buffer (topic_id.data (), topic_id.size ()) != 0) {
        return -1;
    }

    int rc = _socket->send (&topic_msg, 0);
    topic_msg.close ();

    return rc;
}

int spot_node_t::send_query ()
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (!_connected || !_socket) {
        errno = EHOSTUNREACH;
        return -1;
    }

    // DEALER→ROUTER: Send frames directly (no routing ID needed)

    // Frame 0: Command (QUERY)
    msg_t cmd_msg;
    uint8_t cmd = static_cast<uint8_t> (spot_command_t::QUERY);
    if (cmd_msg.init_buffer (&cmd, sizeof (cmd)) != 0) {
        return -1;
    }

    int rc = _socket->send (&cmd_msg, 0);
    cmd_msg.close ();

    return rc;
}

int spot_node_t::recv_query_response (std::vector<std::string> &topics,
                                       int flags)
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (!_connected || !_socket) {
        errno = EHOSTUNREACH;
        return -1;
    }

    topics.clear ();

    // DEALER receives: [empty][cmd][count][topics...]
    // The routing ID is stripped by DEALER socket

    // Frame 0: Empty delimiter
    msg_t empty_msg;
    if (empty_msg.init () != 0) {
        return -1;
    }

    if (_socket->recv (&empty_msg, flags) != 0) {
        empty_msg.close ();
        return -1;
    }
    empty_msg.close ();

    // Frame 1: Command (QUERY_RESP)
    msg_t cmd_msg;
    if (cmd_msg.init () != 0) {
        return -1;
    }

    if (_socket->recv (&cmd_msg, 0) != 0) {
        cmd_msg.close ();
        return -1;
    }

    // Verify command is QUERY_RESP
    if (cmd_msg.size () != 1) {
        cmd_msg.close ();
        errno = EPROTO;
        return -1;
    }

    uint8_t cmd = *static_cast<const uint8_t *> (cmd_msg.data ());
    cmd_msg.close ();

    if (cmd != static_cast<uint8_t> (spot_command_t::QUERY_RESP)) {
        errno = EPROTO;
        return -1;
    }

    // Frame 2: Topic count (uint32_t)
    msg_t count_msg;
    if (count_msg.init () != 0) {
        return -1;
    }

    if (_socket->recv (&count_msg, 0) != 0) {
        count_msg.close ();
        return -1;
    }

    if (count_msg.size () != sizeof (uint32_t)) {
        count_msg.close ();
        errno = EPROTO;
        return -1;
    }

    uint32_t topic_count;
    memcpy (&topic_count, count_msg.data (), sizeof (uint32_t));
    count_msg.close ();

    // Frame 3+: Topic IDs
    for (uint32_t i = 0; i < topic_count; i++) {
        msg_t topic_msg;
        if (topic_msg.init () != 0) {
            return -1;
        }

        if (_socket->recv (&topic_msg, 0) != 0) {
            topic_msg.close ();
            return -1;
        }

        std::string topic_id (static_cast<const char *> (topic_msg.data ()),
                              topic_msg.size ());
        topics.push_back (topic_id);
        topic_msg.close ();
    }

    return 0;
}

int spot_node_t::recv_query_response_blocking (std::vector<std::string> &topics,
                                                 int timeout_ms)
{
    // Handle special timeout values
    if (timeout_ms == 0) {
        // Return immediately
        return recv_query_response (topics, SL_DONTWAIT);
    }

    // Calculate deadline for timeout
    auto start = std::chrono::steady_clock::now ();
    std::chrono::milliseconds timeout_duration (timeout_ms);
    // Use parentheses around max to prevent Windows macro expansion
    auto deadline = (timeout_ms > 0) ? (start + timeout_duration)
                                     : (std::chrono::steady_clock::time_point::max) ();

    // Retry loop with timeout
    while (true) {
        // Try non-blocking receive
        int rc = recv_query_response (topics, SL_DONTWAIT);
        if (rc == 0) {
            return 0; // Success
        }

        // Check if error is not EAGAIN (actual error)
        if (errno != EAGAIN) {
            return -1; // Propagate error
        }

        // Check timeout
        auto now = std::chrono::steady_clock::now ();
        if (now >= deadline) {
            errno = ETIMEDOUT;
            return -1;
        }

        // Sleep briefly to avoid busy-waiting
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
}

int spot_node_t::recv (std::string &topic_id,
                       std::vector<uint8_t> &data,
                       int flags)
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (!_connected || !_socket) {
        errno = EHOSTUNREACH;
        return -1;
    }

    // DEALER receives: [empty][cmd][origin][topic][data]
    // The routing ID is stripped by DEALER socket

    // Frame 0: Empty delimiter
    msg_t empty_msg;
    if (empty_msg.init () != 0) {
        return -1;
    }

    if (_socket->recv (&empty_msg, flags) != 0) {
        empty_msg.close ();
        return -1;
    }
    empty_msg.close ();

    // Frame 1: Command
    msg_t cmd_msg;
    if (cmd_msg.init () != 0) {
        return -1;
    }

    if (_socket->recv (&cmd_msg, 0) != 0) {
        cmd_msg.close ();
        return -1;
    }

    // Verify command is PUBLISH
    if (cmd_msg.size () != 1) {
        cmd_msg.close ();
        errno = EPROTO;
        return -1;
    }

    uint8_t cmd = *static_cast<const uint8_t *> (cmd_msg.data ());
    cmd_msg.close ();

    if (cmd != static_cast<uint8_t> (spot_command_t::PUBLISH)) {
        errno = EPROTO;
        return -1;
    }

    // Frame 2: Origin ID (for loop prevention - discard)
    msg_t origin_msg;
    if (origin_msg.init () != 0) {
        return -1;
    }
    _socket->recv (&origin_msg, 0);
    origin_msg.close ();

    // Frame 3: Topic ID
    msg_t topic_msg;
    if (topic_msg.init () != 0) {
        return -1;
    }

    if (_socket->recv (&topic_msg, 0) != 0) {
        topic_msg.close ();
        return -1;
    }

    topic_id.assign (static_cast<const char *> (topic_msg.data ()),
                     topic_msg.size ());
    topic_msg.close ();

    // Frame 4: Data
    msg_t data_msg;
    if (data_msg.init () != 0) {
        return -1;
    }

    if (_socket->recv (&data_msg, 0) != 0) {
        data_msg.close ();
        return -1;
    }

    const uint8_t *data_ptr = static_cast<const uint8_t *> (data_msg.data ());
    data.assign (data_ptr, data_ptr + data_msg.size ());
    data_msg.close ();

    return 0;
}

int spot_node_t::fd (slk_fd_t *fd) const
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (!_socket) {
        errno = EINVAL;
        return -1;
    }

    size_t fd_size = sizeof (*fd);
    return _socket->getsockopt (SL_FD, fd, &fd_size);
}

const std::string &spot_node_t::endpoint () const
{
    return _endpoint;
}

const std::string &spot_node_t::node_id () const
{
    return _node_id;
}

} // namespace slk

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

namespace slk
{

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
    if (is_connected ()) {
        disconnect ();
    }
}

int spot_node_t::connect ()
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (_connected) {
        return 0; // Already connected
    }

    // Create DEALER socket (Note: ServerLink doesn't have DEALER yet, using ROUTER as placeholder)
    // TODO: When DEALER is implemented, change SL_ROUTER to SL_DEALER
    _socket = _ctx->create_socket (SL_ROUTER);
    if (!_socket) {
        errno = ENOMEM;
        return -1;
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

    // Frame 1: Topic ID
    msg_t topic_msg;
    if (topic_msg.init_buffer (topic_id.data (), topic_id.size ()) != 0) {
        return -1;
    }

    if (_socket->send (&topic_msg, SL_SNDMORE) != 0) {
        topic_msg.close ();
        return -1;
    }
    topic_msg.close ();

    // Frame 2: Data
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

    // Frame 0: Command (QUERY_RESP)
    msg_t cmd_msg;
    if (cmd_msg.init () != 0) {
        return -1;
    }

    if (_socket->recv (&cmd_msg, flags) != 0) {
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

    // Frame 1: Topic count (uint32_t)
    msg_t count_msg;
    if (count_msg.init () != 0) {
        return -1;
    }

    if (_socket->recv (&count_msg, flags) != 0) {
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

    // Frame 2+: Topic IDs
    for (uint32_t i = 0; i < topic_count; i++) {
        msg_t topic_msg;
        if (topic_msg.init () != 0) {
            return -1;
        }

        if (_socket->recv (&topic_msg, flags) != 0) {
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

int spot_node_t::recv (std::string &topic_id,
                       std::vector<uint8_t> &data,
                       int flags)
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (!_connected || !_socket) {
        errno = EHOSTUNREACH;
        return -1;
    }

    // Frame 0: Command
    msg_t cmd_msg;
    if (cmd_msg.init () != 0) {
        return -1;
    }

    if (_socket->recv (&cmd_msg, flags) != 0) {
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

    // Frame 1: Topic ID
    msg_t topic_msg;
    if (topic_msg.init () != 0) {
        return -1;
    }

    if (_socket->recv (&topic_msg, flags) != 0) {
        topic_msg.close ();
        return -1;
    }

    topic_id.assign (static_cast<const char *> (topic_msg.data ()),
                     topic_msg.size ());
    topic_msg.close ();

    // Frame 2: Data
    msg_t data_msg;
    if (data_msg.init () != 0) {
        return -1;
    }

    if (_socket->recv (&data_msg, flags) != 0) {
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

} // namespace slk

/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "precompiled.hpp"
#include "../util/macros.hpp"
#include "router.hpp"
#include "../pipe/pipe.hpp"
#include "../protocol/wire.hpp"
#include "../util/random.hpp"
#include "../util/likely.hpp"
#include "../util/err.hpp"
#include "../util/clock.hpp"
#include "../monitor/connection_manager.hpp"
#include "../monitor/event_dispatcher.hpp"
#include "../monitor/heartbeat.hpp"
#include <new>

slk::router_t::router_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    routing_socket_base_t (parent_, tid_, sid_),
    _prefetched (false),
    _routing_id_sent (false),
    _current_in (NULL),
    _terminate_current_in (false),
    _more_in (false),
    _current_out (NULL),
    _more_out (false),
    _next_integral_routing_id (generate_random ()),
    _mandatory (false),
    // raw_socket functionality in ROUTER is deprecated
    _raw_socket (false),
    _probe_router (false),
    _handover (false)
#ifdef SL_ENABLE_MONITORING
    ,_conn_manager (NULL),
    _event_dispatcher (NULL)
#endif
{
    options.type = SL_ROUTER;
    options.recv_routing_id = true;
    options.raw_socket = false;
    options.can_send_hello_msg = true;
    options.can_recv_disconnect_msg = true;

    _prefetched_id.init ();
    _prefetched_msg.init ();

#ifdef SL_ENABLE_MONITORING
    // Initialize monitoring components
    _conn_manager = new (std::nothrow) connection_manager_t ();
    slk_assert (_conn_manager);

    _event_dispatcher = new (std::nothrow) event_dispatcher_t ();
    slk_assert (_event_dispatcher);
#endif
}

slk::router_t::~router_t ()
{
    slk_assert (_anonymous_pipes.empty ());
    _prefetched_id.close ();
    _prefetched_msg.close ();

#ifdef SL_ENABLE_MONITORING
    // Clean up monitoring components
    if (_conn_manager) {
        delete _conn_manager;
        _conn_manager = NULL;
    }

    if (_event_dispatcher) {
        delete _event_dispatcher;
        _event_dispatcher = NULL;
    }
#endif
}

void slk::router_t::xattach_pipe (pipe_t *pipe_,
                                  bool subscribe_to_all_,
                                  bool locally_initiated_)
{
    SL_UNUSED (subscribe_to_all_);

    slk_assert (pipe_);

    if (_probe_router) {
        msg_t probe_msg;
        int rc = probe_msg.init ();
        errno_assert (rc == 0);

        rc = pipe_->write (&probe_msg);
        // slk_assert (rc) is not applicable here, since it is not a bug
        SL_UNUSED (rc);

        pipe_->flush ();

        rc = probe_msg.close ();
        errno_assert (rc == 0);
    }

    const bool routing_id_ok = identify_peer (pipe_, locally_initiated_);
    if (routing_id_ok)
        _fq.attach (pipe_);
    else
        _anonymous_pipes.insert (pipe_);
}

int slk::router_t::xsetsockopt (int option_,
                                const void *optval_,
                                size_t optvallen_)
{
    const bool is_int = (optvallen_ == sizeof (int));
    int value = 0;
    if (is_int)
        memcpy (&value, optval_, sizeof (int));

    switch (option_) {
        case SL_ROUTER_RAW:
            if (is_int && value >= 0) {
                _raw_socket = (value != 0);
                if (_raw_socket) {
                    options.recv_routing_id = false;
                    options.raw_socket = true;
                }
                return 0;
            }
            break;

        case SL_ROUTER_MANDATORY:
            if (is_int && value >= 0) {
                _mandatory = (value != 0);
                return 0;
            }
            break;

        case SL_PROBE_ROUTER:
            if (is_int && value >= 0) {
                _probe_router = (value != 0);
                return 0;
            }
            break;

        case SL_ROUTER_HANDOVER:
            if (is_int && value >= 0) {
                _handover = (value != 0);
                return 0;
            }
            break;

        case SL_ROUTER_NOTIFY:
            if (is_int && value >= 0
                && value <= (SL_NOTIFY_CONNECT | SL_NOTIFY_DISCONNECT)) {
                options.router_notify = value;
                return 0;
            }
            break;

        default:
            return routing_socket_base_t::xsetsockopt (option_, optval_,
                                                       optvallen_);
    }
    errno = EINVAL;
    return -1;
}

int slk::router_t::xgetsockopt (int option_,
                                void *optval_,
                                size_t *optvallen_)
{
    const bool is_int = (*optvallen_ == sizeof (int));
    int *value = static_cast<int *> (optval_);

    switch (option_) {
        case SL_ROUTER_RAW:
            if (is_int) {
                *value = _raw_socket ? 1 : 0;
                return 0;
            }
            break;

        case SL_ROUTER_MANDATORY:
            if (is_int) {
                *value = _mandatory ? 1 : 0;
                return 0;
            }
            break;

        case SL_PROBE_ROUTER:
            if (is_int) {
                *value = _probe_router ? 1 : 0;
                return 0;
            }
            break;

        case SL_ROUTER_HANDOVER:
            if (is_int) {
                *value = _handover ? 1 : 0;
                return 0;
            }
            break;

        default:
            return socket_base_t::xgetsockopt (option_, optval_,
                                               optvallen_);
    }
    errno = EINVAL;
    return -1;
}

void slk::router_t::xpipe_terminated (pipe_t *pipe_)
{
    if (0 == _anonymous_pipes.erase (pipe_)) {
#ifdef SL_ENABLE_MONITORING
        // Get routing ID before erasing the pipe
        const blob_t &routing_id = pipe_->get_routing_id ();

        // Notify monitoring system of disconnection (before erasing)
        if (_conn_manager && routing_id.size () > 0) {
            const int64_t now = clock_t::now_us ();
            _conn_manager->peer_disconnected (routing_id, now);
            dispatch_event (EVENT_PEER_DISCONNECTED, routing_id, now);
        }
#endif

        erase_out_pipe (pipe_);
        _fq.pipe_terminated (pipe_);
        pipe_->rollback ();
        if (pipe_ == _current_out)
            _current_out = NULL;
    }
}

void slk::router_t::xread_activated (pipe_t *pipe_)
{
    const std::set<pipe_t *>::iterator it = _anonymous_pipes.find (pipe_);
    if (it == _anonymous_pipes.end ())
        _fq.activated (pipe_);
    else {
        const bool routing_id_ok = identify_peer (pipe_, false);
        if (routing_id_ok) {
            _anonymous_pipes.erase (it);
            _fq.attach (pipe_);
        }
    }
}

int slk::router_t::xsend (msg_t *msg_)
{
    // If this is the first part of the message it's the ID of the
    // peer to send the message to
    if (!_more_out) {
        slk_assert (!_current_out);

        // If we have malformed message (prefix with no subsequent message)
        // then just silently ignore it
        if (msg_->flags () & msg_t::more) {
            _more_out = true;

            // Find the pipe associated with the routing id stored in the prefix
            // If there's no such pipe just silently ignore the message, unless
            // router_mandatory is set
            out_pipe_t *out_pipe = lookup_out_pipe (
              blob_t (static_cast<unsigned char *> (msg_->data ()),
                      msg_->size (), reference_tag_t ()));

            if (out_pipe) {
                _current_out = out_pipe->pipe;

                // Check whether pipe is closed or not
                if (!_current_out->check_write ()) {
                    // Check whether pipe is full or not
                    const bool pipe_full = !_current_out->check_hwm ();
                    out_pipe->active = false;
                    _current_out = NULL;

                    if (_mandatory) {
                        _more_out = false;
                        if (pipe_full)
                            errno = EAGAIN;
                        else
                            errno = EHOSTUNREACH;
                        return -1;
                    }
                }
            } else if (_mandatory) {
                _more_out = false;
                errno = EHOSTUNREACH;
                return -1;
            }
        }

        int rc = msg_->close ();
        errno_assert (rc == 0);
        rc = msg_->init ();
        errno_assert (rc == 0);
        return 0;
    }

    // Ignore the MORE flag for raw-sock or assert?
    if (options.raw_socket)
        msg_->reset_flags (msg_t::more);

    // Check whether this is the last part of the message
    _more_out = (msg_->flags () & msg_t::more) != 0;

    // Push the message into the pipe. If there's no out pipe, just drop it
    if (_current_out) {
        // Close the remote connection if user has asked to do so
        // by sending zero length message
        // Pending messages in the pipe will be dropped (on receiving term-ack)
        if (_raw_socket && msg_->size () == 0) {
            _current_out->terminate (false);
            int rc = msg_->close ();
            errno_assert (rc == 0);
            rc = msg_->init ();
            errno_assert (rc == 0);
            _current_out = NULL;
            return 0;
        }

        const bool ok = _current_out->write (msg_);
        if (unlikely (!ok)) {
            // Message failed to send - we must close it ourselves
            const int rc = msg_->close ();
            errno_assert (rc == 0);
            // HWM was checked before, so the pipe must be gone. Roll back
            // messages that were piped
            _current_out->rollback ();
            _current_out = NULL;
        } else {
            if (!_more_out) {
                _current_out->flush ();
                _current_out = NULL;
            }
        }
    } else {
        const int rc = msg_->close ();
        errno_assert (rc == 0);
    }

    // Detach the message from the data buffer
    const int rc = msg_->init ();
    errno_assert (rc == 0);

    return 0;
}

int slk::router_t::xrecv (msg_t *msg_)
{
    if (_prefetched) {
        if (!_routing_id_sent) {
            const int rc = msg_->move (_prefetched_id);
            errno_assert (rc == 0);
            _routing_id_sent = true;
        } else {
            const int rc = msg_->move (_prefetched_msg);
            errno_assert (rc == 0);
            _prefetched = false;
        }
        _more_in = (msg_->flags () & msg_t::more) != 0;

        if (!_more_in) {
            if (_terminate_current_in) {
                _current_in->terminate (true);
                _terminate_current_in = false;
            }
            _current_in = NULL;
            _routing_id_sent = false;
        }
        return 0;
    }

    pipe_t *pipe = NULL;
    int rc = _fq.recvpipe (msg_, &pipe);

    // It's possible that we receive peer's routing id. That happens
    // after reconnection. The current implementation assumes that
    // the peer always uses the same routing id
    while (rc == 0 && msg_->is_routing_id ())
        rc = _fq.recvpipe (msg_, &pipe);

    if (rc != 0)
        return -1;

    slk_assert (pipe != NULL);

    // If we are in the middle of reading a message, just return the next part
    if (_more_in) {
        _more_in = (msg_->flags () & msg_t::more) != 0;

        if (!_more_in) {
            if (_terminate_current_in) {
                _current_in->terminate (true);
                _terminate_current_in = false;
            }
            _current_in = NULL;
        }
    } else {
        // We are at the beginning of a message
        // Keep the message part we have in the prefetch buffer
        // and return the ID of the peer instead
        rc = _prefetched_msg.move (*msg_);
        errno_assert (rc == 0);
        _prefetched = true;
        _current_in = pipe;

        const blob_t &routing_id = pipe->get_routing_id ();

#ifdef SL_ENABLE_MONITORING
        // Check if this is a heartbeat message - handle internally
        if (heartbeat_t::is_heartbeat (msg_)) {
            process_heartbeat_message (routing_id, msg_);

            // Reset state and try to get next message
            _prefetched = false;
            _current_in = NULL;

            // Recursively call to get the next message
            return xrecv (msg_);
        }
#endif

        rc = msg_->init_size (routing_id.size ());
        errno_assert (rc == 0);
        memcpy (msg_->data (), routing_id.data (), routing_id.size ());
        msg_->set_flags (msg_t::more);
        if (_prefetched_msg.metadata ())
            msg_->set_metadata (_prefetched_msg.metadata ());
        _routing_id_sent = true;
    }

    return 0;
}

int slk::router_t::rollback ()
{
    if (_current_out) {
        _current_out->rollback ();
        _current_out = NULL;
        _more_out = false;
    }
    return 0;
}

bool slk::router_t::xhas_in ()
{
    // If we are in the middle of reading the messages, there are
    // definitely more parts available
    if (_more_in)
        return true;

    // We may already have a message pre-fetched
    if (_prefetched)
        return true;

    // Try to read the next message
    // The message, if read, is kept in the pre-fetch buffer
    pipe_t *pipe = NULL;
    int rc = _fq.recvpipe (&_prefetched_msg, &pipe);

    // It's possible that we receive peer's routing id. That happens
    // after reconnection. The current implementation assumes that
    // the peer always uses the same routing id
    while (rc == 0 && _prefetched_msg.is_routing_id ())
        rc = _fq.recvpipe (&_prefetched_msg, &pipe);

    if (rc != 0)
        return false;

    slk_assert (pipe != NULL);

    const blob_t &routing_id = pipe->get_routing_id ();
    rc = _prefetched_id.init_size (routing_id.size ());
    errno_assert (rc == 0);
    memcpy (_prefetched_id.data (), routing_id.data (), routing_id.size ());
    _prefetched_id.set_flags (msg_t::more);
    if (_prefetched_msg.metadata ())
        _prefetched_id.set_metadata (_prefetched_msg.metadata ());

    _prefetched = true;
    _routing_id_sent = false;
    _current_in = pipe;

    return true;
}

static bool check_pipe_hwm (const slk::pipe_t &pipe_)
{
    return pipe_.check_hwm ();
}

bool slk::router_t::xhas_out ()
{
    // In theory, ROUTER socket is always ready for writing (except when
    // MANDATORY is set). Whether actual attempt to write succeeds depends
    // on which pipe the message is going to be routed to

    if (!_mandatory)
        return true;

    return any_of_out_pipes (check_pipe_hwm);
}

int slk::router_t::get_peer_state (const void *routing_id_,
                                   size_t routing_id_size_) const
{
    int res = 0;

    const blob_t routing_id_blob (
      static_cast<unsigned char *> (const_cast<void *> (routing_id_)),
      routing_id_size_, reference_tag_t ());
    const out_pipe_t *out_pipe = lookup_out_pipe (routing_id_blob);
    if (!out_pipe) {
        errno = EHOSTUNREACH;
        return -1;
    }

    if (out_pipe->pipe->check_hwm ())
        res |= SL_POLLOUT;

    return res;
}

bool slk::router_t::identify_peer (pipe_t *pipe_, bool locally_initiated_)
{
    msg_t msg;
    blob_t routing_id;

    SL_DEBUG_LOG("DEBUG: router identify_peer called, locally_initiated=%d, raw_socket=%d\n",
            locally_initiated_, options.raw_socket);

    if (locally_initiated_ && connect_routing_id_is_set ()) {
        const std::string connect_routing_id = extract_connect_routing_id ();
        routing_id.set (
          reinterpret_cast<const unsigned char *> (connect_routing_id.c_str ()),
          connect_routing_id.length ());
        // Not allowed to duplicate an existing rid
        slk_assert (!has_out_pipe (routing_id));
        SL_DEBUG_LOG("DEBUG: router identify_peer: using connect_routing_id\n");
    } else if (
      options
        .raw_socket) { // Always assign an integral routing id for raw-socket
        unsigned char buf[5];
        buf[0] = 0;
        put_uint32 (buf + 1, _next_integral_routing_id++);
        routing_id.set (buf, sizeof buf);
        SL_DEBUG_LOG("DEBUG: router identify_peer: using integral routing_id (raw_socket)\n");
    } else if (!options.raw_socket) {
        // Pick up handshake cases and also case where next integral routing id is set
        SL_DEBUG_LOG("DEBUG: router identify_peer: trying to read routing_id from pipe\n");
        msg.init ();
        const bool ok = pipe_->read (&msg);
        SL_DEBUG_LOG("DEBUG: router identify_peer: pipe_->read() returned %d\n", ok);
        if (!ok)
            return false;

        if (msg.size () == 0) {
            // Fall back on the auto-generation
            unsigned char buf[5];
            buf[0] = 0;
            put_uint32 (buf + 1, _next_integral_routing_id++);
            routing_id.set (buf, sizeof buf);
            msg.close ();
        } else {
            routing_id.set (static_cast<unsigned char *> (msg.data ()),
                            msg.size ());
            msg.close ();

            // Try to remove an existing routing id entry to allow the new
            // connection to take the routing id
            const out_pipe_t *const existing_outpipe =
              lookup_out_pipe (routing_id);

            if (existing_outpipe) {
                if (!_handover)
                    // Ignore peers with duplicate ID
                    return false;

                // We will allow the new connection to take over this
                // routing id. Temporarily assign a new routing id to the
                // existing pipe so we can terminate it asynchronously
                unsigned char buf[5];
                buf[0] = 0;
                put_uint32 (buf + 1, _next_integral_routing_id++);
                blob_t new_routing_id (buf, sizeof buf);

                pipe_t *const old_pipe = existing_outpipe->pipe;

                erase_out_pipe (old_pipe);
                old_pipe->set_router_socket_routing_id (new_routing_id);
                add_out_pipe (SL_MOVE (new_routing_id), old_pipe);

                if (old_pipe == _current_in)
                    _terminate_current_in = true;
                else
                    old_pipe->terminate (true);
            }
        }
    }

    pipe_->set_router_socket_routing_id (routing_id);
    add_out_pipe (SL_MOVE (routing_id), pipe_);

#ifdef SL_ENABLE_MONITORING
    // Notify monitoring system of new connection
    if (_conn_manager) {
        const int64_t now = clock_t::now_us ();
        _conn_manager->peer_connected (routing_id, now);
        dispatch_event (EVENT_PEER_CONNECTED, routing_id, now);
    }
#endif

    return true;
}

#ifdef SL_ENABLE_MONITORING
// Monitoring API implementations

bool slk::router_t::is_peer_connected (const blob_t &routing_id) const
{
    if (_conn_manager)
        return _conn_manager->is_connected (routing_id);
    return false;
}

bool slk::router_t::get_peer_statistics (const blob_t &routing_id,
                                          peer_stats_t *stats) const
{
    if (_conn_manager && stats)
        return _conn_manager->get_stats (routing_id, stats);
    return false;
}

void slk::router_t::get_connected_peers (std::vector<blob_t> *peers) const
{
    if (_conn_manager && peers)
        _conn_manager->get_connected_peers (peers);
}

void slk::router_t::set_monitor_callback (monitor_callback_fn callback,
                                           void *user_data, int event_mask)
{
    if (_event_dispatcher)
        _event_dispatcher->register_callback (callback, user_data, event_mask);
}

int slk::router_t::send_ping (const blob_t &routing_id)
{
    // Create PING message
    msg_t ping_msg;
    const int64_t now = clock_t::now_us ();

    if (!heartbeat_t::create_ping (&ping_msg, now)) {
        errno = ENOMEM;
        return -1;
    }

    // Create routing frame
    msg_t routing_frame;
    int rc = routing_frame.init_size (routing_id.size ());
    if (rc != 0) {
        ping_msg.close ();
        return -1;
    }
    memcpy (routing_frame.data (), routing_id.data (), routing_id.size ());
    routing_frame.set_flags (msg_t::more);

    // Send routing frame
    rc = xsend (&routing_frame);
    if (rc != 0) {
        ping_msg.close ();
        return -1;
    }

    // Send PING message
    rc = xsend (&ping_msg);
    if (rc != 0)
        return -1;

    // Mark ping as sent
    if (_conn_manager)
        _conn_manager->mark_ping_sent (routing_id, now);

    return 0;
}

void slk::router_t::process_heartbeat_message (const blob_t &routing_id,
                                                msg_t *msg)
{
    if (!msg)
        return;

    const int64_t now = clock_t::now_us ();

    if (heartbeat_t::is_ping (msg)) {
        // Respond with PONG
        const int64_t ping_timestamp =
            heartbeat_t::extract_ping_timestamp (msg);

        msg_t pong_msg;
        if (heartbeat_t::create_pong (&pong_msg, ping_timestamp)) {
            // Create routing frame
            msg_t routing_frame;
            int rc = routing_frame.init_size (routing_id.size ());
            if (rc == 0) {
                memcpy (routing_frame.data (), routing_id.data (),
                        routing_id.size ());
                routing_frame.set_flags (msg_t::more);

                // Send PONG response
                xsend (&routing_frame);
                xsend (&pong_msg);
            } else {
                pong_msg.close ();
            }
        }

        // Record heartbeat received
        if (_conn_manager)
            _conn_manager->record_heartbeat (routing_id, now);

    } else if (heartbeat_t::is_pong (msg)) {
        // PONG received - update RTT
        if (_conn_manager)
            _conn_manager->mark_pong_received (routing_id, now);
    }
}

void slk::router_t::dispatch_event (event_type_t type,
                                     const blob_t &routing_id,
                                     int64_t timestamp_us)
{
    if (_event_dispatcher && _event_dispatcher->is_enabled ()) {
        event_data_t event (type, routing_id, timestamp_us);
        _event_dispatcher->dispatch_event (this, event);
    }
}

void slk::router_t::record_send_stats (const blob_t &routing_id, size_t size)
{
    if (_conn_manager) {
        const int64_t now = clock_t::now_us ();
        _conn_manager->record_send (routing_id, size, now);
    }
}

void slk::router_t::record_recv_stats (const blob_t &routing_id, size_t size)
{
    if (_conn_manager) {
        const int64_t now = clock_t::now_us ();
        _conn_manager->record_recv (routing_id, size, now);
    }
}
#endif // SL_ENABLE_MONITORING

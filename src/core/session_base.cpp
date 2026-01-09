/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - 1:1 Parity with session_base.cpp */

#include "../precompiled.hpp"
#include "session_base.hpp"
#include "i_engine.hpp"
#include "../pipe/pipe.hpp"
#include "../util/err.hpp"
#include "../util/likely.hpp"
#include "../msg/msg.hpp"
#include "../core/options.hpp"
#include "../auth/mechanism.hpp"

namespace slk {

session_base_t::session_base_t (class io_thread_t *io_thread_, bool active_, class socket_base_t *socket_, const options_t &options_, address_t *addr_) :
    own_t (io_thread_, options_), _active (active_), _pipe (NULL), _zap_pipe (NULL), _incomplete_in (false), _pending (false),
    _engine (NULL), _socket (socket_), _addr (addr_), _has_linger_timer (false)
{
}

session_base_t::~session_base_t () {
    slk_assert (!_pipe);
    if (_addr) delete _addr;
}

void session_base_t::attach_pipe (pipe_t *pipe_) {
    slk_assert (!is_terminating ());
    slk_assert (!_pipe);
    slk_assert (pipe_);
    _pipe = pipe_;
    _pipe->set_event_sink (this);
}

int session_base_t::pull_msg (msg_t *msg_) {
    if (!_pipe || !_pipe->read (msg_)) {
        errno = EAGAIN;
        return -1;
    }
    _incomplete_in = (msg_->flags () & msg_t::more) != 0;
    return 0;
}

int session_base_t::push_msg (msg_t *msg_) {
    if ((msg_->flags () & msg_t::command) && !msg_->is_subscribe () && !msg_->is_cancel ())
        return 0;
    if (_pipe && _pipe->write (msg_)) {
        int rc = msg_->init ();
        errno_assert (rc == 0);
        return 0;
    }
    errno = EAGAIN;
    return -1;
}

void session_base_t::flush () {
    if (_pipe) _pipe->flush ();
}

void session_base_t::clean_pipes () {
    slk_assert (_pipe != NULL);
    _pipe->rollback ();
    _pipe->flush ();
    while (_incomplete_in) {
        msg_t msg;
        int rc = msg.init ();
        errno_assert (rc == 0);
        rc = pull_msg (&msg);
        errno_assert (rc == 0);
        rc = msg.close ();
        errno_assert (rc == 0);
    }
}

void session_base_t::read_activated (pipe_t *pipe_) {
    if (unlikely (pipe_ != _pipe)) {
        slk_assert (_terminating_pipes.count (pipe_) == 1);
        return;
    }
    if (unlikely (_engine == NULL)) {
        if (_pipe) _pipe->check_read ();
        return;
    }
    _engine->restart_output ();
}

void session_base_t::write_activated (pipe_t *pipe_) {
    slk_assert (pipe_ == _pipe);
    if (_engine) _engine->restart_input ();
}

void session_base_t::engine_error (bool handshaked_, i_engine::error_reason_t reason_) {
    (void)handshaked_; // Unused
    switch (reason_) {
        case i_engine::timeout_error:
        case i_engine::connection_error:
            if (_active) { reconnect (); break; }
        case i_engine::protocol_error:
            if (_pending) { if (_pipe) _pipe->terminate (false); }
            else terminate ();
            break;
    }
    if (_pipe) _pipe->check_read ();
}

void session_base_t::reconnect () {
    if (_pipe && options.immediate == 1) {
        _pipe->hiccup ();
        _pipe->terminate (false);
        _terminating_pipes.insert (_pipe);
        _pipe = NULL;
    }
    start_connecting (true);
}

void session_base_t::terminate () {
    unplug ();
    own_t::terminate ();
}

void session_base_t::unplug () {
    if (_engine) {
        _engine->terminate ();
        _engine = NULL;
    }
}

} // namespace slk
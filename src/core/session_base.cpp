/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - Simplified for ROUTER socket only */

#include "../precompiled.hpp"
#include "../util/macros.hpp"
#include "session_base.hpp"
#include "i_engine.hpp"
#include "../util/err.hpp"
#include "../pipe/pipe.hpp"
#include "../util/likely.hpp"
#include "../transport/tcp_connecter.hpp"
#if defined SL_HAVE_IPC
#include "../transport/ipc_connecter.hpp"
#endif
#include "../transport/address.hpp"

#include "ctx.hpp"

slk::session_base_t *slk::session_base_t::create (class io_thread_t *io_thread_,
                                                  bool active_,
                                                  class socket_base_t *socket_,
                                                  const options_t &options_,
                                                  address_t *addr_)
{
    // ServerLink only supports ROUTER socket
    session_base_t *s = NULL;
    s = new (std::nothrow)
      session_base_t (io_thread_, active_, socket_, options_, addr_);
    alloc_assert (s);
    return s;
}

slk::session_base_t::session_base_t (class io_thread_t *io_thread_,
                                     bool active_,
                                     class socket_base_t *socket_,
                                     const options_t &options_,
                                     address_t *addr_) :
    own_t (io_thread_, options_),
    io_object_t (io_thread_),
    _active (active_),
    _pipe (NULL),
    _incomplete_in (false),
    _pending (false),
    _engine (NULL),
    _socket (socket_),
    _io_thread (io_thread_),
    _has_linger_timer (false),
    _addr (addr_)
{
}

const slk::endpoint_uri_pair_t &slk::session_base_t::get_endpoint () const
{
    return _engine->get_endpoint ();
}

slk::session_base_t::~session_base_t ()
{
    slk_assert (!_pipe);

    //  If there's still a pending linger timer, remove it.
    if (_has_linger_timer) {
        cancel_timer (linger_timer_id);
        _has_linger_timer = false;
    }

    //  Close the engine.
    if (_engine)
        _engine->terminate ();

    SL_DELETE (_addr);
}

void slk::session_base_t::attach_pipe (pipe_t *pipe_)
{
    slk_assert (!is_terminating ());
    slk_assert (!_pipe);
    slk_assert (pipe_);
    _pipe = pipe_;
    _pipe->set_event_sink (this);
}

int slk::session_base_t::pull_msg (msg_t *msg_)
{
    if (!_pipe || !_pipe->read (msg_)) {
        errno = EAGAIN;
        return -1;
    }

    _incomplete_in = (msg_->flags () & msg_t::more) != 0;

    return 0;
}

int slk::session_base_t::push_msg (msg_t *msg_)
{
    //  pass subscribe/cancel to the sockets
    if ((msg_->flags () & msg_t::command) && !msg_->is_subscribe ()
        && !msg_->is_cancel ())
        return 0;
    if (_pipe && _pipe->write (msg_)) {
        const int rc = msg_->init ();
        errno_assert (rc == 0);
        return 0;
    }

    errno = EAGAIN;
    return -1;
}

void slk::session_base_t::reset ()
{
}

void slk::session_base_t::flush ()
{
    SL_DEBUG_LOG("DEBUG: session flush called, _pipe=%p\n", (void*)_pipe);
    if (_pipe) {
        SL_DEBUG_LOG("DEBUG: session flush: calling _pipe->flush()\n");
        _pipe->flush ();
        SL_DEBUG_LOG("DEBUG: session flush: _pipe->flush() returned\n");
    }
}

void slk::session_base_t::rollback ()
{
    if (_pipe)
        _pipe->rollback ();
}

void slk::session_base_t::clean_pipes ()
{
    slk_assert (_pipe != NULL);

    //  Get rid of half-processed messages in the out pipe. Flush any
    //  unflushed messages upstream.
    _pipe->rollback ();
    _pipe->flush ();

    //  Remove any half-read message from the in pipe.
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

void slk::session_base_t::pipe_terminated (pipe_t *pipe_)
{
    // Drop the reference to the deallocated pipe if required.
    slk_assert (pipe_ == _pipe || _terminating_pipes.count (pipe_) == 1);

    if (pipe_ == _pipe) {
        // If this is our current pipe, remove it
        _pipe = NULL;
        if (_has_linger_timer) {
            cancel_timer (linger_timer_id);
            _has_linger_timer = false;
        }
    } else
        // Remove the pipe from the detached pipes set
        _terminating_pipes.erase (pipe_);

    //  If we are waiting for pending messages to be sent, at this point
    //  we are sure that there will be no more messages and we can proceed
    //  with termination safely.
    if (_pending && !_pipe && _terminating_pipes.empty ()) {
        _pending = false;
        own_t::process_term (0);
    }
}

void slk::session_base_t::read_activated (pipe_t *pipe_)
{
    SL_DEBUG_LOG("DEBUG: session read_activated called\n");
    // Skip activating if we're detaching this pipe
    if (unlikely (pipe_ != _pipe)) {
        slk_assert (_terminating_pipes.count (pipe_) == 1);
        return;
    }

    if (unlikely (_engine == NULL)) {
        if (_pipe) {
            SL_DEBUG_LOG("DEBUG: session read_activated: calling _pipe->check_read()\n");
            _pipe->check_read ();
        }
        return;
    }

    if (likely (pipe_ == _pipe)) {
        SL_DEBUG_LOG("DEBUG: session read_activated: calling _engine->restart_output()\n");
        _engine->restart_output ();
    }
}

void slk::session_base_t::write_activated (pipe_t *pipe_)
{
    SL_DEBUG_LOG("DEBUG: session write_activated called\n");
    // Skip activating if we're detaching this pipe
    if (_pipe != pipe_) {
        slk_assert (_terminating_pipes.count (pipe_) == 1);
        return;
    }

    if (_engine) {
        SL_DEBUG_LOG("DEBUG: session write_activated: calling engine->restart_input\n");
        _engine->restart_input ();
    }
}

void slk::session_base_t::hiccuped (pipe_t *)
{
    //  Hiccups are always sent from session to socket, not the other
    //  way round.
    slk_assert (false);
}

slk::socket_base_t *slk::session_base_t::get_socket () const
{
    return _socket;
}

void slk::session_base_t::process_plug ()
{
    if (_active)
        start_connecting (false);
}

void slk::session_base_t::process_attach (i_engine *engine_)
{
    slk_assert (engine_ != NULL);
    slk_assert (!_engine);
    _engine = engine_;

    if (!engine_->has_handshake_stage ())
        engine_ready ();

    //  Plug in the engine.
    _engine->plug (_io_thread, this);
}

void slk::session_base_t::engine_ready ()
{
    //  Create the pipe if it does not exist yet.
    if (!_pipe && !is_terminating ()) {
        object_t *parents[2] = {this, _socket};
        pipe_t *pipes[2] = {NULL, NULL};

        int hwms[2] = {options.rcvhwm, options.sndhwm};
        bool conflates[2] = {false, false};
        const int rc = pipepair (parents, pipes, hwms, conflates);
        errno_assert (rc == 0);

        //  Plug the local end of the pipe.
        pipes[0]->set_event_sink (this);

        //  Initialize the pipe's ypipe by calling check_read() once.
        //  This ensures the ypipe marks the reader as sleeping (_c = nullptr).
        pipes[0]->check_read ();

        //  Remember the local end of the pipe.
        slk_assert (!_pipe);
        _pipe = pipes[0];

        //  The endpoints strings are not set on bind, set them here so that
        //  events can use them.
        pipes[0]->set_endpoint_pair (_engine->get_endpoint ());
        pipes[1]->set_endpoint_pair (_engine->get_endpoint ());

        //  DON'T call check_read() on pipes[1] here!
        //  If we do, it will set _in_active=false because there's no data yet.
        //  Instead, let attach_pipe() in the socket handle initialization.
        //  The pipe starts with _in_active=true from the constructor.
        SL_DEBUG_LOG("DEBUG: engine_ready: session thread=%u, socket thread=%u, pipes[1]=%p, pipes[1] thread=%u\n",
                get_tid(), _socket->get_tid(), (void*)pipes[1], pipes[1]->get_tid());

        //  Ask socket to plug into the remote end of the pipe.
        //  This adds the pipe to the socket's pipes list and sets the event sink.
        send_bind (_socket, pipes[1]);
    }
}

void slk::session_base_t::engine_error (bool handshaked_,
                                        slk::i_engine::error_reason_t reason_)
{
    //  Engine is dead. Let's forget about it.
    _engine = NULL;

    //  Remove any half-done messages from the pipes.
    if (_pipe)
        clean_pipes ();

    slk_assert (reason_ == i_engine::connection_error
                || reason_ == i_engine::timeout_error
                || reason_ == i_engine::protocol_error);

    // Suppress unused parameter warning
    (void)handshaked_;

    switch (reason_) {
        case i_engine::timeout_error:
        case i_engine::connection_error:
            if (_active) {
                reconnect ();
                break;
            }
            // FALLTHROUGH - fall through for passive connections

        case i_engine::protocol_error:
            if (_pending) {
                if (_pipe)
                    _pipe->terminate (false);
            } else {
                terminate ();
            }
            break;
    }

    //  Just in case there's only a delimiter in the pipe.
    if (_pipe)
        _pipe->check_read ();
}

void slk::session_base_t::process_term (int linger_)
{
    slk_assert (!_pending);

    //  If the termination of the pipe happens before the term command is
    //  delivered there's nothing much to do. We can proceed with the
    //  standard termination immediately.
    if (!_pipe && _terminating_pipes.empty ()) {
        own_t::process_term (0);
        return;
    }

    _pending = true;

    if (_pipe != NULL) {
        //  If there's finite linger value, delay the termination.
        //  If linger is infinite (negative) we don't even have to set
        //  the timer.
        if (linger_ > 0) {
            slk_assert (!_has_linger_timer);
            add_timer (linger_, linger_timer_id);
            _has_linger_timer = true;
        }

        //  Start pipe termination process. Delay the termination till all messages
        //  are processed in case the linger time is non-zero.
        _pipe->terminate (linger_ != 0);

        //  In case there's no engine and there's only delimiter in the
        //  pipe it wouldn't be ever read. Thus we check for it explicitly.
        if (!_engine)
            _pipe->check_read ();
    }
}

void slk::session_base_t::timer_event (int id_)
{
    //  Linger period expired. We can proceed with termination even though
    //  there are still pending messages to be sent.
    slk_assert (id_ == linger_timer_id);
    _has_linger_timer = false;

    //  Ask pipe to terminate even though there may be pending messages in it.
    slk_assert (_pipe);
    _pipe->terminate (false);
}

void slk::session_base_t::reconnect ()
{
    //  For delayed connect situations, terminate the pipe
    //  and reestablish later on
    if (_pipe && options.immediate == 1) {
        _pipe->hiccup ();
        _pipe->terminate (false);
        _terminating_pipes.insert (_pipe);
        _pipe = NULL;

        if (_has_linger_timer) {
            cancel_timer (linger_timer_id);
            _has_linger_timer = false;
        }
    }

    reset ();

    //  Reconnect.
    if (options.reconnect_ivl > 0)
        start_connecting (true);
}

void slk::session_base_t::start_connecting (bool wait_)
{
    slk_assert (_active);

    //  Choose I/O thread to run connecter in. Given that we are already
    //  running in an I/O thread, there must be at least one available.
    io_thread_t *io_thread = choose_io_thread (options.affinity);
    slk_assert (io_thread);

    //  Create the connecter object.
    own_t *connecter = NULL;
    if (_addr->protocol == protocol_name::tcp) {
        connecter = new (std::nothrow)
          tcp_connecter_t (io_thread, this, options, _addr, wait_);
    }
#if defined SL_HAVE_IPC
    else if (_addr->protocol == protocol_name::ipc) {
        connecter = new (std::nothrow)
          ipc_connecter_t (io_thread, this, options, _addr, wait_);
    }
#endif

    if (connecter != NULL) {
        alloc_assert (connecter);
        launch_child (connecter);
        return;
    }

    slk_assert (false);
}

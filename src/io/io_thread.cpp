/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "../precompiled.hpp"
#include "io_thread.hpp"
#include "../core/ctx.hpp"
#include "poller.hpp"
#include "mailbox.hpp"
#include <cstdio>

#ifdef SL_USE_ASIO
#include <asio.hpp>
#endif

slk::io_thread_t::io_thread_t (slk::ctx_t *ctx_, uint32_t tid_) :
    object_t (ctx_, tid_),
    _mailbox_handle (static_cast<poller_t::handle_t> (NULL))
{
    _poller = new (std::nothrow) poller_t (ctx_);
    alloc_assert (_poller);

    _mailbox_handle = _poller->add_fd (_mailbox.get_fd (), this);
    _poller->set_pollin (_mailbox_handle);
}

slk::io_thread_t::~io_thread_t () 
{
    SL_DELETE (_poller);
}

void slk::io_thread_t::start () 
{
    //  Start the underlying I/O thread.
    _poller->start ();
}

void slk::io_thread_t::stop () 
{
    send_stop ();
}

slk::mailbox_t *slk::io_thread_t::get_mailbox () 
{
    return &_mailbox;
}

int slk::io_thread_t::get_load () const 
{
    return _poller->get_load ();
}

slk::poller_t *slk::io_thread_t::get_poller () const 
{
    return _poller;
}

#ifdef SL_USE_ASIO
asio::io_context& slk::io_thread_t::get_io_context()
{
    return _poller->get_context();
}
#endif

void slk::io_thread_t::process_stop () 
{
    _poller->rm_fd (_mailbox_handle);
    _poller->stop ();
}

void slk::io_thread_t::in_event () 
{
    //  TODO: Do we want to limit number of commands processed at once?
    while (true) {
        command_t cmd;
        const int rc = _mailbox.recv (&cmd, 0);

        //  There is no command waiting for execution.
        if (rc == -1 && errno == EAGAIN)
            break;

        //  If there was an error return from poll() wait for a while
        //  and retry.
        if (rc == -1 && errno == EINTR)
            continue;

        //  Process the command.
        cmd.destination->process_command (cmd);
    }
}

void slk::io_thread_t::out_event () 
{
    slk_assert (false);
}

void slk::io_thread_t::timer_event (int)
{
    slk_assert (false);
}

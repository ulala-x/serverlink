/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "io_thread.hpp"
#include "../util/err.hpp"
#include "../core/ctx.hpp"
#include <new>
#include <stdio.h>

#ifdef SL_USE_IOCP
#include "iocp.hpp"
#endif

slk::io_thread_t::io_thread_t (ctx_t *ctx_, uint32_t tid_) :
    object_t (ctx_, tid_),
    _mailbox_handle (static_cast<poller_t::handle_t> (NULL))
{
    _poller = new (std::nothrow) poller_t (ctx_);
    alloc_assert (_poller);

#ifdef SL_USE_IOCP
    // For IOCP, configure mailbox signaler to use PostQueuedCompletionStatus
    // for wakeup instead of socket-based signaling
    fprintf(stderr, "[io_thread] IOCP mode: configuring mailbox signaler\n");
    iocp_t *iocp_poller = static_cast<iocp_t *> (_poller);
    fprintf(stderr, "[io_thread] iocp_poller=%p, this=%p\n", iocp_poller, this);

    // Enable IOCP mode in mailbox
    fprintf(stderr, "[io_thread] Calling _mailbox.set_iocp_mode(true)\n");
    _mailbox.set_iocp_mode (true);

    signaler_t *signaler = _mailbox.get_signaler ();
    fprintf(stderr, "[io_thread] _mailbox.get_signaler() returned %p\n", signaler);
    if (signaler) {
        fprintf(stderr, "[io_thread] Calling signaler->set_iocp()\n");
        signaler->set_iocp (iocp_poller);
        fprintf(stderr, "[io_thread] signaler->set_iocp() completed\n");
    } else {
        fprintf(stderr, "[io_thread] WARNING: get_signaler() returned NULL!\n");
    }

    // Register this io_thread as the mailbox handler for SIGNALER_KEY events
    fprintf(stderr, "[io_thread] Calling iocp_poller->set_mailbox_handler(this)\n");
    iocp_poller->set_mailbox_handler (this);

    // Don't register mailbox fd with IOCP - we use PostQueuedCompletionStatus instead
    // However, we still need to increment load count for the mailbox
    // This matches the behavior of select/epoll/kqueue which increment load in add_fd()
    fprintf(stderr, "[io_thread] Calling adjust_mailbox_load(1)\n");
    iocp_poller->adjust_mailbox_load (1);
    fprintf(stderr, "[io_thread] IOCP mailbox configuration complete\n");
#else
    // For non-IOCP pollers (epoll, kqueue, select), register mailbox fd
    if (_mailbox.get_fd () != retired_fd) {
        _mailbox_handle = _poller->add_fd (_mailbox.get_fd (), this);
        _poller->set_pollin (_mailbox_handle);
    }
#endif
}

slk::io_thread_t::~io_thread_t ()
{
    delete _poller;
}

void slk::io_thread_t::start ()
{
    char name[16] = "";
    snprintf (name, sizeof (name), "IO/%u",
              get_tid () - slk::ctx_t::reaper_tid - 1);
    // Start the underlying I/O thread
    _poller->start (name);
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

void slk::io_thread_t::in_event ()
{
    // TODO: Do we want to limit number of commands I/O thread can
    // process in a single go?

    command_t cmd;
    int rc = _mailbox.recv (&cmd, 0);

    while (rc == 0 || errno == EINTR) {
        if (rc == 0)
            cmd.destination->process_command (cmd);
        rc = _mailbox.recv (&cmd, 0);
    }

    errno_assert (rc != 0 && errno == EAGAIN);
}

void slk::io_thread_t::out_event ()
{
    // We are never polling for POLLOUT here. This function is never called
    slk_assert (false);
}

void slk::io_thread_t::timer_event (int)
{
    // No timers here. This function is never called
    slk_assert (false);
}

slk::poller_t *slk::io_thread_t::get_poller () const
{
    slk_assert (_poller);
    return _poller;
}

void slk::io_thread_t::process_stop ()
{
    fprintf(stderr, "[io_thread::process_stop] ENTER: this=%p\n", this);

#ifdef SL_USE_IOCP
    // For IOCP, we don't register mailbox fd, so nothing to remove
    // However, we need to decrement load count to match the increment in constructor
    fprintf(stderr, "[io_thread::process_stop] IOCP mode: adjusting mailbox load -1\n");
    iocp_t *iocp_poller = static_cast<iocp_t *> (_poller);
    iocp_poller->adjust_mailbox_load (-1);
    fprintf(stderr, "[io_thread::process_stop] Load after adjustment: %d\n", _poller->get_load());
#else
    slk_assert (_mailbox_handle);
    _poller->rm_fd (_mailbox_handle);
#endif

    fprintf(stderr, "[io_thread::process_stop] Calling _poller->stop()\n");
    _poller->stop ();
    fprintf(stderr, "[io_thread::process_stop] EXIT\n");
}

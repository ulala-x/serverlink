/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "reaper.hpp"
#include "../core/socket_base.hpp"
#include "../util/err.hpp"
#include <new>

#ifdef SL_USE_IOCP
#include "iocp.hpp"
#endif

slk::reaper_t::reaper_t (class ctx_t *ctx_, uint32_t tid_) :
    object_t (ctx_, tid_),
    _mailbox_handle (static_cast<poller_t::handle_t> (NULL)),
    _poller (NULL),
    _sockets (0),
    _terminating (false)
{
    if (!_mailbox.valid ())
        return;

    _poller = new (std::nothrow) poller_t (ctx_);
    alloc_assert (_poller);

#ifdef SL_USE_IOCP
    // For IOCP, configure mailbox signaler to use PostQueuedCompletionStatus
    // for wakeup instead of socket-based signaling (same as io_thread)
    fprintf(stderr, "[reaper] IOCP mode: configuring mailbox signaler\n");
    iocp_t *iocp_poller = static_cast<iocp_t *> (_poller);
    fprintf(stderr, "[reaper] iocp_poller=%p, this=%p\n", iocp_poller, this);

    signaler_t *signaler = _mailbox.get_signaler ();
    fprintf(stderr, "[reaper] _mailbox.get_signaler() returned %p\n", signaler);
    if (signaler) {
        fprintf(stderr, "[reaper] Calling signaler->set_iocp()\n");
        signaler->set_iocp (iocp_poller);
        fprintf(stderr, "[reaper] signaler->set_iocp() completed\n");
    } else {
        fprintf(stderr, "[reaper] WARNING: get_signaler() returned NULL!\n");
    }

    // Enable IOCP mode on mailbox for optimized recv()
    fprintf(stderr, "[reaper] Calling _mailbox.set_iocp_mode(true)\n");
    _mailbox.set_iocp_mode (true);

    // Register this reaper as the mailbox handler for SIGNALER_KEY events
    fprintf(stderr, "[reaper] Calling iocp_poller->set_mailbox_handler(this)\n");
    iocp_poller->set_mailbox_handler (this);

    // Don't register mailbox fd with IOCP - we use PostQueuedCompletionStatus instead
    // However, we still need to increment load count for the mailbox
    // This matches the behavior of select/epoll/kqueue which increment load in add_fd()
    fprintf(stderr, "[reaper] Calling adjust_mailbox_load(1)\n");
    iocp_poller->adjust_mailbox_load (1);
    fprintf(stderr, "[reaper] IOCP mailbox configuration complete\n");
#else
    // For non-IOCP pollers (epoll, kqueue, select), register mailbox fd
    if (_mailbox.get_fd () != retired_fd) {
        _mailbox_handle = _poller->add_fd (_mailbox.get_fd (), this);
        _poller->set_pollin (_mailbox_handle);
    }
#endif
}

slk::reaper_t::~reaper_t ()
{
    delete _poller;
}

slk::mailbox_t *slk::reaper_t::get_mailbox ()
{
    return &_mailbox;
}

void slk::reaper_t::start ()
{
    slk_assert (_mailbox.valid ());

    // Start the thread
    _poller->start ("Reaper");
}

void slk::reaper_t::stop ()
{
    if (get_mailbox ()->valid ()) {
        send_stop ();
    }
}

void slk::reaper_t::in_event ()
{
    fprintf(stderr, "[reaper_t::in_event] ENTER\n");
    int cmd_count = 0;
    while (true) {
        // Get the next command. If there is none, exit
        command_t cmd;
        const int rc = _mailbox.recv (&cmd, 0);
        if (rc != 0 && errno == EINTR)
            continue;
        if (rc != 0 && errno == EAGAIN) {
            fprintf(stderr, "[reaper_t::in_event] No more commands (EAGAIN), processed %d commands\n", cmd_count);
            break;
        }
        errno_assert (rc == 0);

        // Process the command
        cmd_count++;
        fprintf(stderr, "[reaper_t::in_event] Processing command %d, type=%d\n", cmd_count, (int)cmd.type);
        cmd.destination->process_command (cmd);
    }
    fprintf(stderr, "[reaper_t::in_event] EXIT\n");
}

void slk::reaper_t::out_event ()
{
    slk_assert (false);
}

void slk::reaper_t::timer_event (int)
{
    slk_assert (false);
}

void slk::reaper_t::process_stop ()
{
    fprintf(stderr, "[reaper::process_stop] ENTER: _sockets=%d, _terminating=%d\n", _sockets, _terminating);

    _terminating = true;

    // If there are no sockets pending, finish immediately
    if (_sockets == 0) {
        fprintf(stderr, "[reaper::process_stop] No sockets pending - calling send_done()\n");
        send_done ();
#ifdef SL_USE_IOCP
        // For IOCP, we don't register mailbox fd, so nothing to remove
        // However, we need to decrement load count to match the increment in constructor
        fprintf(stderr, "[reaper::process_stop] IOCP mode: adjusting mailbox load -1\n");
        iocp_t *iocp_poller = static_cast<iocp_t *> (_poller);
        iocp_poller->adjust_mailbox_load (-1);
        fprintf(stderr, "[reaper::process_stop] Load after adjustment: %d\n", _poller->get_load());
#else
        _poller->rm_fd (_mailbox_handle);
#endif
        fprintf(stderr, "[reaper::process_stop] Calling _poller->stop()\n");
        _poller->stop ();
        fprintf(stderr, "[reaper::process_stop] EXIT (stopped)\n");
    } else {
        fprintf(stderr, "[reaper::process_stop] EXIT (waiting for %d sockets)\n", _sockets);
    }
}

void slk::reaper_t::process_reap (socket_base_t *socket_)
{
    //  Add the socket to the poller
    socket_->start_reaping (_poller);

    ++_sockets;
}

void slk::reaper_t::process_reaped ()
{
    --_sockets;

    // If reaped was already asked to terminate and there are no more sockets,
    // finish immediately
    if (!_sockets && _terminating) {
        send_done ();
#ifdef SL_USE_IOCP
        // For IOCP, we don't register mailbox fd, so nothing to remove
        // However, we need to decrement load count to match the increment in constructor
        iocp_t *iocp_poller = static_cast<iocp_t *> (_poller);
        iocp_poller->adjust_mailbox_load (-1);
#else
        _poller->rm_fd (_mailbox_handle);
#endif
        _poller->stop ();
    }
}

/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "reaper.hpp"
#include "../core/socket_base.hpp"
#include "../util/err.hpp"
#include <new>

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

    if (_mailbox.get_fd () != retired_fd) {
        _mailbox_handle = _poller->add_fd (_mailbox.get_fd (), this);
        _poller->set_pollin (_mailbox_handle);
    }
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
    while (true) {
        // Get the next command. If there is none, exit
        command_t cmd;
        const int rc = _mailbox.recv (&cmd, 0);
        if (rc != 0 && errno == EINTR)
            continue;
        if (rc != 0 && errno == EAGAIN)
            break;
        errno_assert (rc == 0);

        // Process the command
        cmd.destination->process_command (cmd);
    }
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
    _terminating = true;

    // FIXME: Since socket reaping is not fully implemented yet (process_reap is a stub),
    // we always send done immediately. This allows context destruction to complete.
    // In the full implementation, we should only send done when _sockets == 0.
    send_done ();
    _poller->rm_fd (_mailbox_handle);
    _poller->stop ();
}

void slk::reaper_t::process_reap (socket_base_t *socket_)
{
    // Stub for Phase 5 - socket doesn't have start_reaping yet
    (void)socket_;
    ++_sockets;
}

void slk::reaper_t::process_reaped ()
{
    --_sockets;

    // If reaped was already asked to terminate and there are no more sockets,
    // finish immediately
    if (!_sockets && _terminating) {
        send_done ();
        _poller->rm_fd (_mailbox_handle);
        _poller->stop ();
    }
}

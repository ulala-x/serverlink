/* SPDX-License-Identifier: MPL-2.0 */

#include "mailbox.hpp"
#include "../util/err.hpp"

namespace slk
{
mailbox_t::mailbox_t ()
{
    // Get the pipe into passive state. That way, if the user starts by
    // polling on the associated file descriptor it will get woken up when
    // new command is posted.
    const bool ok = _cpipe.check_read ();
    slk_assert (!ok);
    _active = false;
}

mailbox_t::~mailbox_t ()
{
    // TODO: Retrieve and deallocate commands inside the _cpipe.

    // Work around problem that other threads might still be in our
    // send() method, by waiting on the mutex before disappearing.
    _sync.lock ();
    _sync.unlock ();
}

fd_t mailbox_t::get_fd () const
{
    return _signaler.get_fd ();
}

void mailbox_t::send (const command_t &cmd_)
{
    _sync.lock ();
    _cpipe.write (cmd_, false);
    const bool ok = _cpipe.flush ();
    _sync.unlock ();
    if (!ok)
        _signaler.send ();
}

int mailbox_t::recv (command_t *cmd_, int timeout_)
{
    // Try to get the command straight away
    if (_active) {
        if (_cpipe.read (cmd_))
            return 0;

        // If there are no more commands available, switch into passive state
        _active = false;
    }

    // Wait for signal from the command sender
    int rc = _signaler.wait (timeout_);
    if (rc == -1) {
        errno_assert (errno == EAGAIN || errno == EINTR);
        return -1;
    }

    // Receive the signal
    rc = _signaler.recv_failable ();
    if (rc == -1) {
        errno_assert (errno == EAGAIN);
        return -1;
    }

    // Switch into active state
    _active = true;

    // Get a command
    const bool ok = _cpipe.read (cmd_);
    slk_assert (ok);
    return 0;
}

bool mailbox_t::has_pending () const
{
    // Check if we're in active state (already have commands)
    // or if there are commands in the pipe
    return _active || _cpipe.check_read ();
}

bool mailbox_t::valid () const
{
    return _signaler.valid ();
}

} // namespace slk

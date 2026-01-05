/* SPDX-License-Identifier: MPL-2.0 */

#include "mailbox.hpp"
#include "../util/err.hpp"

namespace slk
{
mailbox_t::mailbox_t ()
#ifdef SL_USE_IOCP
    : _iocp_mode (false)
#endif
{
    fprintf(stderr, "[mailbox_t] Constructor starting: this=%p\n", this);

    fprintf(stderr, "[mailbox_t] Initializing signaler\n");
    // Get the pipe into passive state. That way, if the user starts by
    // polling on the associated file descriptor it will get woken up when
    // new command is posted.
    const bool ok = _cpipe.check_read ();
    slk_assert (!ok);
    _active = false;

    fprintf(stderr, "[mailbox_t] Constructor completed: this=%p, fd=%d\n",
            this, (int)_signaler.get_fd());
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
    if (!ok) {
        fprintf(stderr, "[mailbox_t::send] Pipe flush returned false - calling signaler.send()\n");
        _signaler.send ();
        fprintf(stderr, "[mailbox_t::send] signaler.send() completed\n");
    } else {
        fprintf(stderr, "[mailbox_t::send] Pipe flush returned true - NOT calling signaler.send()\n");
    }
}

int mailbox_t::recv (command_t *cmd_, int timeout_)
{
#ifdef SL_USE_IOCP
    // IOCP mode with timeout=0: in_event() call itself represents signal reception
    // Skip signaler wait/recv and directly read from command pipe
    if (_iocp_mode && timeout_ == 0) {
        fprintf(stderr, "[mailbox_t::recv] IOCP mode: skipping signaler, directly reading cpipe\n");
        _active = true;
        if (_cpipe.read (cmd_)) {
            fprintf(stderr, "[mailbox_t::recv] IOCP mode: command read successfully\n");
            return 0;
        }
        _active = false;
        fprintf(stderr, "[mailbox_t::recv] IOCP mode: no commands available (EAGAIN)\n");
        errno = EAGAIN;
        return -1;
    }
#endif

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

bool mailbox_t::valid () const
{
    return _signaler.valid ();
}

signaler_t *mailbox_t::get_signaler ()
{
    return &_signaler;
}

} // namespace slk

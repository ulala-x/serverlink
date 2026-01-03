/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_MAILBOX_HPP_INCLUDED
#define SERVERLINK_MAILBOX_HPP_INCLUDED

#include <stddef.h>

#include "signaler.hpp"
#include "fd.hpp"
#include "i_mailbox.hpp"
#include "../util/config.hpp"
#include "../pipe/command.hpp"
#include "../util/ypipe.hpp"
#include "../util/mutex.hpp"
#include "../util/macros.hpp"

namespace slk
{
class mailbox_t final : public i_mailbox
{
  public:
    mailbox_t ();
    ~mailbox_t ();

    fd_t get_fd () const;
    void send (const command_t &cmd_);
    int recv (command_t *cmd_, int timeout_);

    bool valid () const;

#ifdef HAVE_FORK
    // Close file descriptors in forked child process
    void forked () final
    {
        _signaler.forked ();
    }
#endif

  private:
    // Pipe to store actual commands
    typedef ypipe_t<command_t, command_pipe_granularity> cpipe_t;
    cpipe_t _cpipe;

    // Signaler to pass signals from writer thread to reader thread
    signaler_t _signaler;

    // Synchronization mutex (there's only one reader but multiple writers)
    mutex_t _sync;

    // True if the underlying pipe is active (we are allowed to read commands)
    bool _active;

    SL_NON_COPYABLE_NOR_MOVABLE (mailbox_t)
};
}

#endif

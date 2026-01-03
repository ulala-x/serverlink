/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_MAILBOX_SAFE_HPP_INCLUDED
#define SERVERLINK_MAILBOX_SAFE_HPP_INCLUDED

#include <vector>
#include <stddef.h>

#include "signaler.hpp"
#include "fd.hpp"
#include "i_mailbox.hpp"
#include "../util/config.hpp"
#include "../pipe/command.hpp"
#include "../util/ypipe.hpp"
#include "../util/mutex.hpp"
#include "../util/condition_variable.hpp"
#include "../util/macros.hpp"

namespace slk
{
class mailbox_safe_t final : public i_mailbox
{
  public:
    mailbox_safe_t (mutex_t *sync_);
    ~mailbox_safe_t ();

    void send (const command_t &cmd_);
    int recv (command_t *cmd_, int timeout_);

    // Add signaler to mailbox which will be called when a message is ready
    void add_signaler (signaler_t *signaler_);
    void remove_signaler (signaler_t *signaler_);
    void clear_signalers ();

#ifdef HAVE_FORK
    // Close file descriptors in forked child process
    void forked () final
    {
        // TODO: call fork on the condition variable
    }
#endif

  private:
    // Pipe to store actual commands
    typedef ypipe_t<command_t, command_pipe_granularity> cpipe_t;
    cpipe_t _cpipe;

    // Condition variable to pass signals from writer thread to reader thread
    condition_variable_t _cond_var;

    // Synchronize access to the mailbox from receivers and senders
    mutex_t *const _sync;

    // Signalers to notify when message is ready
    std::vector<slk::signaler_t *> _signalers;

    SL_NON_COPYABLE_NOR_MOVABLE (mailbox_safe_t)
};
}

#endif

/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_I_MAILBOX_HPP_INCLUDED
#define SERVERLINK_I_MAILBOX_HPP_INCLUDED

#include "../util/macros.hpp"
#include "../pipe/command.hpp"

namespace slk
{
// Interface to be implemented by mailbox

class i_mailbox
{
  public:
    virtual ~i_mailbox () SL_DEFAULT;

    virtual void send (const command_t &cmd_) = 0;
    virtual int recv (command_t *cmd_, int timeout_) = 0;

#ifdef HAVE_FORK
    // Close file descriptors in the signaller (for forked child processes)
    virtual void forked () = 0;
#endif
};
}

#endif

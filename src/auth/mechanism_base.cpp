/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "../precompiled.hpp"

#include "mechanism_base.hpp"
#include "../core/session_base.hpp"
#include "../msg/msg.hpp"
#include "../util/err.hpp"

slk::mechanism_base_t::mechanism_base_t (session_base_t *const session_,
                                         const options_t &options_) :
    mechanism_t (options_), session (session_)
{
}

int slk::mechanism_base_t::check_basic_command_structure (msg_t *msg_) const
{
    if (msg_->size () <= 1
        || msg_->size () <= (static_cast<uint8_t *> (msg_->data ()))[0]) {
        // Malformed command
        errno = EPROTO;
        return -1;
    }
    return 0;
}

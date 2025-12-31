/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_MECHANISM_BASE_HPP_INCLUDED
#define SL_MECHANISM_BASE_HPP_INCLUDED

#include "mechanism.hpp"

namespace slk
{
class msg_t;
class session_base_t;

class mechanism_base_t : public mechanism_t
{
  protected:
    mechanism_base_t (session_base_t *session_, const options_t &options_);

    session_base_t *const session;

    int check_basic_command_structure (msg_t *msg_) const;
};
}

#endif

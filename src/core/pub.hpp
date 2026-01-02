/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_PUB_HPP_INCLUDED
#define SL_PUB_HPP_INCLUDED

#include "xpub.hpp"

namespace slk
{
class ctx_t;
class msg_t;
class pipe_t;

class pub_t final : public xpub_t
{
  public:
    pub_t (ctx_t *parent_, uint32_t tid_, int sid_);
    ~pub_t () override;

    // Implementations of virtual functions from socket_base_t
    void xattach_pipe (pipe_t *pipe_,
                       bool subscribe_to_all_ = false,
                       bool locally_initiated_ = false) override;
    int xrecv (msg_t *msg_) override;
    bool xhas_in () override;

    SL_NON_COPYABLE_NOR_MOVABLE (pub_t)
};
}

#endif

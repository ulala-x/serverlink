/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_PAIR_HPP_INCLUDED
#define SL_PAIR_HPP_INCLUDED

#include "socket_base.hpp"

namespace slk
{
class ctx_t;
class msg_t;
class pipe_t;

class pair_t SL_FINAL : public socket_base_t
{
  public:
    pair_t (ctx_t *parent_, uint32_t tid_, int sid_);
    ~pair_t () SL_OVERRIDE;

    // Overrides of virtual functions from socket_base_t
    void xattach_pipe (pipe_t *pipe_,
                       bool subscribe_to_all_ = false,
                       bool locally_initiated_ = false) SL_OVERRIDE;
    int xsend (msg_t *msg_) SL_OVERRIDE;
    int xrecv (msg_t *msg_) SL_OVERRIDE;
    bool xhas_in () SL_OVERRIDE;
    bool xhas_out () SL_OVERRIDE;
    void xread_activated (pipe_t *pipe_) SL_OVERRIDE;
    void xwrite_activated (pipe_t *pipe_) SL_OVERRIDE;
    void xpipe_terminated (pipe_t *pipe_) SL_OVERRIDE;

  private:
    // The pipe to the peer. PAIR can only connect to a single peer.
    pipe_t *_pipe;

    SL_NON_COPYABLE_NOR_MOVABLE (pair_t)
};
}

#endif

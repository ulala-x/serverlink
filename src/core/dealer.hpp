/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_DEALER_HPP_INCLUDED
#define SL_DEALER_HPP_INCLUDED

#include "socket_base.hpp"
#include "../pipe/fq.hpp"
#include "../pipe/lb.hpp"

namespace slk
{
class ctx_t;
class pipe_t;
class msg_t;

class dealer_t : public socket_base_t
{
  public:
    dealer_t (slk::ctx_t *parent_, uint32_t tid_, int sid_);
    ~dealer_t () override;

    // socket_base_t virtual overrides
    void xattach_pipe (slk::pipe_t *pipe_,
                       bool subscribe_to_all_,
                       bool locally_initiated_) override;
    int xsend (slk::msg_t *msg_) override;
    int xrecv (slk::msg_t *msg_) override;
    bool xhas_in () override;
    bool xhas_out () override;
    void xread_activated (slk::pipe_t *pipe_) override;
    void xwrite_activated (slk::pipe_t *pipe_) override;
    void xpipe_terminated (slk::pipe_t *pipe_) override;

  private:
    // Fair-queue for incoming messages
    fq_t _fq;

    // Load-balancer for outgoing messages
    lb_t _lb;

    SL_NON_COPYABLE_NOR_MOVABLE (dealer_t)
};
}

#endif

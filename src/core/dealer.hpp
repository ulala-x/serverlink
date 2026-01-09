/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_DEALER_HPP_INCLUDED
#define SL_DEALER_HPP_INCLUDED

#include "socket_base.hpp"
#include "session_base.hpp"
#include "../pipe/fq.hpp"
#include "../pipe/lb.hpp"

namespace slk
{
class ctx_t;
class msg_t;
class pipe_t;

class dealer_t : public socket_base_t
{
  public:
    dealer_t (slk::ctx_t *parent_, uint32_t tid_, int sid_);
    ~dealer_t () override;

  protected:
    //  Overrides of functions from socket_base_t.
    void xattach_pipe (slk::pipe_t *pipe_,
                       bool subscribe_to_all_,
                       bool locally_initiated_) override;
    int xsetsockopt (int option_,
                     const void *optval_,
                     size_t optvallen_) override;
    int xsend (slk::msg_t *msg_) override;
    int xrecv (slk::msg_t *msg_) override;
    bool xhas_in () override;
    bool xhas_out () override;
    void xread_activated (slk::pipe_t *pipe_) override;
    void xwrite_activated (slk::pipe_t *pipe_) override;
    void xpipe_terminated (slk::pipe_t *pipe_) override;

    //  Send and recv - knowing which pipe was used.
    int sendpipe (slk::msg_t *msg_, slk::pipe_t **pipe_);
    int recvpipe (slk::msg_t *msg_, slk::pipe_t **pipe_);

  private:
    //  Messages are fair-queued from inbound pipes. And load-balanced to
    //  the outbound pipes.
    fq_t _fq;
    lb_t _lb;

    // if true, send an empty message to every connected router peer
    bool _probe_router;

    SL_NON_COPYABLE_NOR_MOVABLE (dealer_t)
};
}

#endif

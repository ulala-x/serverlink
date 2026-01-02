/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_XSUB_HPP_INCLUDED
#define SL_XSUB_HPP_INCLUDED

#include "socket_base.hpp"
#include "session_base.hpp"
#include "../pipe/dist.hpp"
#include "../pipe/fq.hpp"
#include "../pipe/trie.hpp"
#include "../pattern/pattern_trie.hpp"

namespace slk
{
class ctx_t;
class pipe_t;

class xsub_t : public socket_base_t
{
  public:
    xsub_t (ctx_t *parent_, uint32_t tid_, int sid_);
    ~xsub_t () SL_OVERRIDE;

  protected:
    // Overrides of functions from socket_base_t
    void xattach_pipe (pipe_t *pipe_,
                       bool subscribe_to_all_,
                       bool locally_initiated_) SL_FINAL;
    int xsetsockopt (int option_,
                     const void *optval_,
                     size_t optvallen_) SL_OVERRIDE;
    int xgetsockopt (int option_, void *optval_, size_t *optvallen_) SL_FINAL;
    int xsend (msg_t *msg_) SL_OVERRIDE;
    bool xhas_out () SL_OVERRIDE;
    int xrecv (msg_t *msg_) SL_FINAL;
    bool xhas_in () SL_FINAL;
    void xread_activated (pipe_t *pipe_) SL_FINAL;
    void xwrite_activated (pipe_t *pipe_) SL_FINAL;
    void xhiccuped (pipe_t *pipe_) SL_FINAL;
    void xpipe_terminated (pipe_t *pipe_) SL_FINAL;

  private:
    // Check whether the message matches at least one subscription
    bool match (msg_t *msg_);

    // Function to be applied to the trie to send all the subscriptions
    // upstream
    static void
    send_subscription (unsigned char *data_, size_t size_, void *arg_);

    // Fair queueing object for inbound pipes
    fq_t _fq;

    // Object for distributing the subscriptions upstream
    dist_t _dist;

    // The repository of subscriptions
    trie_with_size_t _subscriptions;

    // The repository of pattern subscriptions (glob patterns)
    pattern_trie_t _pattern_subscriptions;

    // If true, send all unsubscription messages upstream, not just
    // unique ones
    bool _verbose_unsubs;

    // If true, 'message' contains a matching message to return on the
    // next recv call
    bool _has_message;
    msg_t _message;

    // If true, part of a multipart message was already sent, but
    // there are following parts still waiting
    bool _more_send;

    // If true, part of a multipart message was already received, but
    // there are following parts still waiting
    bool _more_recv;

    // If true, subscribe and cancel messages are processed for the rest
    // of multipart message
    bool _process_subscribe;

    // This option is enabled with SLK_ONLY_FIRST_SUBSCRIBE.
    // If true, messages following subscribe/unsubscribe in a multipart
    // message are treated as user data regardless of the first byte.
    bool _only_first_subscribe;

    SL_NON_COPYABLE_NOR_MOVABLE (xsub_t)
};
}

#endif

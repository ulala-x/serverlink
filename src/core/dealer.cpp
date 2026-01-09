/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "../precompiled.hpp"
#include "dealer.hpp"
#include <serverlink/serverlink.h>
#include "../util/err.hpp"
#include "../msg/msg.hpp"
#include "../pipe/pipe.hpp"

slk::dealer_t::dealer_t (slk::ctx_t *parent_, uint32_t tid_, int sid_) :
    socket_base_t (parent_, tid_, sid_, false)
{
    options.type = SLK_DEALER;
}

slk::dealer_t::~dealer_t ()
{
}

void slk::dealer_t::xattach_pipe (slk::pipe_t *pipe_,
                                  bool subscribe_to_all_,
                                  bool locally_initiated_)
{
    SL_UNUSED (subscribe_to_all_);
    SL_UNUSED (locally_initiated_);
    slk_assert (pipe_);

    // Send the routing ID to the peer so they know who we are.
    send_routing_id (pipe_, options);
    pipe_->flush ();

    _fq.attach (pipe_);
    _lb.attach (pipe_);
}

void slk::dealer_t::xread_activated (slk::pipe_t *pipe_)
{
    _fq.activated (pipe_);
}

void slk::dealer_t::xwrite_activated (slk::pipe_t *pipe_)
{
    _lb.activated (pipe_);
}

void slk::dealer_t::xpipe_terminated (slk::pipe_t *pipe_)
{
    _fq.pipe_terminated (pipe_);
    _lb.pipe_terminated (pipe_);
}

int slk::dealer_t::xsend (slk::msg_t *msg_)
{
    return _lb.send (msg_);
}

int slk::dealer_t::xrecv (slk::msg_t *msg_)
{
    return _fq.recv (msg_);
}

bool slk::dealer_t::xhas_in ()
{
    return _fq.has_in ();
}

bool slk::dealer_t::xhas_out ()
{
    return _lb.has_out ();
}

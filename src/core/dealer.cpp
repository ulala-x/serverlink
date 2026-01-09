/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "../precompiled.hpp"
#include "dealer.hpp"
#include <serverlink/serverlink.h>
#include "../util/err.hpp"
#include "../msg/msg.hpp"
#include "../pipe/pipe.hpp"
#include "../util/constants.hpp"

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
    // DEALER sockets must always send an identity frame (even if empty).
    msg_t routing_id_msg;
    int rc = routing_id_msg.init_size (options.routing_id_size);
    errno_assert (rc == 0);
    if (options.routing_id_size > 0) {
        memcpy (routing_id_msg.data (), options.routing_id, options.routing_id_size);
    }
    routing_id_msg.set_flags (msg_t::routing_id);
    const bool ok = pipe_->write (&routing_id_msg);
    slk_assert (ok);
    pipe_->flush ();
    rc = routing_id_msg.close ();
    errno_assert (rc == 0);

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

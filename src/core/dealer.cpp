/* SPDX-License-Identifier: MPL-2.0 */

#include "../precompiled.hpp"
#include "../util/macros.hpp"
#include "dealer.hpp"
#include "../util/err.hpp"
#include "../msg/msg.hpp"
#include "../pipe/pipe.hpp"

slk::dealer_t::dealer_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    socket_base_t (parent_, tid_, sid_), _probe_router (false)
{
    options.type = SL_DEALER;
    options.can_send_hello_msg = true;
    options.can_recv_hiccup_msg = true;
}

slk::dealer_t::~dealer_t ()
{
}

void slk::dealer_t::xattach_pipe (pipe_t *pipe_,
                                  bool subscribe_to_all_,
                                  bool locally_initiated_)
{
    SL_UNUSED (subscribe_to_all_);
    SL_UNUSED (locally_initiated_);

    slk_assert (pipe_);

    if (_probe_router) {
        msg_t probe_msg;
        int rc = probe_msg.init ();
        errno_assert (rc == 0);

        // rc is ignored here, since it is not a bug if write fails.
        pipe_->write (&probe_msg);
        pipe_->flush ();

        rc = probe_msg.close ();
        errno_assert (rc == 0);
    }

    _fq.attach (pipe_);
    _lb.attach (pipe_);
}

int slk::dealer_t::xsetsockopt (int option_,
                                const void *optval_,
                                size_t optvallen_)
{
    const bool is_int = (optvallen_ == sizeof (int));
    int value = 0;
    if (is_int)
        memcpy (&value, optval_, sizeof (int));

    switch (option_) {
        case SL_PROBE_ROUTER:
            if (is_int && value >= 0) {
                _probe_router = (value != 0);
                return 0;
            }
            break;

        default:
            break;
    }

    errno = EINVAL;
    return -1;
}

int slk::dealer_t::xsend (msg_t *msg_)
{
    return sendpipe (msg_, NULL);
}

int slk::dealer_t::xrecv (msg_t *msg_)
{
    return recvpipe (msg_, NULL);
}

bool slk::dealer_t::xhas_in ()
{
    return _fq.has_in ();
}

bool slk::dealer_t::xhas_out ()
{
    return _lb.has_out ();
}

void slk::dealer_t::xread_activated (pipe_t *pipe_)
{
    _fq.activated (pipe_);
}

void slk::dealer_t::xwrite_activated (pipe_t *pipe_)
{
    _lb.activated (pipe_);
}

void slk::dealer_t::xpipe_terminated (pipe_t *pipe_)
{
    _fq.pipe_terminated (pipe_);
    _lb.pipe_terminated (pipe_);
}

int slk::dealer_t::sendpipe (msg_t *msg_, pipe_t **pipe_)
{
    return _lb.sendpipe (msg_, pipe_);
}

int slk::dealer_t::recvpipe (msg_t *msg_, pipe_t **pipe_)
{
    return _fq.recvpipe (msg_, pipe_);
}
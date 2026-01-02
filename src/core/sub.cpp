/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "precompiled.hpp"
#include "sub.hpp"
#include "../msg/msg.hpp"
#include "../util/err.hpp"
#include "../util/constants.hpp"

slk::sub_t::sub_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    xsub_t (parent_, tid_, sid_)
{
    options.type = SL_SUB;

    // Switch filtering messages on (as opposed to XSUB where the
    // filtering is off by default)
    options.filter = true;
}

slk::sub_t::~sub_t ()
{
}

int slk::sub_t::xsetsockopt (int option_,
                             const void *optval_,
                             size_t optvallen_)
{
    if (option_ != SL_SUBSCRIBE && option_ != SL_UNSUBSCRIBE
        && option_ != SL_PSUBSCRIBE && option_ != SL_PUNSUBSCRIBE) {
        errno = EINVAL;
        return -1;
    }

    // Pattern subscriptions are handled directly by xsub_t
    if (option_ == SL_PSUBSCRIBE || option_ == SL_PUNSUBSCRIBE) {
        return xsub_t::xsetsockopt (option_, optval_, optvallen_);
    }

    // Create the subscription message
    msg_t msg;
    int rc;
    const unsigned char *data = static_cast<const unsigned char *> (optval_);

    if (option_ == SL_SUBSCRIBE) {
        rc = msg.init_subscribe (optvallen_, data);
    } else {
        rc = msg.init_cancel (optvallen_, data);
    }
    errno_assert (rc == 0);

    // Pass it further on in the stack
    rc = xsub_t::xsend (&msg);
    return close_and_return (&msg, rc);
}

int slk::sub_t::xsend (msg_t *)
{
    // Override the XSUB's send - SUB sockets cannot send user messages
    errno = ENOTSUP;
    return -1;
}

bool slk::sub_t::xhas_out ()
{
    // Override the XSUB's send - SUB sockets cannot send user messages
    return false;
}

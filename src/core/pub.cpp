/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "precompiled.hpp"
#include "pub.hpp"
#include "../pipe/pipe.hpp"
#include "../util/err.hpp"
#include "../msg/msg.hpp"
#include "../util/constants.hpp"

slk::pub_t::pub_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    xpub_t (parent_, tid_, sid_)
{
    options.type = SL_PUB;
}

slk::pub_t::~pub_t ()
{
}

void slk::pub_t::xattach_pipe (pipe_t *pipe_,
                               bool subscribe_to_all_,
                               bool locally_initiated_)
{
    slk_assert (pipe_);

    // Don't delay pipe termination as there is no one
    // to receive the delimiter
    pipe_->set_nodelay ();

    xpub_t::xattach_pipe (pipe_, subscribe_to_all_, locally_initiated_);
}

int slk::pub_t::xrecv (class msg_t *)
{
    // Messages cannot be received from PUB socket
    errno = ENOTSUP;
    return -1;
}

bool slk::pub_t::xhas_in ()
{
    return false;
}

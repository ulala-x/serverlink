/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "../precompiled.hpp"
#include "pair.hpp"
#include "../pipe/pipe.hpp"
#include "../util/err.hpp"
#include "../msg/msg.hpp"
#include "../util/constants.hpp"

slk::pair_t::pair_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    socket_base_t (parent_, tid_, sid_, false),
    _pipe (NULL)
{
    options.type = SL_PAIR;
}

slk::pair_t::~pair_t ()
{
    slk_assert (!_pipe);
}

void slk::pair_t::xattach_pipe (pipe_t *pipe_,
                                bool subscribe_to_all_,
                                bool locally_initiated_)
{
    SL_UNUSED (subscribe_to_all_);
    SL_UNUSED (locally_initiated_);

    slk_assert (pipe_);

    // PAIR socket only supports a single peer connection.
    if (_pipe) {
        pipe_->terminate (false);
        return;
    }

    _pipe = pipe_;
}

void slk::pair_t::xpipe_terminated (pipe_t *pipe_)
{
    if (pipe_ == _pipe)
        _pipe = NULL;
}

void slk::pair_t::xread_activated (pipe_t *pipe_)
{
    // PAIR socket doesn't need to do anything here,
    // socket_base handles it.
    (void)pipe_;
}

void slk::pair_t::xwrite_activated (pipe_t *pipe_)
{
    // PAIR socket doesn't need to do anything here,
    // socket_base handles it.
    (void)pipe_;
}

int slk::pair_t::xsend (msg_t *msg_)
{
    if (!_pipe || !_pipe->write (msg_)) {
        errno = EAGAIN;
        return -1;
    }

    if (!(msg_->flags () & msg_t::more))
        _pipe->flush ();

    // Standard libzmq: detach the original message from the data buffer.
    const int rc = msg_->init ();
    errno_assert (rc == 0);

    return 0;
}

int slk::pair_t::xrecv (msg_t *msg_)
{
    // Deallocate old content of the message.
    int rc = msg_->close ();
    errno_assert (rc == 0);

    if (!_pipe || !_pipe->read (msg_)) {
        rc = msg_->init ();
        errno_assert (rc == 0);
        errno = EAGAIN;
        return -1;
    }
    return 0;
}

bool slk::pair_t::xhas_in ()
{
    if (!_pipe)
        return false;

    return _pipe->check_read ();
}

bool slk::pair_t::xhas_out ()
{
    if (!_pipe)
        return false;

    return _pipe->check_write ();
}

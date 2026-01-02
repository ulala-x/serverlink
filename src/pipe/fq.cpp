/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "fq.hpp"
#include "pipe.hpp"
#include "../util/err.hpp"
#include "../msg/msg.hpp"

slk::fq_t::fq_t () : _active (0), _current (0), _more (false)
{
}

slk::fq_t::~fq_t ()
{
    slk_assert (_pipes.empty ());
}

void slk::fq_t::attach (pipe_t *pipe_)
{
    _pipes.push_back (pipe_);
    _pipes.swap (_active, _pipes.size () - 1);
    _active++;
}

void slk::fq_t::pipe_terminated (pipe_t *pipe_)
{
    const pipes_t::size_type index = _pipes.index (pipe_);

    //  Remove the pipe from the list; adjust number of active pipes
    //  accordingly.
    if (index < _active) {
        _active--;
        _pipes.swap (index, _active);
        if (_current == _active)
            _current = 0;
    }
    _pipes.erase (pipe_);
}

void slk::fq_t::activated (pipe_t *pipe_)
{
    //  Move the pipe to the list of active pipes.
    _pipes.swap (_pipes.index (pipe_), _active);
    _active++;
}

int slk::fq_t::recv (msg_t *msg_)
{
    return recvpipe (msg_, NULL);
}

int slk::fq_t::recvpipe (msg_t *msg_, pipe_t **pipe_)
{
    //  Deallocate old content of the message.
    int rc = msg_->close ();
    errno_assert (rc == 0);

    //  Round-robin over the pipes to get the next message.
    while (_active > 0) SL_LIKELY_ATTR {
        //  Try to fetch new message. If we've already read part of the message
        //  subsequent part should be immediately available.
        const bool fetched = _pipes[_current]->read (msg_);

        //  Note that when message is not fetched, current pipe is deactivated
        //  and replaced by another active pipe. Thus we don't have to increase
        //  the 'current' pointer.
        if (fetched) SL_LIKELY_ATTR {
            if (pipe_)
                *pipe_ = _pipes[_current];
            _more = (msg_->flags () & msg_t::more) != 0;
            if (!_more) SL_LIKELY_ATTR {
                _current = (_current + 1) % _active;
            }
            return 0;
        }

        //  Check the atomicity of the message.
        //  If we've already received the first part of the message
        //  we should get the remaining parts without blocking.
        slk_assert (!_more);

        _active--;
        _pipes.swap (_current, _active);
        if (_current == _active)
            _current = 0;
    }

    //  No message is available. Initialise the output parameter
    //  to be a 0-byte message.
    rc = msg_->init ();
    errno_assert (rc == 0);
    errno = EAGAIN;
    return -1;
}

bool slk::fq_t::has_in ()
{
    //  There are subsequent parts of the partly-read message available.
    if (_more) SL_UNLIKELY_ATTR
        return true;

    //  Note that messing with current doesn't break the fairness of fair
    //  queueing algorithm. If there are no messages available current will
    //  get back to its original value. Otherwise it'll point to the first
    //  pipe holding messages, skipping only pipes with no messages available.
    while (_active > 0) SL_LIKELY_ATTR {
        if (_pipes[_current]->check_read ()) SL_LIKELY_ATTR
            return true;

        //  Deactivate the pipe.
        _active--;
        _pipes.swap (_current, _active);
        if (_current == _active)
            _current = 0;
    }

    return false;
}

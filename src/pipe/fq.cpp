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
    activated (pipe_);
}

void slk::fq_t::pipe_terminated (pipe_t *pipe_)
{
    const pipes_t::size_type index = _pipes.index (pipe_);

    if (index < _active) {
        _active--;
        _pipes.swap (index, _active);
        if (_current > _active || (_current == _active && _current > 0))
            _current--;
    }
    _pipes.erase (pipe_);
}

void slk::fq_t::activated (pipe_t *pipe_)
{
    const pipes_t::size_type index = _pipes.index (pipe_);
    if (index >= _active) {
        _pipes.swap (index, _active);
        _active++;
    }
}

int slk::fq_t::recv (msg_t *msg_)
{
    return recvpipe (msg_, NULL);
}

int slk::fq_t::recvpipe (msg_t *msg_, pipe_t **pipe_)
{
    int rc = msg_->close ();
    errno_assert (rc == 0);

    while (_active > 0) {
        if (_pipes [_current]->read (msg_)) {
            if (pipe_)
                *pipe_ = _pipes [_current];
            _more = (msg_->flags () & msg_t::more) != 0;
            if (!_more) {
                _current = (_current + 1) % _active;
            }
            return 0;
        }

        _active--;
        _pipes.swap (_current, _active);
        if (_current == _active)
            _current = 0;
    }

    rc = msg_->init ();
    errno_assert (rc == 0);
    errno = EAGAIN;
    return -1;
}

bool slk::fq_t::has_in ()
{
    if (_more)
        return true;

    while (_active > 0) {
        if (_pipes [_current]->check_read ())
            return true;

        _active--;
        _pipes.swap (_current, _active);
        if (_current == _active)
            _current = 0;
    }

    return false;
}
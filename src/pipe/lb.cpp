/* SPDX-License-Identifier: MPL-2.0 */

#include "../precompiled.hpp"
#include "lb.hpp"
#include "pipe.hpp"
#include "../util/err.hpp"
#include "../msg/msg.hpp"

slk::lb_t::lb_t () : _active (0), _current (0), _more (false), _dropping (false)
{
}

slk::lb_t::~lb_t ()
{
    slk_assert (_pipes.empty ());
}

void slk::lb_t::attach (pipe_t *pipe_)
{
    _pipes.push_back (pipe_);
    activated (pipe_);
}

void slk::lb_t::pipe_terminated (pipe_t *pipe_)
{
    const pipes_t::size_type index = _pipes.index (pipe_);

    //  If we are in the middle of multipart message and current pipe
    //  have disconnected, we have to drop the remainder of the message.
    if (index == _current && _more)
        _dropping = true;

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

void slk::lb_t::activated (pipe_t *pipe_)
{
    //  Move the pipe to the list of active pipes.
    _pipes.swap (_pipes.index (pipe_), _active);
    _active++;
}

int slk::lb_t::send (msg_t *msg_)
{
    return sendpipe (msg_, NULL);
}

int slk::lb_t::sendpipe (msg_t *msg_, pipe_t **pipe_)
{
    //  Drop the message if required. If we are at the end of the message
    //  switch back to non-dropping mode.
    if (_dropping) {
        _more = (msg_->flags () & msg_t::more) != 0;
        _dropping = _more;

        int rc = msg_->close ();
        errno_assert (rc == 0);
        rc = msg_->init ();
        errno_assert (rc == 0);
        return 0;
    }

    while (_active > 0) {
        if (_pipes[_current]->write (msg_)) {
            if (pipe_)
                *pipe_ = _pipes[_current];
            _more = (msg_->flags () & msg_t::more) != 0;
            if (!_more) {
                _current++;
                if (_current >= _active)
                    _current = 0;
            }
            return 0;
        }

        // If send fails for multi-part msg rollback other parts sent earlier
        if (_more) {
            _pipes[_current]->rollback ();
            _active--;
            _pipes.swap (_current, _active);
            if (_current == _active)
                _current = 0;
            _dropping = true;
            _more = false;
            errno = EFAULT;
            return -1;
        }

        //  Deactivate the pipe.
        _active--;
        _pipes.swap (_current, _active);
        if (_current == _active)
            _current = 0;
    }

    errno = EAGAIN;
    return -1;
}

bool slk::lb_t::has_out ()
{
    if (_dropping)
        return true;

    while (_active > 0) {
        if (_pipes[_current]->check_write ())
            return true;

        //  Deactivate the pipe.
        _active--;
        _pipes.swap (_current, _active);
        if (_current == _active)
            _current = 0;
    }

    return false;
}

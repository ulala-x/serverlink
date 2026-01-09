/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - 1:1 Parity with lb.cpp */

#include "../precompiled.hpp"
#include "lb.hpp"
#include "pipe.hpp"
#include "../util/err.hpp"
#include "../msg/msg.hpp"

namespace slk {

lb_t::lb_t () : _active (0), _current (0), _more (false), _dropping (false) {}
lb_t::~lb_t () {}

void lb_t::attach (pipe_t *pipe_) {
    _pipes.push_back (pipe_);
    activated (pipe_);
}

void lb_t::pipe_terminated (pipe_t *pipe_) {
    int index = _pipes.index (pipe_);
    if (index == _current && _more) _dropping = true;
    if (index < _active) {
        _active--;
        _pipes.swap (index, _active);
        if (_current == _active) _current = 0;
    }
    _pipes.erase (pipe_);
}

void lb_t::activated (pipe_t *pipe_) {
    _pipes.swap (_pipes.index (pipe_), _active);
    _active++;
}

int lb_t::send (msg_t *msg_) { return sendpipe (msg_, NULL); }

int lb_t::sendpipe (msg_t *msg_, pipe_t **pipe_) {
    if (_dropping) {
        _more = (msg_->flags () & msg_t::more) != 0;
        _dropping = _more;
        int rc = msg_->close (); slk_assert (rc == 0);
        rc = msg_->init (); slk_assert (rc == 0);
        return 0;
    }

    while (_active > 0) {
        if (_pipes[_current]->write (msg_)) {
            if (pipe_) *pipe_ = _pipes[_current];
            _more = (msg_->flags () & msg_t::more) != 0;
            if (!_more) { _current++; if (_current >= _active) _current = 0; }
            return 0;
        }
        _active--;
        _pipes.swap (_current, _active);
        if (_current == _active) _current = 0;
    }
    errno = EAGAIN; return -1;
}

bool lb_t::has_out () { return _active > 0; }

} // namespace slk
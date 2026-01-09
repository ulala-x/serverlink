/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - 1:1 Parity with fq.cpp */

#include "../precompiled.hpp"
#include "fq.hpp"
#include "pipe.hpp"
#include "../util/err.hpp"
#include "../msg/msg.hpp"

namespace slk {

fq_t::fq_t () : _active (0), _current (0), _more (false) {}
fq_t::~fq_t () {}

void fq_t::attach (pipe_t *pipe_) {
    _pipes.push_back (pipe_);
    activated (pipe_);
}

void fq_t::pipe_terminated (pipe_t *pipe_) {
    int index = _pipes.index (pipe_);
    if (index < _active) {
        _active--;
        _pipes.swap (index, _active);
        if (_current == _active) _current = 0;
    }
    _pipes.erase (pipe_);
}

void fq_t::activated (pipe_t *pipe_) {
    _pipes.swap (_pipes.index (pipe_), _active);
    _active++;
}

int fq_t::recv (msg_t *msg_) { return recvpipe (msg_, NULL); }

int fq_t::recvpipe (msg_t *msg_, pipe_t **pipe_) {
    int rc = msg_->close (); slk_assert (rc == 0);
    while (_active > 0) {
        if (_pipes[_current]->read (msg_)) {
            if (pipe_) *pipe_ = _pipes[_current];
            _more = (msg_->flags () & msg_t::more) != 0;
            if (!_more) { _current++; if (_current >= _active) _current = 0; }
            return 0;
        }
        slk_assert (!_more);
        _active--;
        _pipes.swap (_current, _active);
        if (_current == _active) _current = 0;
    }
    errno = EAGAIN; return -1;
}

bool fq_t::has_in () { return _active > 0; }

} // namespace slk
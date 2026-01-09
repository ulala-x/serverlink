/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - 1:1 Parity with dist.cpp */

#include "../precompiled.hpp"
#include "dist.hpp"
#include "pipe.hpp"
#include "../util/err.hpp"
#include "../msg/msg.hpp"

namespace slk {

dist_t::dist_t () : _eligible (0), _active (0), _matching (0), _more (false) {}
dist_t::~dist_t () {}

void dist_t::attach (pipe_t *pipe_) {
    _pipes.push_back (pipe_);
    _eligible++;
    // libzmq parity: New pipe is not immediately active if we are in the middle of a multipart msg
    if (!_more) _active = _eligible;
}

void dist_t::pipe_terminated (pipe_t *pipe_) {
    int index = _pipes.index (pipe_);
    if (index < _matching) _matching--;
    if (index < _active) _active--;
    _eligible--;
    _pipes.swap (index, _eligible);
    _pipes.erase (pipe_);
}

void dist_t::activated (pipe_t *pipe_) {
    // Already active, nothing to do
}

int dist_t::send_to_all (msg_t *msg_) {
    _matching = _active;
    return send_to_matching (msg_);
}

int dist_t::send_to_matching (msg_t *msg_) {
    const bool msg_more = (msg_->flags () & msg_t::more) != 0;
    distribute (msg_);
    if (!msg_more) _active = _eligible;
    _more = msg_more;
    return 0;
}

void dist_t::distribute (msg_t *msg_) {
    if (_matching == 0) {
        int rc = msg_->close (); slk_assert (rc == 0);
        rc = msg_->init (); slk_assert (rc == 0);
        return;
    }

    for (int i = 0; i < _matching; i++) {
        msg_t copy;
        if (i < _matching - 1) {
            int rc = copy.copy (msg_); slk_assert (rc == 0);
        } else {
            copy = *msg_;
            msg_->init ();
        }
        if (!_pipes[i]->write (&copy)) {
            copy.close ();
            // Pipe is full, deactivate it exactly like libzmq
            _active--; _matching--;
            _pipes.swap (i, _matching);
            _pipes.swap (_matching, _active);
            i--; // Retry same index with the swapped pipe
        }
    }
}

bool dist_t::has_out () { return _active > 0; }

} // namespace slk
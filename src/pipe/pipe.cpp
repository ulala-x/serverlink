/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - 1:1 Parity with pipe.cpp */

#include "pipe.hpp"
#include "../core/options.hpp"
#include <new>
#include <stddef.h>
#include "../util/macros.hpp"
#include "../util/err.hpp"
#include "../util/ypipe.hpp"
#include "../util/ypipe_conflate.hpp"
#include "../util/likely.hpp"

namespace slk {

int pipepair (object_t *parents_[2], pipe_t *pipes_[2], const int hwms_[2], const bool conflate_[2]) {
    typedef ypipe_t<msg_t, message_pipe_granularity> upipe_normal_t;
    typedef ypipe_conflate_t<msg_t> upipe_conflate_t;

    pipe_t::upipe_t *upipe1;
    if (conflate_[0]) upipe1 = new (std::nothrow) upipe_conflate_t ();
    else upipe1 = new (std::nothrow) upipe_normal_t ();
    alloc_assert (upipe1);

    pipe_t::upipe_t *upipe2;
    if (conflate_[1]) upipe2 = new (std::nothrow) upipe_conflate_t ();
    else upipe2 = new (std::nothrow) upipe_normal_t ();
    alloc_assert (upipe2);

    pipes_[0] = new (std::nothrow) pipe_t (parents_[0], upipe1, upipe2, hwms_[1], hwms_[0], conflate_[0]);
    alloc_assert (pipes_[0]);
    pipes_[1] = new (std::nothrow) pipe_t (parents_[1], upipe2, upipe1, hwms_[0], hwms_[1], conflate_[1]);
    alloc_assert (pipes_[1]);

    pipes_[0]->set_peer (pipes_[1]);
    pipes_[1]->set_peer (pipes_[0]);
    return 0;
}

pipe_t::pipe_t (object_t *parent_, upipe_t *inpipe_, upipe_t *outpipe_, int inhwm_, int outhwm_, bool conflate_) :
    object_t (parent_), _in_pipe (inpipe_), _out_pipe (outpipe_), _in_active (true), _out_active (true),
    _hwm (outhwm_), _lwm (compute_lwm (inhwm_)), _in_hwm_boost (0), _out_hwm_boost (0),
    _msgs_read (0), _msgs_written (0), _peers_msgs_read (0), _peer (NULL), _sink (NULL),
    _state (active), _delay (true), _conflate (conflate_)
{
    _disconnect_msg.init ();
}

pipe_t::~pipe_t () {
    _disconnect_msg.close ();
    delete _in_pipe;
    if (_state != term_ack_sent) delete _out_pipe;
}

void pipe_t::set_event_sink (i_pipe_events *sink_) { _sink = sink_; }

bool pipe_t::check_read () {
    if (unlikely (!_in_active || (_state != active && _state != waiting_for_delimiter))) return false;
    if (_in_pipe->check_read ()) return true;
    _in_active = false;
    return false;
}

bool pipe_t::read (msg_t *msg_) {
    if (unlikely (!_in_active || (_state != active && _state != waiting_for_delimiter))) return false;
    while (true) {
        if (!_in_pipe->read (msg_)) { _in_active = false; return false; }
        if (unlikely (msg_->is_credential ())) {
            int rc = msg_->close ();
            slk_assert (rc == 0);
        } else break;
    }
    _msgs_read++;
    if (unlikely (_msgs_read % message_pipe_granularity == 0)) send_activate_write (_peer, _msgs_read);
    return true;
}

bool pipe_t::check_write () {
    if (unlikely (!_out_active || _state != active)) return false;
    bool theoretical = _hwm > 0 && (_msgs_written - _peers_msgs_read >= (uint64_t) (_hwm + _out_hwm_boost));
    if (unlikely (theoretical)) { _out_active = false; return false; }
    return true;
}

bool pipe_t::write (const msg_t *msg_) {
    if (unlikely (!check_write ())) return false;
    _out_pipe->write (*msg_, (msg_->flags () & msg_t::more) != 0);
    _msgs_written++;
    return true;
}

void pipe_t::rollback () const { if (_out_pipe) _out_pipe->rollback (); }

void pipe_t::flush () {
    if (_state == term_ack_sent) return;
    if (_out_pipe && !_out_pipe->flush ()) send_activate_read (_peer);
}

void pipe_t::process_activate_read () {
    if (!_in_active && (_state == active || _state == waiting_for_delimiter)) {
        _in_active = true;
        _sink->read_activated (this);
    }
}

void pipe_t::process_activate_write (uint64_t msgs_read_) {
    if (msgs_read_ > _peers_msgs_read) _peers_msgs_read = msgs_read_;
    if (!_out_active && _state == active) {
        _out_active = true;
        _sink->write_activated (this);
    }
}

void pipe_t::hiccup () {
    if (_state == active || _state == waiting_for_delimiter) send_hiccup (_peer);
}

void pipe_t::process_hiccup (void *) {
    _in_pipe->rollback ();
    _sink->hiccuped (this);
}

void pipe_t::terminate (bool delay_) {
    if (_state == term_req_sent1 || _state == term_req_sent2 || _state == term_ack_sent) return;
    _delay = delay_;
    if (_state == active) _state = term_req_sent1;
    else if (_state == delimiter_received) _state = term_req_sent2;
    send_term (_peer, 0);
}

void pipe_t::process_term (int linger_) {
    (void)linger_;
    if (_state == active) _state = waiting_for_delimiter;
    else if (_state == term_req_sent1) _state = term_req_sent2;
    else if (_state == term_req_sent2) _state = term_ack_sent;
    else if (_state == delimiter_received) _state = term_ack_sent;
    check_read ();
}

void pipe_t::set_peer (pipe_t *peer_) { _peer = peer_; }
int pipe_t::compute_lwm (int hwm_) { return (hwm_ + 1) / 2; }
void pipe_t::process_delimiter () {
    if (_state == active) _state = delimiter_received;
    else if (_state == waiting_for_delimiter) _state = term_ack_sent;
    else if (_state == term_req_sent1) _state = term_ack_sent;
    else if (_state == term_req_sent2) _state = term_ack_sent;
}

} // namespace slk

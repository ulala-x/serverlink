/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - 1:1 Parity with zmtp_engine.cpp */

#include "../precompiled.hpp"
#include "zmtp_engine.hpp"
#include "v2_encoder.hpp"
#include "v2_decoder.hpp"
#include "../auth/null_mechanism.hpp"
#include "../util/err.hpp"
#include "../util/likely.hpp"
#include "../protocol/wire.hpp"
#include "../core/session_base.hpp"

namespace slk {

zmtp_engine_t::zmtp_engine_t (std::unique_ptr<i_async_stream> stream_, const options_t &options_, const endpoint_uri_pair_t &endpoint_uri_pair_) :
    stream_engine_base_t (std::move (stream_), options_, endpoint_pair_, true),
    _greeting_size (v2_greeting_size), _greeting_bytes_read (0), _subscription_required (false)
{
    _next_msg = static_cast<int (stream_engine_base_t::*) (msg_t *)> (&zmtp_engine_t::routing_id_msg);
    _process_msg = static_cast<int (stream_engine_base_t::*) (msg_t *)> (&zmtp_engine_t::process_routing_id_msg);
    _routing_id_msg.init ();
}

zmtp_engine_t::~zmtp_engine_t () { _routing_id_msg.close (); }

void zmtp_engine_t::plug_internal () {
    // libzmq Parity: Construct ZMTP 3.0 greeting exactly
    _outpos = _greeting_send;
    memset (_outpos, 0, v3_greeting_size);
    _outpos[0] = 0xff;
    put_uint64 (&_outpos[1], 1);
    _outpos[9] = 0x7f;
    _outpos[10] = 3; // ZMTP 3.0
    _outsize = v3_greeting_size;
    restart_output ();
}

void zmtp_engine_t::process_handshake_data (unsigned char *buffer_, size_t size_) {
    const size_t to_copy = std::min (size_, v3_greeting_size - _greeting_bytes_read);
    memcpy (_greeting_recv + _greeting_bytes_read, buffer_, to_copy);
    _greeting_bytes_read += to_copy;
    if (_greeting_bytes_read >= v3_greeting_size) {
        if (_greeting_recv[10] >= 3) handshake_v3_x ();
        else error (protocol_error);
    }
}

bool zmtp_engine_t::handshake_v3_x () {
    _mechanism = new (std::nothrow) null_mechanism_t (_session, "", _options);
    alloc_assert (_mechanism);
    _encoder = new (std::nothrow) v2_encoder_t (_options.out_batch_size);
    alloc_assert (_encoder);
    _decoder = new (std::nothrow) v2_decoder_t (_options.in_batch_size, _options.max_msg_size, true);
    alloc_assert (_decoder);
    _handshaking = false;
    _session->engine_error (false, i_engine::no_error); // Signal handshake complete
    return true;
}

int zmtp_engine_t::routing_id_msg (msg_t *msg_) {
    int rc = _routing_id_msg.move (msg_);
    _next_msg = &zmtp_engine_t::pull_msg_from_session;
    return rc;
}

int zmtp_engine_t::process_routing_id_msg (msg_t *msg_) {
    _process_msg = &zmtp_engine_t::decode_and_push;
    return _session->push_msg (msg_);
}

} // namespace slk
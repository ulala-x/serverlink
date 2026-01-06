/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - Simplified for ROUTER socket only */

#include "../precompiled.hpp"
#include "../util/macros.hpp"

#include <limits.h>
#include <string.h>
#include <algorithm>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <new>
#include <sstream>

#include "zmtp_engine.hpp"
#include "../io/io_thread.hpp"
#include "../core/session_base.hpp"
#include "v2_encoder.hpp"
#include "v2_decoder.hpp"
#include "v3_1_encoder.hpp"
#include "../auth/null_mechanism.hpp"
#include "../util/config.hpp"
#include "../util/err.hpp"
#include "../util/likely.hpp"
#include "../protocol/wire.hpp"

slk::zmtp_engine_t::zmtp_engine_t (
  std::unique_ptr<i_async_stream> stream_,
  const options_t &options_,
  const endpoint_uri_pair_t &endpoint_uri_pair_) :
    stream_engine_base_t (std::move(stream_), options_, endpoint_uri_pair_, true),
    _greeting_size (v2_greeting_size),
    _greeting_bytes_read (0),
    _subscription_required (false),
    _heartbeat_timeout (0)
{
    _next_msg = static_cast<int (stream_engine_base_t::*) (msg_t *)> (
      &zmtp_engine_t::routing_id_msg);
    _process_msg = static_cast<int (stream_engine_base_t::*) (msg_t *)> (
      &zmtp_engine_t::process_routing_id_msg);

    int rc = _pong_msg.init ();
    errno_assert (rc == 0);

    rc = _routing_id_msg.init ();
    errno_assert (rc == 0);

    if (_options.heartbeat_interval > 0) {
        _heartbeat_timeout = _options.heartbeat_timeout;
        if (_heartbeat_timeout == -1)
            _heartbeat_timeout = _options.heartbeat_interval;
    }
}

slk::zmtp_engine_t::~zmtp_engine_t ()
{
    const int rc = _routing_id_msg.close ();
    errno_assert (rc == 0);
}

void slk::zmtp_engine_t::plug_internal ()
{
    // start optional timer, to prevent handshake hanging on no input
    set_handshake_timer ();

    //  Send the greeting.
    _outpos = _greeting_send;
    memset(_outpos, 0, v3_greeting_size);
    _outpos[0] = 0xff;
    put_uint64 (&_outpos[1], 1); // 8-byte length, only for signature
    _outpos[9] = 0x7f;
    _outpos[10] = 3; // Revision
    _outpos[11] = 0; // Minor
    memcpy(_outpos + 12, "NULL", 4);
    _outsize = v3_greeting_size;

    _greeting_size = v3_greeting_size;

    // The read loop is now started automatically in the base class plug().
    // The write loop will be started when there's data to send.
    if (_outsize > 0)
        restart_output();
}

void slk::zmtp_engine_t::process_handshake_data(unsigned char* buffer, size_t size)
{
    // Save the number of bytes read so far to calculate how many new bytes are consumed.
    size_t bytes_read_before = _greeting_bytes_read;

    // Append the newly received data to our internal greeting buffer.
    // Ensure we don't overflow the buffer.
    const size_t bytes_to_copy = std::min(size, v3_greeting_size - _greeting_bytes_read);
    memcpy(_greeting_recv + _greeting_bytes_read, buffer, bytes_to_copy);
    _greeting_bytes_read += bytes_to_copy;

    // Try to process the greeting with the data we have so far.
    if (process_greeting()) {
        // Handshake is complete.
        // Calculate how many bytes from the current 'buffer' were actually used to complete the handshake.
        // The total used is _greeting_bytes_read. The amount we already had is bytes_read_before.
        // So the amount taken from 'buffer' is (_greeting_bytes_read - bytes_read_before).
        
        const size_t consumed_from_buffer = _greeting_bytes_read - bytes_read_before;
        
        // Update the base class pointers to point to the remaining data (start of the first message).
        _inpos = buffer + consumed_from_buffer;
        _insize = size - consumed_from_buffer;

        set_handshake_complete();
    }
}

//  Position of the revision and minor fields in the greeting.
const size_t revision_pos = 10;
const size_t minor_pos = 11;

bool slk::zmtp_engine_t::process_greeting()
{
    // If we don't have the first byte, we can't do anything.
    if (_greeting_bytes_read == 0)
        return false;

    // Check for unversioned protocol (first byte not 0xff)
    bool unversioned = (_greeting_recv[0] != 0xff);
    
    if (unversioned) {
        // We have enough to decide.
        return (this->*select_handshake_fun (true, 0, 0)) ();
    }

    // Versioned protocol. We need at least 10 bytes to be sure.
    if (_greeting_bytes_read >= signature_size && !(_greeting_recv[9] & 0x01)) {
        unversioned = true;
        return (this->*select_handshake_fun (true, 0, 0)) ();
    }
    
    // We need more data to determine the version.
    if (_greeting_bytes_read < _greeting_size) {
        return false;
    }
    
    // We have the full greeting.
    return (this->*select_handshake_fun(false, _greeting_recv[revision_pos], _greeting_recv[minor_pos]))();
}


slk::zmtp_engine_t::handshake_fun_t slk::zmtp_engine_t::select_handshake_fun (
  bool unversioned_, unsigned char revision_, unsigned char minor_)
{
    //  ServerLink only supports ZMTP 3.x
    //  For simplicity, reject older protocols
    if (unversioned_) {
        return &zmtp_engine_t::handshake_v1_0_unversioned;
    }
    switch (revision_) {
        case ZMTP_1_0:
            return &zmtp_engine_t::handshake_v1_0;
        case ZMTP_2_0:
            return &zmtp_engine_t::handshake_v2_0;
        case ZMTP_3_x:
            switch (minor_) {
                case 0:
                    return &zmtp_engine_t::handshake_v3_0;
                default:
                    return &zmtp_engine_t::handshake_v3_1;
            }
        default:
            return &zmtp_engine_t::handshake_v3_1;
    }
}

bool slk::zmtp_engine_t::handshake_v1_0_unversioned ()
{
    // ServerLink doesn't support ZMTP 1.0 unversioned
    error (protocol_error);
    return false;
}

bool slk::zmtp_engine_t::handshake_v1_0 ()
{
    // ServerLink doesn't support ZMTP 1.0
    error (protocol_error);
    return false;
}

bool slk::zmtp_engine_t::handshake_v2_0 ()
{
    // ServerLink doesn't support ZMTP 2.0
    error (protocol_error);
    return false;
}

bool slk::zmtp_engine_t::handshake_v3_x ()
{
    // ServerLink only supports NULL mechanism
    if (memcmp (_greeting_recv + 12, "NULL\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 20) == 0) {
        _mechanism = new (std::nothrow)
          null_mechanism_t (session (), _peer_address, _options);
        alloc_assert (_mechanism);
    } else {
        // Unsupported mechanism
        error (protocol_error);
        return false;
    }

    _next_msg = &zmtp_engine_t::next_handshake_command;
    _process_msg = &zmtp_engine_t::process_handshake_command;

    // Trigger sending the READY command
    restart_output ();

    return true;
}

bool slk::zmtp_engine_t::handshake_v3_0 ()
{
    _encoder = new (std::nothrow) v2_encoder_t (_options.out_batch_size);
    alloc_assert (_encoder);

    _decoder = new (std::nothrow) v2_decoder_t (
      _options.in_batch_size, _options.maxmsgsize, _options.zero_copy);
    alloc_assert (_decoder);

    return slk::zmtp_engine_t::handshake_v3_x ();
}

bool slk::zmtp_engine_t::handshake_v3_1 ()
{
    _encoder = new (std::nothrow) v3_1_encoder_t (_options.out_batch_size);
    alloc_assert (_encoder);

    _decoder = new (std::nothrow) v2_decoder_t (
      _options.in_batch_size, _options.maxmsgsize, _options.zero_copy);
    alloc_assert (_decoder);

    return slk::zmtp_engine_t::handshake_v3_x ();
}

int slk::zmtp_engine_t::routing_id_msg (msg_t *msg_)
{
    const int rc = msg_->init_size (_options.routing_id_size);
    errno_assert (rc == 0);
    if (_options.routing_id_size > 0)
        memcpy (msg_->data (), _options.routing_id, _options.routing_id_size);
    _next_msg = &zmtp_engine_t::pull_msg_from_session;
    return 0;
}

int slk::zmtp_engine_t::process_routing_id_msg (msg_t *msg_)
{
    if (_options.recv_routing_id) {
        msg_->set_flags (msg_t::routing_id);
        const int rc = session ()->push_msg (msg_);
        errno_assert (rc == 0);
    } else {
        int rc = msg_->close ();
        errno_assert (rc == 0);
        rc = msg_->init ();
        errno_assert (rc == 0);
    }

    if (_mechanism->status () == mechanism_t::ready)
        mechanism_ready ();
    else
        _process_msg = &zmtp_engine_t::process_handshake_command;

    return 0;
}

int slk::zmtp_engine_t::produce_ping_message (msg_t *msg_)
{
    // Heartbeat PING support
    const int rc = msg_->init_size (7);
    errno_assert (rc == 0);

    unsigned char *data = static_cast<unsigned char *> (msg_->data ());
    put_uint16 (data, 0); // TTL
    memcpy (data + 2, "\4PING", 5);
    msg_->set_flags (msg_t::command);

    return 0;
}

int slk::zmtp_engine_t::process_heartbeat_message (msg_t *msg_)
{
    const unsigned char *data = static_cast<unsigned char *> (msg_->data ());
    const size_t data_size = msg_->size ();

    if (data_size >= 6 && memcmp (data + 2, "\4PING", 5) == 0) {
        // Received PING, prepare PONG
        const int rc = _pong_msg.close ();
        errno_assert (rc == 0);
        produce_pong_message (&_pong_msg);
    }

    const int rc = msg_->close ();
    errno_assert (rc == 0);

    return 0;
}

int slk::zmtp_engine_t::produce_pong_message (msg_t *msg_)
{
    const int rc = msg_->init_size (7);
    errno_assert (rc == 0);

    unsigned char *data = static_cast<unsigned char *> (msg_->data ());
    put_uint16 (data, 0); // TTL
    memcpy (data + 2, "\4PONG", 5);
    msg_->set_flags (msg_t::command);

    return 0;
}

int slk::zmtp_engine_t::process_command_message (msg_t *msg_)
{
    const uint8_t *data = static_cast<const uint8_t *> (msg_->data ());
    const size_t data_size = msg_->size ();

    // Command messages have format: [cmd_name_size][cmd_name][data]
    // cmd_name_size is the first byte and includes the size byte itself
    if (data_size < 1)
        return 0;

    const uint8_t cmd_name_size = data[0];
    if (data_size < static_cast<size_t> (cmd_name_size) + 1)
        return 0;

    const uint8_t *cmd_name = data + 1;

    // Check for SUBSCRIBE command ("\x9SUBSCRIBE" = size 9 + "SUBSCRIBE")
    const size_t sub_name_size = msg_t::sub_cmd_name_size - 1;  // 9
    const size_t cancel_name_size = msg_t::cancel_cmd_name_size - 1;  // 6
    const size_t ping_name_size = 4;  // "PING"

    if (cmd_name_size == sub_name_size
        && memcmp (cmd_name, "SUBSCRIBE", cmd_name_size) == 0) {
        msg_->set_flags (msg_t::subscribe);
        return 0;
    }

    if (cmd_name_size == cancel_name_size
        && memcmp (cmd_name, "CANCEL", cmd_name_size) == 0) {
        msg_->set_flags (msg_t::cancel);
        return 0;
    }

    if (cmd_name_size == ping_name_size
        && memcmp (cmd_name, "PING", cmd_name_size) == 0) {
        msg_->set_flags (msg_t::ping);
        return process_heartbeat_message (msg_);
    }

    if (cmd_name_size == ping_name_size
        && memcmp (cmd_name, "PONG", cmd_name_size) == 0) {
        msg_->set_flags (msg_t::pong);
        return process_heartbeat_message (msg_);
    }

    // Unknown command - ignore
    const int rc = msg_->close ();
    errno_assert (rc == 0);
    return 0;
}

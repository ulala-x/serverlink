/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - Simplified for ROUTER socket only */

#include "../precompiled.hpp"
#include "../util/macros.hpp"

#include <limits.h>
#include <string.h>

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
  fd_t fd_,
  const options_t &options_,
  const endpoint_uri_pair_t &endpoint_uri_pair_) :
    stream_engine_base_t (fd_, options_, endpoint_uri_pair_, true),
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

    //  Send the 'length' and 'flags' fields of the routing id message.
    //  The 'length' field is encoded in the long format.
    _outpos = _greeting_send;
    _outpos[_outsize++] = 0xff; // UCHAR_MAX signature start
    put_uint64 (&_outpos[_outsize], _options.routing_id_size + 1);
    _outsize += 8;
    _outpos[_outsize++] = 0x7f;

    set_pollin ();
    set_pollout ();
    //  Flush all the data that may have been already received downstream.
    in_event ();
}

//  Position of the revision and minor fields in the greeting.
const size_t revision_pos = 10;
const size_t minor_pos = 11;

bool slk::zmtp_engine_t::handshake ()
{
    slk_assert (_greeting_bytes_read < _greeting_size);
    //  Receive the greeting.
    const int rc = receive_greeting ();
    if (rc == -1)
        return false;
    const bool unversioned = rc != 0;

    if (!(this
            ->*select_handshake_fun (unversioned, _greeting_recv[revision_pos],
                                     _greeting_recv[minor_pos])) ())
        return false;

    // Start polling for output if necessary.
    if (_outsize == 0)
        set_pollout ();

    return true;
}

int slk::zmtp_engine_t::receive_greeting ()
{
    bool unversioned = false;
    while (_greeting_bytes_read < _greeting_size) {
        const int n = read (_greeting_recv + _greeting_bytes_read,
                            _greeting_size - _greeting_bytes_read);
        if (n == -1) {
            if (errno != EAGAIN)
                error (connection_error);
            return -1;
        }

        _greeting_bytes_read += n;

        //  We have received at least one byte from the peer.
        //  If the first byte is not 0xff, we know that the
        //  peer is using unversioned protocol.
        if (_greeting_recv[0] != 0xff) {
            unversioned = true;
            break;
        }

        if (_greeting_bytes_read < signature_size)
            continue;

        //  Inspect the right-most bit of the 10th byte (which coincides
        //  with the 'flags' field if a regular message was sent).
        //  Zero indicates this is a header of a routing id message
        //  (i.e. the peer is using the unversioned protocol).
        if (!(_greeting_recv[9] & 0x01)) {
            unversioned = true;
            break;
        }

        //  The peer is using versioned protocol.
        receive_greeting_versioned ();
    }
    return unversioned ? 1 : 0;
}

void slk::zmtp_engine_t::receive_greeting_versioned ()
{
    //  Send the major version number.
    if (_outpos + _outsize == _greeting_send + signature_size) {
        if (_outsize == 0)
            set_pollout ();
        _outpos[_outsize++] = 3; //  Major version number (ZMTP 3.x)
    }

    if (_greeting_bytes_read > signature_size) {
        if (_outpos + _outsize == _greeting_send + signature_size + 1) {
            if (_outsize == 0)
                set_pollout ();

            // ServerLink only supports ZMTP 3.x
            _outpos[_outsize++] = 1; //  Minor version number
            memset (_outpos + _outsize, 0, 20);

            // ServerLink only supports NULL mechanism
            memcpy (_outpos + _outsize, "NULL", 4);
            _outsize += 20;
            memset (_outpos + _outsize, 0, 32);
            _outsize += 32;
            _greeting_size = v3_greeting_size;
        }
    }
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

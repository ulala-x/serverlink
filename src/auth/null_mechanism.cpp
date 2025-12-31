/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "../precompiled.hpp"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../util/err.hpp"
#include "../msg/msg.hpp"
#include "../core/session_base.hpp"
#include "null_mechanism.hpp"

const char error_command_name[] = "\5ERROR";
const size_t error_command_name_len = sizeof (error_command_name) - 1;
const size_t error_reason_len_size = 1;

const char ready_command_name[] = "\5READY";
const size_t ready_command_name_len = sizeof (ready_command_name) - 1;

slk::null_mechanism_t::null_mechanism_t (session_base_t *session_,
                                         const std::string &peer_address_,
                                         const options_t &options_) :
    mechanism_base_t (session_, options_),
    _ready_command_sent (false),
    _error_command_sent (false),
    _ready_command_received (false),
    _error_command_received (false)
{
    // ZAP has been removed - peer_address_ is unused in ServerLink
    (void)peer_address_;
}

slk::null_mechanism_t::~null_mechanism_t ()
{
}

int slk::null_mechanism_t::next_handshake_command (msg_t *msg_)
{
    if (_ready_command_sent || _error_command_sent) {
        errno = EAGAIN;
        return -1;
    }

    // No ZAP in ServerLink - send READY directly
    make_command_with_basic_properties (msg_, ready_command_name,
                                        ready_command_name_len);

    _ready_command_sent = true;

    return 0;
}

int slk::null_mechanism_t::process_handshake_command (msg_t *msg_)
{
    if (_ready_command_received || _error_command_received) {
        // Unexpected command after handshake complete
        errno = EPROTO;
        return -1;
    }

    const unsigned char *cmd_data =
      static_cast<unsigned char *> (msg_->data ());
    const size_t data_size = msg_->size ();

    int rc = 0;
    if (data_size >= ready_command_name_len
        && !memcmp (cmd_data, ready_command_name, ready_command_name_len))
        rc = process_ready_command (cmd_data, data_size);
    else if (data_size >= error_command_name_len
             && !memcmp (cmd_data, error_command_name, error_command_name_len))
        rc = process_error_command (cmd_data, data_size);
    else {
        // Unknown command
        errno = EPROTO;
        rc = -1;
    }

    if (rc == 0) {
        rc = msg_->close ();
        errno_assert (rc == 0);
        rc = msg_->init ();
        errno_assert (rc == 0);
    }
    return rc;
}

int slk::null_mechanism_t::process_ready_command (
  const unsigned char *cmd_data_, size_t data_size_)
{
    _ready_command_received = true;
    return parse_metadata (cmd_data_ + ready_command_name_len,
                           data_size_ - ready_command_name_len);
}

int slk::null_mechanism_t::process_error_command (
  const unsigned char *cmd_data_, size_t data_size_)
{
    const size_t fixed_prefix_size =
      error_command_name_len + error_reason_len_size;
    if (data_size_ < fixed_prefix_size) {
        // Malformed ERROR command
        errno = EPROTO;
        return -1;
    }
    const size_t error_reason_len =
      static_cast<size_t> (cmd_data_[error_command_name_len]);
    if (error_reason_len > data_size_ - fixed_prefix_size) {
        // Malformed ERROR command - reason too long
        errno = EPROTO;
        return -1;
    }

    // Error command received - handshake failed
    _error_command_received = true;
    return 0;
}

slk::mechanism_t::status_t slk::null_mechanism_t::status () const
{
    if (_ready_command_sent && _ready_command_received)
        return ready;

    const bool command_sent = _ready_command_sent || _error_command_sent;
    const bool command_received =
      _ready_command_received || _error_command_received;
    return command_sent && command_received ? error : handshaking;
}

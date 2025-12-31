/* SPDX-License-Identifier: MPL-2.0 */

#include "v2_encoder.hpp"
#include "v2_protocol.hpp"
#include "wire.hpp"
#include "../util/likely.hpp"
#include "../msg/msg.hpp"
#include <climits>

namespace slk
{

v2_encoder_t::v2_encoder_t(std::size_t bufsize)
    : encoder_base_t<v2_encoder_t>(bufsize)
{
    // Write 0 bytes to the batch and go to message_ready state.
    next_step(nullptr, 0, &v2_encoder_t::message_ready, true);
}

v2_encoder_t::~v2_encoder_t()
{
}

void v2_encoder_t::message_ready()
{
    // Encode flags.
    std::size_t size = in_progress()->size();
    std::size_t header_size = 2; // flags byte + size byte
    unsigned char& protocol_flags = m_tmp_buf[0];
    protocol_flags = 0;

    if (in_progress()->flags() & msg_t::more) {
        protocol_flags |= v2_protocol_t::more_flag;
    }
    if (in_progress()->size() > UCHAR_MAX) {
        protocol_flags |= v2_protocol_t::large_flag;
    }
    if (in_progress()->flags() & msg_t::command) {
        protocol_flags |= v2_protocol_t::command_flag;
    }
    if (in_progress()->is_subscribe() || in_progress()->is_cancel()) {
        ++size;
    }

    // Encode the message length. For messages less than 256 bytes,
    // the length is encoded as 8-bit unsigned integer. For larger
    // messages, 64-bit unsigned integer in network byte order is used.
    if (unlikely(size > UCHAR_MAX)) {
        put_uint64(m_tmp_buf + 1, size);
        header_size = 9; // flags byte + size 8 bytes
    } else {
        m_tmp_buf[1] = static_cast<uint8_t>(size);
    }

    // Encode the subscribe/cancel byte. This is done in the encoder as
    // opposed to when the subscribe message is created to allow different
    // protocol behaviour on the wire in the v3.1 and legacy encoders.
    // It results in the work being done multiple times in case the sub
    // is sending the subscription/cancel to multiple pubs, but it cannot
    // be avoided. This processing can be moved to xsub once support for
    // ZMTP < 3.1 is dropped.
    if (in_progress()->is_subscribe()) {
        m_tmp_buf[header_size++] = 1;
    } else if (in_progress()->is_cancel()) {
        m_tmp_buf[header_size++] = 0;
    }

    next_step(m_tmp_buf, header_size, &v2_encoder_t::size_ready, false);
}

void v2_encoder_t::size_ready()
{
    // Write message body into the buffer.
    next_step(in_progress()->data(),
              in_progress()->size(),
              &v2_encoder_t::message_ready,
              true);
}

} // namespace slk

/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_V3_1_ENCODER_HPP_INCLUDED
#define SL_V3_1_ENCODER_HPP_INCLUDED

#include "encoder.hpp"
#include "../msg/msg.hpp"
#include "../util/macros.hpp"

namespace slk
{
// Encoder for ZMTP/3.1 framing protocol. Converts messages into data stream.
// ZMTP 3.1 is the latest version and includes command frames.

class v3_1_encoder_t final : public encoder_base_t<v3_1_encoder_t>
{
  public:
    explicit v3_1_encoder_t(std::size_t bufsize);
    ~v3_1_encoder_t();

  private:
    void size_ready();
    void message_ready();

    // Flags byte + size (8 bytes max) + sub/cancel command string
    unsigned char m_tmp_buf[9 + msg_t::sub_cmd_name_size];

    SL_NON_COPYABLE_NOR_MOVABLE(v3_1_encoder_t)
};

} // namespace slk

#endif

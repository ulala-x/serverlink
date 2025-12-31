/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_V2_ENCODER_HPP_INCLUDED
#define SL_V2_ENCODER_HPP_INCLUDED

#include "encoder.hpp"
#include "../util/macros.hpp"

namespace slk
{
// Encoder for ZMTP/2.x framing protocol. Converts messages into data stream.

class v2_encoder_t final : public encoder_base_t<v2_encoder_t>
{
  public:
    explicit v2_encoder_t(std::size_t bufsize);
    ~v2_encoder_t();

  private:
    void size_ready();
    void message_ready();

    // Flags byte + size byte (or 8 bytes) + sub/cancel byte
    unsigned char m_tmp_buf[10];

    SL_NON_COPYABLE_NOR_MOVABLE(v2_encoder_t)
};

} // namespace slk

#endif

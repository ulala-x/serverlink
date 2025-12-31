/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_V2_DECODER_HPP_INCLUDED
#define SL_V2_DECODER_HPP_INCLUDED

#include "decoder.hpp"
#include "decoder_allocators.hpp"
#include "../util/macros.hpp"
#include <cstdint>

namespace slk
{
// Decoder for ZMTP/2.x framing protocol. Converts data stream into messages.
// The class has to inherit from shared_message_memory_allocator because
// the base class calls allocate in its constructor.
class v2_decoder_t final
    : public decoder_base_t<v2_decoder_t, shared_message_memory_allocator>
{
  public:
    v2_decoder_t(std::size_t bufsize, int64_t maxmsgsize, bool zero_copy);
    ~v2_decoder_t();

    // i_decoder interface.
    msg_t* msg() override
    {
        return &m_in_progress;
    }

  private:
    int flags_ready(const unsigned char* data);
    int one_byte_size_ready(const unsigned char* read_from);
    int eight_byte_size_ready(const unsigned char* read_from);
    int message_ready(const unsigned char* data);

    int size_ready(uint64_t size, const unsigned char* read_pos);

    unsigned char m_tmpbuf[8];
    unsigned char m_msg_flags;
    msg_t m_in_progress;

    const bool m_zero_copy;
    const int64_t m_max_msg_size;

    SL_NON_COPYABLE_NOR_MOVABLE(v2_decoder_t)
};

} // namespace slk

#endif

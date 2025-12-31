/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_RAW_DECODER_HPP_INCLUDED
#define SL_RAW_DECODER_HPP_INCLUDED

#include "i_decoder.hpp"
#include "decoder_allocators.hpp"
#include "../msg/msg.hpp"
#include "../util/macros.hpp"
#include <cstddef>
#include <cstdint>

namespace slk
{
// Decoder for raw TCP mode (no ZMTP framing).
// Each received chunk becomes a complete message.

class raw_decoder_t final : public i_decoder
{
  public:
    explicit raw_decoder_t(std::size_t bufsize);
    ~raw_decoder_t();

    // i_decoder interface.
    void get_buffer(unsigned char** data, std::size_t* size) override;

    int decode(const unsigned char* data,
               std::size_t size,
               std::size_t& bytes_used) override;

    msg_t* msg() override
    {
        return &m_in_progress;
    }

    void resize_buffer(std::size_t /*new_size*/) override
    {
        // No-op for raw decoder
    }

  private:
    msg_t m_in_progress;
    shared_message_memory_allocator m_allocator;

    SL_NON_COPYABLE_NOR_MOVABLE(raw_decoder_t)
};

} // namespace slk

#endif

/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_I_DECODER_HPP_INCLUDED
#define SL_I_DECODER_HPP_INCLUDED

#include "../util/macros.hpp"
#include <cstddef>
#include <cstdint>

namespace slk
{
class msg_t;

// Interface to be implemented by message decoders.
// Decoders parse incoming byte streams into messages.

class i_decoder
{
  public:
    virtual ~i_decoder() = default;

    // Get a buffer to receive data into.
    // Sets *data to point to the buffer and *size to the buffer size.
    virtual void get_buffer(unsigned char** data, std::size_t* size) = 0;

    // Resize the internal buffer (for zero-copy optimization).
    virtual void resize_buffer(std::size_t new_size) = 0;

    // Decode data from the provided buffer.
    // Returns:
    //   1  - A complete message has been decoded
    //   0  - Need more data
    //   -1 - Error (errno is set)
    // processed is set to the number of bytes consumed.
    virtual int decode(const unsigned char* data,
                       std::size_t size,
                       std::size_t& processed) = 0;

    // Get the decoded message.
    virtual msg_t* msg() = 0;
};

} // namespace slk

#endif

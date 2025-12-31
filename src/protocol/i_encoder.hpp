/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_I_ENCODER_HPP_INCLUDED
#define SL_I_ENCODER_HPP_INCLUDED

#include "../util/macros.hpp"
#include <cstddef>
#include <cstdint>

namespace slk
{
// Forward declaration
class msg_t;

// Interface to be implemented by message encoders.
// Encoders convert messages into byte streams for transmission.

struct i_encoder
{
    virtual ~i_encoder() SL_DEFAULT;

    // Encode data to the provided buffer.
    // If data points to NULL, the encoder provides its own buffer.
    // Returns the number of bytes encoded.
    // Returns 0 when a new message is required (call load_msg).
    virtual std::size_t encode(unsigned char** data, std::size_t size) = 0;

    // Load a new message into the encoder for encoding.
    virtual void load_msg(msg_t* msg) = 0;
};

} // namespace slk

#endif

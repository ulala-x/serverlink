/* SPDX-License-Identifier: MPL-2.0 */

#include "raw_encoder.hpp"
#include "../msg/msg.hpp"

namespace slk
{

raw_encoder_t::raw_encoder_t(std::size_t bufsize)
    : encoder_base_t<raw_encoder_t>(bufsize)
{
    // Write 0 bytes to the batch and go to message_ready state.
    next_step(nullptr, 0, &raw_encoder_t::raw_message_ready, true);
}

raw_encoder_t::~raw_encoder_t()
{
}

void raw_encoder_t::raw_message_ready()
{
    // For raw mode, just write the message data without any framing
    next_step(in_progress()->data(),
              in_progress()->size(),
              &raw_encoder_t::raw_message_ready,
              true);
}

} // namespace slk

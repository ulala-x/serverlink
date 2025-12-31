/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_RAW_ENCODER_HPP_INCLUDED
#define SL_RAW_ENCODER_HPP_INCLUDED

#include "encoder.hpp"
#include "../util/macros.hpp"
#include <cstddef>

namespace slk
{
// Encoder for raw TCP mode (no ZMTP framing).
// Converts messages into data batches without framing.

class raw_encoder_t final : public encoder_base_t<raw_encoder_t>
{
  public:
    explicit raw_encoder_t(std::size_t bufsize);
    ~raw_encoder_t();

  private:
    void raw_message_ready();

    SL_NON_COPYABLE_NOR_MOVABLE(raw_encoder_t)
};

} // namespace slk

#endif

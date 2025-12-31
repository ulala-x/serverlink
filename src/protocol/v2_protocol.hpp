/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_V2_PROTOCOL_HPP_INCLUDED
#define SL_V2_PROTOCOL_HPP_INCLUDED

namespace slk
{
// Definition of constants for ZMTP/2.0 transport protocol.
class v2_protocol_t
{
  public:
    // Message flags (encoded in the first byte of each frame).
    enum
    {
        more_flag = 1,     // More message parts follow
        large_flag = 2,    // Message size is 8 bytes (vs 1 byte)
        command_flag = 4   // This is a command frame
    };
};

} // namespace slk

#endif

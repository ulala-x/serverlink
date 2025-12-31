/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_WIRE_HPP_INCLUDED
#define SL_WIRE_HPP_INCLUDED

#include <cstdint>

namespace slk
{
// Helper functions to convert different integer types to/from network
// byte order (big-endian).

inline void put_uint8(unsigned char* buffer, uint8_t value)
{
    *buffer = value;
}

inline uint8_t get_uint8(const unsigned char* buffer)
{
    return *buffer;
}

inline void put_uint16(unsigned char* buffer, uint16_t value)
{
    buffer[0] = static_cast<unsigned char>((value >> 8) & 0xff);
    buffer[1] = static_cast<unsigned char>(value & 0xff);
}

inline uint16_t get_uint16(const unsigned char* buffer)
{
    return (static_cast<uint16_t>(buffer[0]) << 8) |
           static_cast<uint16_t>(buffer[1]);
}

inline void put_uint32(unsigned char* buffer, uint32_t value)
{
    buffer[0] = static_cast<unsigned char>((value >> 24) & 0xff);
    buffer[1] = static_cast<unsigned char>((value >> 16) & 0xff);
    buffer[2] = static_cast<unsigned char>((value >> 8) & 0xff);
    buffer[3] = static_cast<unsigned char>(value & 0xff);
}

inline uint32_t get_uint32(const unsigned char* buffer)
{
    return (static_cast<uint32_t>(buffer[0]) << 24) |
           (static_cast<uint32_t>(buffer[1]) << 16) |
           (static_cast<uint32_t>(buffer[2]) << 8) |
           static_cast<uint32_t>(buffer[3]);
}

inline void put_uint64(unsigned char* buffer, uint64_t value)
{
    buffer[0] = static_cast<unsigned char>((value >> 56) & 0xff);
    buffer[1] = static_cast<unsigned char>((value >> 48) & 0xff);
    buffer[2] = static_cast<unsigned char>((value >> 40) & 0xff);
    buffer[3] = static_cast<unsigned char>((value >> 32) & 0xff);
    buffer[4] = static_cast<unsigned char>((value >> 24) & 0xff);
    buffer[5] = static_cast<unsigned char>((value >> 16) & 0xff);
    buffer[6] = static_cast<unsigned char>((value >> 8) & 0xff);
    buffer[7] = static_cast<unsigned char>(value & 0xff);
}

inline uint64_t get_uint64(const unsigned char* buffer)
{
    return (static_cast<uint64_t>(buffer[0]) << 56) |
           (static_cast<uint64_t>(buffer[1]) << 48) |
           (static_cast<uint64_t>(buffer[2]) << 40) |
           (static_cast<uint64_t>(buffer[3]) << 32) |
           (static_cast<uint64_t>(buffer[4]) << 24) |
           (static_cast<uint64_t>(buffer[5]) << 16) |
           (static_cast<uint64_t>(buffer[6]) << 8) |
           static_cast<uint64_t>(buffer[7]);
}

} // namespace slk

#endif

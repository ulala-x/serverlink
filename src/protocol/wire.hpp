/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_WIRE_HPP_INCLUDED
#define SL_WIRE_HPP_INCLUDED

#include <stdint.h>
#include <string.h>
#include <bit>

namespace slk
{
// libzmq parity: Optimized network byte order conversions
// C++20: use std::endian and bit_cast for zero-overhead conversion

inline uint32_t get_uint32 (const unsigned char *buf_) {
    uint32_t res;
    memcpy (&res, buf_, sizeof (res));
    if constexpr (std::endian::native == std::endian::little) return __builtin_bswap32 (res);
    return res;
}

inline void put_uint32 (unsigned char *buf_, uint32_t val_) {
    if constexpr (std::endian::native == std::endian::little) val_ = __builtin_bswap32 (val_);
    memcpy (buf_, &val_, sizeof (val_));
}

inline uint64_t get_uint64 (const unsigned char *buf_) {
    uint64_t res;
    memcpy (&res, buf_, sizeof (res));
    if constexpr (std::endian::native == std::endian::little) return __builtin_bswap64 (res);
    return res;
}

inline void put_uint64 (unsigned char *buf_, uint64_t val_) {
    if constexpr (std::endian::native == std::endian::little) val_ = __builtin_bswap64 (val_);
    memcpy (buf_, &val_, sizeof (val_));
}

} // namespace slk

#endif
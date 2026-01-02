/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_LIKELY_HPP_INCLUDED
#define SL_LIKELY_HPP_INCLUDED

#include <serverlink/config.h>

// Branch prediction hints
// C++20 provides [[likely]] and [[unlikely]] attributes for branch prediction.
// For older compilers or when C++20 attributes are not available, fall back to
// compiler-specific intrinsics (__builtin_expect for GCC/Clang).

#if SL_HAVE_LIKELY
    // C++20 attributes - Note: these are statement attributes, not expression macros
    // They must be used directly in if/else statements, not wrapped in macros
    #define SL_LIKELY_ATTR   [[likely]]
    #define SL_UNLIKELY_ATTR [[unlikely]]

    // For backward compatibility, keep the old macro style using __builtin_expect
    #if defined __GNUC__
        #define likely(x)   __builtin_expect((x), 1)
        #define unlikely(x) __builtin_expect((x), 0)
    #else
        #define likely(x)   (x)
        #define unlikely(x) (x)
    #endif
#else
    // Fallback for compilers without C++20 attribute support
    #define SL_LIKELY_ATTR
    #define SL_UNLIKELY_ATTR

    #if defined __GNUC__
        #define likely(x)   __builtin_expect((x), 1)
        #define unlikely(x) __builtin_expect((x), 0)
    #else
        #define likely(x)   (x)
        #define unlikely(x) (x)
    #endif
#endif

#endif

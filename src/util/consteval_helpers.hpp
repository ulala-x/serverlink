/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - C++20 Compile-Time Utilities */

#ifndef SL_CONSTEVAL_HELPERS_HPP_INCLUDED
#define SL_CONSTEVAL_HELPERS_HPP_INCLUDED

#include <serverlink/config.h>
#include <cstddef>

namespace slk {

// C++20: consteval functions for compile-time computation
// These functions are guaranteed to execute at compile time only

#if SL_HAVE_CONSTEVAL

// Compile-time power of 2 check
consteval bool is_power_of_2(size_t n) noexcept {
    return n > 0 && (n & (n - 1)) == 0;
}

// Compile-time next power of 2
consteval size_t next_power_of_2(size_t n) noexcept {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if constexpr (sizeof(size_t) > 4) {
        n |= n >> 32;
    }
    return n + 1;
}

// Compile-time alignment check
consteval bool is_aligned(size_t size, size_t alignment) noexcept {
    return (size % alignment) == 0;
}

// Compile-time alignment computation
consteval size_t align_up(size_t size, size_t alignment) noexcept {
    return (size + alignment - 1) & ~(alignment - 1);
}

// Compile-time size validation - returns n if valid, 0 otherwise
consteval size_t validate_queue_granularity(size_t n) noexcept {
    // Queue granularity must be positive for optimal performance
    return (n > 0) ? n : 0;
}

#else

// Fallback to constexpr for non-C++20 compilers
constexpr bool is_power_of_2(size_t n) noexcept {
    return n > 0 && (n & (n - 1)) == 0;
}

constexpr size_t next_power_of_2(size_t n) noexcept {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if constexpr (sizeof(size_t) > 4) {
        n |= n >> 32;
    }
    return n + 1;
}

constexpr bool is_aligned(size_t size, size_t alignment) noexcept {
    return (size % alignment) == 0;
}

constexpr size_t align_up(size_t size, size_t alignment) noexcept {
    return (size + alignment - 1) & ~(alignment - 1);
}

constexpr size_t validate_queue_granularity(size_t n) noexcept {
    return n;
}

#endif

// Compile-time constants for queue granularity
// These values are tuned for optimal cache performance
inline constexpr size_t message_pipe_granularity = 256;
inline constexpr size_t command_pipe_granularity = 16;

// Polling constants
inline constexpr size_t default_pollitems = 16;

// Compile-time validation
static_assert(message_pipe_granularity > 0, "Message pipe granularity must be positive");
static_assert(command_pipe_granularity > 0, "Command pipe granularity must be positive");
static_assert(default_pollitems > 0, "Default pollitems must be positive");

}  // namespace slk

#endif

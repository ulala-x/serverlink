/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - C++20 Concepts for type constraints */

#ifndef SL_CONCEPTS_HPP_INCLUDED
#define SL_CONCEPTS_HPP_INCLUDED

#include <serverlink/config.h>

#if SL_HAVE_CONCEPTS

#include <concepts>
#include <type_traits>
#include <cstddef>

namespace slk {

// =============================================================================
// Lock-free Data Structure Concepts
// =============================================================================

// Concept for types that can be stored in lock-free queues (ypipe, yqueue)
// Requirements:
// - Trivially copyable (for safe memcpy-like operations)
// - Default constructible (for pre-allocation)
// - Destructible (for cleanup)
template <typename T>
concept YPipeable = std::is_trivially_copyable_v<T>
                 && std::is_default_constructible_v<T>
                 && std::is_destructible_v<T>;

// Concept for pointer types used in atomic operations
template <typename T>
concept AtomicPointerable = std::is_pointer_v<T>
                         && std::is_trivially_copyable_v<T>;

// =============================================================================
// Message Concepts
// =============================================================================

// Concept for types that behave like messages (have data and size)
template <typename T>
concept MessageLike = requires(T& t, const T& ct) {
    { t.data() } -> std::convertible_to<void*>;
    { ct.data() } -> std::convertible_to<const void*>;
    { ct.size() } -> std::convertible_to<size_t>;
};

// Concept for types that can be used as buffers
template <typename T>
concept BufferLike = requires(T& t, const T& ct) {
    { t.data() } -> std::convertible_to<unsigned char*>;
    { ct.size() } -> std::convertible_to<size_t>;
    { t.resize(size_t{}) };
};

// =============================================================================
// Pipe Event Handler Concepts
// =============================================================================

// Concept for pipe event handlers
template <typename T>
concept PipeEventHandler = requires(T& t) {
    // Forward declare pipe_t to avoid circular dependencies
    typename T::pipe_type;
};

// =============================================================================
// Socket Concepts
// =============================================================================

// Concept for socket option types
template <typename T>
concept SocketOption = std::is_trivially_copyable_v<T>
                    && (std::is_integral_v<T> || std::is_pointer_v<T>);

// =============================================================================
// Callable Concepts
// =============================================================================

// Concept for timer callbacks
template <typename F>
concept TimerCallback = std::invocable<F, int>;

// Concept for socket callbacks (for polling)
template <typename F>
concept SocketCallback = std::invocable<F, short>;

} // namespace slk

#else // !SL_HAVE_CONCEPTS

// Fallback: No concept constraints for pre-C++20 compilers
// These macros allow code to compile but provide no compile-time checking

#define YPipeable typename
#define AtomicPointerable typename
#define MessageLike typename
#define BufferLike typename
#define PipeEventHandler typename
#define SocketOption typename
#define TimerCallback typename
#define SocketCallback typename

#endif // SL_HAVE_CONCEPTS

#endif // SL_CONCEPTS_HPP_INCLUDED

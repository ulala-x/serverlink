/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq (C++11 simplified) */

#ifndef SL_ATOMIC_COUNTER_HPP_INCLUDED
#define SL_ATOMIC_COUNTER_HPP_INCLUDED

#include <atomic>
#include <cstdint>

#include "macros.hpp"

namespace slk {

// This class represents an integer that can be incremented/decremented
// in atomic fashion.
class alignas(sizeof(void *)) atomic_counter_t {
  public:
    typedef uint32_t integer_t;

    atomic_counter_t(integer_t value = 0) noexcept : _value(value) {}

    // Set counter value (not thread-safe).
    void set(integer_t value) noexcept { _value = value; }

    // Atomic addition. Returns the old value.
    integer_t add(integer_t increment) noexcept
    {
        return _value.fetch_add(increment, std::memory_order_acq_rel);
    }

    // Atomic subtraction. Returns false if the counter drops to zero.
    bool sub(integer_t decrement) noexcept
    {
        const integer_t old = _value.fetch_sub(decrement, std::memory_order_acq_rel);
        return old - decrement != 0;
    }

    integer_t get() const noexcept
    {
        return _value.load(std::memory_order_relaxed);
    }

  private:
    std::atomic<integer_t> _value;
};

// C API wrapper functions
void* atomic_counter_new();
void atomic_counter_set(void* counter, int value);
int atomic_counter_inc(void* counter);
int atomic_counter_dec(void* counter);
int atomic_counter_value(void* counter);
void atomic_counter_destroy(void** counter_p);

}  // namespace slk

#endif

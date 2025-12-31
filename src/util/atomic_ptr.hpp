/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq (C++11 simplified) */

#ifndef SL_ATOMIC_PTR_HPP_INCLUDED
#define SL_ATOMIC_PTR_HPP_INCLUDED

#include <atomic>
#include <cstddef>

#include "macros.hpp"

namespace slk {

// This class encapsulates several atomic operations on pointers.
template <typename T>
class atomic_ptr_t {
  public:
    // Initialise atomic pointer
    atomic_ptr_t() SL_NOEXCEPT : _ptr(nullptr) {}

    // Set value of atomic pointer in a non-threadsafe way
    // Use this function only when you are sure that at most one
    // thread is accessing the pointer at the moment.
    void set(T *ptr) SL_NOEXCEPT { _ptr.store(ptr, std::memory_order_relaxed); }

    // Perform atomic 'exchange pointers' operation. Pointer is set
    // to the 'val' value. Old value is returned.
    T *xchg(T *val) SL_NOEXCEPT
    {
        return _ptr.exchange(val, std::memory_order_acq_rel);
    }

    // Perform atomic 'compare and swap' operation on the pointer.
    // The pointer is compared to 'cmp' argument and if they are
    // equal, its value is set to 'val'. Old value of the pointer
    // is returned.
    T *cas(T *cmp, T *val) SL_NOEXCEPT
    {
        _ptr.compare_exchange_strong(cmp, val, std::memory_order_acq_rel);
        return cmp;
    }

  private:
    std::atomic<T *> _ptr;
};

// Atomic value for storing integers.
struct atomic_value_t {
    atomic_value_t(const int value) SL_NOEXCEPT : _value(value) {}

    atomic_value_t(const atomic_value_t &src) SL_NOEXCEPT
        : _value(src.load()) {}

    void store(const int value) SL_NOEXCEPT
    {
        _value.store(value, std::memory_order_release);
    }

    int load() const SL_NOEXCEPT
    {
        return _value.load(std::memory_order_acquire);
    }

  private:
    std::atomic<int> _value;

    atomic_value_t &operator=(const atomic_value_t &src) = delete;
};

}  // namespace slk

#endif

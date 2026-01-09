/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - 1:1 Parity with clock.cpp */

#include "../precompiled.hpp"
#include "clock.hpp"
#include "config.hpp"
#include "err.hpp"
#include "likely.hpp"

#include <chrono>

namespace slk {

clock_t::clock_t () : _last_tsc (0), _last_time (0) {}
clock_t::~clock_t () {}

uint64_t clock_t::now_ms () {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

uint64_t clock_t::now_us () {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

uint64_t clock_t::rdtsc () {
#if (defined __i386__ || defined __x86_64__)
    uint32_t low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d" (high));
    return (uint64_t) high << 32 | low;
#else
    return now_us();
#endif
}

} // namespace slk
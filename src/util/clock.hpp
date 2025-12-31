/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_CLOCK_HPP_INCLUDED
#define SL_CLOCK_HPP_INCLUDED

#include "macros.hpp"
#include <cstdint>

#if defined(__APPLE__)
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#ifndef HAVE_CLOCK_GETTIME
#define HAVE_CLOCK_GETTIME
#endif

#include <mach/clock.h>
#include <mach/mach.h>
#include <time.h>
#include <sys/time.h>
#endif

namespace slk {

class clock_t {
  public:
    clock_t();

    // CPU's timestamp counter. Returns 0 if it's not available.
    static uint64_t rdtsc();

    // High precision timestamp in microseconds.
    static uint64_t now_us();

    // Low precision timestamp in milliseconds. In tight loops generating it
    // can be 10 to 100 times faster than the high precision timestamp.
    uint64_t now_ms();

  private:
    // TSC timestamp of when last time measurement was made.
    uint64_t _last_tsc;

    // Physical time corresponding to the TSC above (in milliseconds).
    uint64_t _last_time;

    SL_NON_COPYABLE_NOR_MOVABLE(clock_t)
};

}  // namespace slk

#endif

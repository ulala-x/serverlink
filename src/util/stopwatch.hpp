/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Stopwatch utility for timing measurements */

#ifndef SL_STOPWATCH_HPP_INCLUDED
#define SL_STOPWATCH_HPP_INCLUDED

#include "macros.hpp"
#include <cstdint>

namespace slk {

class stopwatch_t {
  public:
    stopwatch_t();

    // Get intermediate time (microseconds since start)
    uint64_t intermediate();

    // Stop the stopwatch and return elapsed time in microseconds
    uint64_t stop();

  private:
    uint64_t _start;
    bool _stopped;

    SL_NON_COPYABLE_NOR_MOVABLE(stopwatch_t)
};

} // namespace slk

#endif

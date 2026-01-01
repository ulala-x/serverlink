/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Stopwatch utility implementation */

#include "stopwatch.hpp"
#include "clock.hpp"

namespace slk {

stopwatch_t::stopwatch_t() : _start(clock_t::now_us()), _stopped(false)
{
}

uint64_t stopwatch_t::intermediate()
{
    if (_stopped)
        return 0;
    return clock_t::now_us() - _start;
}

uint64_t stopwatch_t::stop()
{
    if (_stopped)
        return 0;

    _stopped = true;
    return clock_t::now_us() - _start;
}

} // namespace slk

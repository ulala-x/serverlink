/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_TIMERS_HPP_INCLUDED
#define SL_TIMERS_HPP_INCLUDED

#include "clock.hpp"
#include "macros.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>

namespace slk {

typedef void (timers_timer_fn)(int timer_id, void* arg);

class timers_t {
  public:
    timers_t();
    ~timers_t();

    // Add timer to the set, timer repeats forever, or until cancel is called.
    // Returns a timer_id that is used to cancel the timer.
    // Returns -1 if there was an error.
    int add(size_t interval, timers_timer_fn handler, void* arg);

    // Set the interval of the timer.
    // This method is slow, cancelling existing and adding a new timer yield better performance.
    // Returns 0 on success and -1 on error.
    int set_interval(int timer_id, size_t interval);

    // Reset the timer.
    // This method is slow, cancelling existing and adding a new timer yield better performance.
    // Returns 0 on success and -1 on error.
    int reset(int timer_id);

    // Cancel a timer.
    // Returns 0 on success and -1 on error.
    int cancel(int timer_id);

    // Returns the time in milliseconds until the next timer.
    // Returns -1 if no timer is due.
    long timeout();

    // Execute timers.
    // Return 0 if all succeed and -1 if error.
    int execute();

    // Return false if object is not a timers class.
    bool check_tag() const;

  private:
    // Used to check whether the object is a timers class.
    uint32_t _tag;

    int _next_timer_id;

    // Clock instance.
    clock_t _clock;

    struct timer_t {
        int timer_id;
        size_t interval;
        timers_timer_fn* handler;
        void* arg;
    };

    typedef std::multimap<uint64_t, timer_t> timersmap_t;
    timersmap_t _timers;

    typedef std::set<int> cancelled_timers_t;
    cancelled_timers_t _cancelled_timers;

    struct match_by_id;

    SL_NON_COPYABLE_NOR_MOVABLE(timers_t)
};

} // namespace slk

#endif

/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "timers.hpp"
#include "err.hpp"

#include <algorithm>
#include <cerrno>
#include <serverlink/config.h>

#if SL_HAVE_RANGES
#include <ranges>
#endif

namespace slk {

timers_t::timers_t() : _tag(0xCAFEDADA), _next_timer_id(0)
{
}

timers_t::~timers_t()
{
    // Mark the timers as dead
    _tag = 0xDEADBEEF;
}

bool timers_t::check_tag() const
{
    return _tag == 0xCAFEDADA;
}

int timers_t::add(size_t interval, timers_timer_fn handler, void* arg)
{
    if (handler == nullptr) {
        errno = EFAULT;
        return -1;
    }

    const uint64_t when = _clock.now_ms() + interval;
    timer_t timer = {++_next_timer_id, interval, handler, arg};
    _timers.insert(timersmap_t::value_type(when, timer));

    return timer.timer_id;
}

struct timers_t::match_by_id {
    match_by_id(int timer_id) : _timer_id(timer_id) {}

    bool operator()(const timersmap_t::value_type& entry) const
    {
        return entry.second.timer_id == _timer_id;
    }

  private:
    int _timer_id;
};

int timers_t::cancel(int timer_id)
{
    // Check first if timer exists at all
#if SL_HAVE_RANGES
    if (std::ranges::find_if(_timers, match_by_id(timer_id)) == _timers.end()) {
#else
    if (_timers.end() == std::find_if(_timers.begin(), _timers.end(), match_by_id(timer_id))) {
#endif
        errno = EINVAL;
        return -1;
    }

    // Check if timer was already cancelled
    if (_cancelled_timers.count(timer_id)) {
        errno = EINVAL;
        return -1;
    }

    _cancelled_timers.insert(timer_id);

    return 0;
}

int timers_t::set_interval(int timer_id, size_t interval)
{
#if SL_HAVE_RANGES
    auto it = std::ranges::find_if(_timers, match_by_id(timer_id));
    if (it != _timers.end()) {
#else
    const timersmap_t::iterator end = _timers.end();
    const timersmap_t::iterator it = std::find_if(_timers.begin(), end, match_by_id(timer_id));
    if (it != end) {
#endif
        timer_t timer = it->second;
        timer.interval = interval;
        const uint64_t when = _clock.now_ms() + interval;
        _timers.erase(it);
        _timers.insert(timersmap_t::value_type(when, timer));

        return 0;
    }

    errno = EINVAL;
    return -1;
}

int timers_t::reset(int timer_id)
{
#if SL_HAVE_RANGES
    auto it = std::ranges::find_if(_timers, match_by_id(timer_id));
    if (it != _timers.end()) {
#else
    const timersmap_t::iterator end = _timers.end();
    const timersmap_t::iterator it = std::find_if(_timers.begin(), end, match_by_id(timer_id));
    if (it != end) {
#endif
        timer_t timer = it->second;
        const uint64_t when = _clock.now_ms() + timer.interval;
        _timers.erase(it);
        _timers.insert(timersmap_t::value_type(when, timer));

        return 0;
    }

    errno = EINVAL;
    return -1;
}

long timers_t::timeout()
{
    const uint64_t now = _clock.now_ms();
    long res = -1;

    const timersmap_t::iterator begin = _timers.begin();
    const timersmap_t::iterator end = _timers.end();
    timersmap_t::iterator it = begin;
    for (; it != end; ++it) {
        if (0 == _cancelled_timers.erase(it->second.timer_id)) {
            // Live timer, lets return the timeout
            res = std::max(static_cast<long>(it->first - now), 0L);
            break;
        }
    }

    // Remove timed-out timers
    _timers.erase(begin, it);

    return res;
}

int timers_t::execute()
{
    const uint64_t now = _clock.now_ms();

    const timersmap_t::iterator begin = _timers.begin();
    const timersmap_t::iterator end = _timers.end();
    timersmap_t::iterator it = _timers.begin();
    for (; it != end; ++it) {
        if (0 == _cancelled_timers.erase(it->second.timer_id)) {
            // Timer is not cancelled

            // Map is ordered, if we have to wait for current timer we can stop.
            if (it->first > now)
                break;

            const timer_t& timer = it->second;

            timer.handler(timer.timer_id, timer.arg);

            _timers.insert(timersmap_t::value_type(now + timer.interval, timer));
        }
    }
    _timers.erase(begin, it);

    return 0;
}

} // namespace slk

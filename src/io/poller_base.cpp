/* SPDX-License-Identifier: MPL-2.0 */

#include "poller_base.hpp"
#include "i_poll_events.hpp"
#include "../util/err.hpp"
#include <cstdio>

namespace slk
{
poller_base_t::~poller_base_t ()
{
    const int remaining_load = get_load ();
    if (remaining_load != 0) {
        (void)remaining_load; 
    }
}

int poller_base_t::get_load () const
{
    return _load.get ();
}

void poller_base_t::adjust_load (int amount_)
{
    if (amount_ > 0)
        _load.add (amount_);
    else if (amount_ < 0)
        _load.sub (-amount_);
}

void poller_base_t::add_timer (int timeout_, i_poll_events *sink_, int id_)
{
    uint64_t expiration = _clock.now_ms () + timeout_;
    timer_info_t info = {sink_, id_};
    _timers.insert (timers_t::value_type (expiration, info));
}

void poller_base_t::cancel_timer (i_poll_events *sink_, int id_)
{
    for (timers_t::iterator it = _timers.begin (), end = _timers.end ();
         it != end; ++it)
        if (it->second.sink == sink_ && it->second.id == id_) {
            _timers.erase (it);
            return;
        }
}

uint64_t poller_base_t::execute_timers ()
{
    if (_timers.empty ()) 
        return 0;

    const uint64_t current = _clock.now_ms ();

    uint64_t res = 0;
    timer_info_t timer_temp;
    timers_t::iterator it;

    do {
        it = _timers.begin ();

        if (it->first > current) {
            res = it->first - current;
            break;
        }

        timer_temp = it->second;
        _timers.erase (it);

        timer_temp.sink->timer_event (timer_temp.id);

    } while (!_timers.empty ());

    return res;
}

worker_poller_base_t::worker_poller_base_t (ctx_t *ctx_) :
    _stopping (false), _ctx (ctx_)
{
}

void worker_poller_base_t::stop_worker ()
{
    _stopping = true;
    _worker.stop ();
}

void worker_poller_base_t::start (const char *name_)
{
    slk_assert (get_load () > 0);
    _worker.start (worker_routine, this);
    // TODO: Set thread name if supported
    (void)name_;
}

void worker_poller_base_t::check_thread () const
{
#ifndef NDEBUG
    slk_assert (!_worker.get_started () || _worker.is_current_thread ());
#endif
}

void worker_poller_base_t::worker_routine (void *arg_)
{
    fprintf(stderr, "DEBUG: worker_routine starting thread\n");
    fflush(stderr);
    worker_poller_base_t *self = static_cast<worker_poller_base_t *> (arg_);
    self->loop ();
    fprintf(stderr, "DEBUG: worker_routine thread exiting\n");
    fflush(stderr);
}

} // namespace slk
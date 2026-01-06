/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_POLLER_BASE_HPP_INCLUDED
#define SERVERLINK_POLLER_BASE_HPP_INCLUDED

#include <map>

#include "../util/clock.hpp"
#include "../util/atomic_counter.hpp"
#include "../util/macros.hpp"
#include "../util/thread.hpp"

namespace slk
{
struct i_poll_events;
class ctx_t;  // Forward declaration

// Base class for poller implementations
// Provides timer management and load tracking
class poller_base_t
{
  public:
    poller_base_t () = default;
    virtual ~poller_base_t ();

    // Methods from the poller concept
    int get_load () const;
    void add_timer (int timeout_, slk::i_poll_events *sink_, int id_);
    void cancel_timer (slk::i_poll_events *sink_, int id_);

    // Called by individual poller implementations to manage the load
    void adjust_load (int amount_);

  protected:

    // Executes any timers that are due. Returns number of milliseconds
    // to wait to match the next timer or 0 meaning "no timers"
    uint64_t execute_timers ();

  private:
    // Clock instance private to this I/O thread
    clock_t _clock;

    // List of active timers
    struct timer_info_t
    {
        slk::i_poll_events *sink;
        int id;
    };
    typedef std::multimap<uint64_t, timer_info_t> timers_t;
    timers_t _timers;

    // Load of the poller (currently the number of file descriptors registered)
    atomic_counter_t _load;

    SL_NON_COPYABLE_NOR_MOVABLE (poller_base_t)
};

// Base class for a poller with a single worker thread
class worker_poller_base_t : public poller_base_t
{
  public:
    worker_poller_base_t (ctx_t *ctx_);

    // Methods from the poller concept
    void start (const char *name = NULL);

  protected:
    // Checks whether the currently executing thread is the worker thread
    void check_thread () const;

    // Stops the worker thread (should be called from destructor of leaf class)
    void stop_worker ();

    // Flag to signal the worker thread to stop
    // Accessed by I/O thread in loop() and by other threads in stop_worker()
    // Using bool is safe here as it's set once and read repeatedly
    bool _stopping;

  private:
    // Main worker thread routine
    static void worker_routine (void *arg_);

    virtual void loop () = 0;

    // Reference to context
    ctx_t *_ctx;

    // Handle of the physical thread doing the I/O work
    thread_t _worker;
};
}

#endif

/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_THREAD_HPP_INCLUDED
#define SL_THREAD_HPP_INCLUDED

#ifndef _WIN32
#include <pthread.h>
#endif

#include <set>
#include <cstring>

#include "macros.hpp"

// Default thread priority and scheduling policy
#define SL_THREAD_PRIORITY_DFLT -1
#define SL_THREAD_SCHED_POLICY_DFLT -1

namespace slk {

typedef void (thread_fn)(void *);

// Class encapsulating OS thread. Thread initiation/termination is done
// using special functions rather than in constructor/destructor so that
// thread isn't created during object construction by accident.

class thread_t {
  public:
    thread_t() :
        _tfn(nullptr),
        _arg(nullptr),
        _started(false),
        _thread_priority(SL_THREAD_PRIORITY_DFLT),
        _thread_sched_policy(SL_THREAD_SCHED_POLICY_DFLT)
    {
        memset(_name, 0, sizeof(_name));
    }

    // Creates OS thread. 'tfn' is main thread function. It'll be passed
    // 'arg' as an argument. Name is 16 characters max including terminating NUL.
    void start(thread_fn *tfn, void *arg, const char *name = nullptr);

    // Returns whether the thread was started.
    bool get_started() const;

    // Returns whether the executing thread is the thread represented by this object.
    bool is_current_thread() const;

    // Waits for thread termination.
    void stop();

    // Sets the thread scheduling parameters.
    void setSchedulingParameters(int priority, int scheduling_policy,
                                  const std::set<int> &affinity_cpus);

    // These are internal members. They should be private, however then
    // they would not be accessible from the main C routine of the thread.
    void applySchedulingParameters();
    void applyThreadName();
    thread_fn *_tfn;
    void *_arg;
    char _name[16];

  private:
    bool _started;

#ifdef _WIN32
    void *_descriptor;  // HANDLE
    unsigned int _thread_id;
#else
    pthread_t _descriptor;
#endif

    // Thread scheduling parameters.
    int _thread_priority;
    int _thread_sched_policy;
    std::set<int> _thread_affinity_cpus;

    SL_NON_COPYABLE_NOR_MOVABLE(thread_t)
};

}  // namespace slk

#endif

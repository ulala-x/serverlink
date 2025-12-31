/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "thread.hpp"
#include "err.hpp"
#include "macros.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#else
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#endif

bool slk::thread_t::get_started() const
{
    return _started;
}

#ifdef _WIN32

extern "C" {
static unsigned int __stdcall thread_routine(void *arg)
{
    slk::thread_t *self = static_cast<slk::thread_t *>(arg);
    self->applyThreadName();
    self->_tfn(self->_arg);
    return 0;
}
}

void slk::thread_t::start(thread_fn *tfn, void *arg, const char *name)
{
    _tfn = tfn;
    _arg = arg;
    if (name)
        strncpy(_name, name, sizeof(_name) - 1);

    // Set default stack size to 4MB to avoid std::map stack overflow on x64
    unsigned int stack = 0;
#if defined(_WIN64)
    stack = 0x400000;
#endif

    _descriptor = (void *)_beginthreadex(nullptr, stack, &::thread_routine, this,
                                          0, &_thread_id);
    win_assert(_descriptor != nullptr);
    _started = true;
}

bool slk::thread_t::is_current_thread() const
{
    return GetCurrentThreadId() == _thread_id;
}

void slk::thread_t::stop()
{
    if (_started) {
        const DWORD rc = WaitForSingleObject((HANDLE)_descriptor, INFINITE);
        win_assert(rc != WAIT_FAILED);
        const BOOL rc2 = CloseHandle((HANDLE)_descriptor);
        win_assert(rc2 != 0);
    }
}

void slk::thread_t::setSchedulingParameters(
    int priority, int scheduling_policy, const std::set<int> &affinity_cpus)
{
    // Not implemented on Windows
    SL_UNUSED(priority);
    SL_UNUSED(scheduling_policy);
    SL_UNUSED(affinity_cpus);
}

void slk::thread_t::applySchedulingParameters()
{
    // Not implemented on Windows
}

void slk::thread_t::applyThreadName()
{
    // Thread naming on Windows requires debugger - skip for simplicity
}

#else  // POSIX

extern "C" {
static void *thread_routine(void *arg)
{
#if !defined(__ANDROID__)
    // Block all signals in the I/O thread for predictable latencies.
    sigset_t signal_set;
    int rc = sigfillset(&signal_set);
    errno_assert(rc == 0);
    rc = pthread_sigmask(SIG_BLOCK, &signal_set, nullptr);
    posix_assert(rc);
#endif
    slk::thread_t *self = static_cast<slk::thread_t *>(arg);
    self->applySchedulingParameters();
    self->applyThreadName();
    self->_tfn(self->_arg);
    return nullptr;
}
}

void slk::thread_t::start(thread_fn *tfn, void *arg, const char *name)
{
    _tfn = tfn;
    _arg = arg;
    if (name)
        strncpy(_name, name, sizeof(_name) - 1);
    int rc = pthread_create(&_descriptor, nullptr, ::thread_routine, this);
    posix_assert(rc);
    _started = true;
}

void slk::thread_t::stop()
{
    if (_started) {
        int rc = pthread_join(_descriptor, nullptr);
        posix_assert(rc);
    }
}

bool slk::thread_t::is_current_thread() const
{
    return pthread_equal(pthread_self(), _descriptor) != 0;
}

void slk::thread_t::setSchedulingParameters(
    int priority, int scheduling_policy, const std::set<int> &affinity_cpus)
{
    _thread_priority = priority;
    _thread_sched_policy = scheduling_policy;
    _thread_affinity_cpus = affinity_cpus;
}

void slk::thread_t::applySchedulingParameters()
{
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && _POSIX_THREAD_PRIORITY_SCHEDULING >= 0
    int policy = 0;
    struct sched_param param;

    int rc = pthread_getschedparam(pthread_self(), &policy, &param);
    posix_assert(rc);

    if (_thread_sched_policy != SL_THREAD_SCHED_POLICY_DFLT) {
        policy = _thread_sched_policy;
    }

    bool use_nice_instead_priority = (policy != SCHED_FIFO) && (policy != SCHED_RR);

    if (use_nice_instead_priority)
        param.sched_priority = 0;
    else if (_thread_priority != SL_THREAD_PRIORITY_DFLT)
        param.sched_priority = _thread_priority;

    rc = pthread_setschedparam(pthread_self(), policy, &param);

#if defined(__FreeBSD_kernel__) || defined(__FreeBSD__)
    if (rc == ENOSYS)
        return;
#endif

    posix_assert(rc);

#ifdef __linux__
    if (!_thread_affinity_cpus.empty()) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (auto it = _thread_affinity_cpus.begin(); it != _thread_affinity_cpus.end(); ++it) {
            CPU_SET(*it, &cpuset);
        }
        rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        posix_assert(rc);
    }
#endif
#endif
}

void slk::thread_t::applyThreadName()
{
    if (!_name[0])
        return;

#if defined(__APPLE__)
    pthread_setname_np(_name);
#elif defined(__linux__)
    pthread_setname_np(pthread_self(), _name);
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
    pthread_set_name_np(pthread_self(), _name);
#endif
}

#endif

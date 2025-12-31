/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq (C++11 simplified) */

#ifndef SL_CONDITION_VARIABLE_HPP_INCLUDED
#define SL_CONDITION_VARIABLE_HPP_INCLUDED

#include "err.hpp"
#include "macros.hpp"
#include "mutex.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace slk {

class condition_variable_t {
  public:
    condition_variable_t() { InitializeConditionVariable(&_cv); }

    int wait(mutex_t *mutex, int timeout)
    {
        int rc = SleepConditionVariableCS(&_cv, mutex->get_cs(), timeout);

        if (rc != 0)
            return 0;

        rc = GetLastError();

        if (rc != ERROR_TIMEOUT)
            win_assert(rc);

        errno = EAGAIN;
        return -1;
    }

    void broadcast() { WakeAllConditionVariable(&_cv); }

  private:
    CONDITION_VARIABLE _cv;

    SL_NON_COPYABLE_NOR_MOVABLE(condition_variable_t)
};

}  // namespace slk

#else  // POSIX

#include <pthread.h>
#include <time.h>

namespace slk {

class condition_variable_t {
  public:
    condition_variable_t()
    {
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
#if !defined(__APPLE__)
        pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
#endif
        int rc = pthread_cond_init(&_cond, &attr);
        posix_assert(rc);
        pthread_condattr_destroy(&attr);
    }

    ~condition_variable_t()
    {
        int rc = pthread_cond_destroy(&_cond);
        posix_assert(rc);
    }

    int wait(mutex_t *mutex, int timeout)
    {
        int rc;

        if (timeout != -1) {
            struct timespec ts;

#ifdef __APPLE__
            ts.tv_sec = timeout / 1000;
            ts.tv_nsec = (timeout % 1000) * 1000000;
            rc = pthread_cond_timedwait_relative_np(&_cond, mutex->get_mutex(), &ts);
#else
            rc = clock_gettime(CLOCK_MONOTONIC, &ts);
            posix_assert(rc);

            ts.tv_sec += timeout / 1000;
            ts.tv_nsec += (timeout % 1000) * 1000000;

            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }

            rc = pthread_cond_timedwait(&_cond, mutex->get_mutex(), &ts);
#endif
        } else {
            rc = pthread_cond_wait(&_cond, mutex->get_mutex());
        }

        if (rc == 0)
            return 0;

        if (rc == ETIMEDOUT) {
            errno = EAGAIN;
            return -1;
        }

        posix_assert(rc);
        return -1;
    }

    void broadcast()
    {
        int rc = pthread_cond_broadcast(&_cond);
        posix_assert(rc);
    }

  private:
    pthread_cond_t _cond;

    SL_NON_COPYABLE_NOR_MOVABLE(condition_variable_t)
};

}  // namespace slk

#endif

#endif

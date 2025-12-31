/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_MUTEX_HPP_INCLUDED
#define SL_MUTEX_HPP_INCLUDED

#include "err.hpp"
#include "macros.hpp"

// Mutex class encapsulates OS mutex in a platform-independent way.

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace slk {

class mutex_t {
  public:
    mutex_t() { InitializeCriticalSection(&_cs); }

    ~mutex_t() { DeleteCriticalSection(&_cs); }

    void lock() { EnterCriticalSection(&_cs); }

    bool try_lock() { return (TryEnterCriticalSection(&_cs)) ? true : false; }

    void unlock() { LeaveCriticalSection(&_cs); }

    CRITICAL_SECTION *get_cs() { return &_cs; }

  private:
    CRITICAL_SECTION _cs;

    SL_NON_COPYABLE_NOR_MOVABLE(mutex_t)
};

}  // namespace slk

#else

#include <pthread.h>

namespace slk {

class mutex_t {
  public:
    mutex_t() {
        int rc = pthread_mutexattr_init(&_attr);
        posix_assert(rc);

        rc = pthread_mutexattr_settype(&_attr, PTHREAD_MUTEX_RECURSIVE);
        posix_assert(rc);

        rc = pthread_mutex_init(&_mutex, &_attr);
        posix_assert(rc);
    }

    ~mutex_t() {
        int rc = pthread_mutex_destroy(&_mutex);
        posix_assert(rc);

        rc = pthread_mutexattr_destroy(&_attr);
        posix_assert(rc);
    }

    void lock() {
        int rc = pthread_mutex_lock(&_mutex);
        posix_assert(rc);
    }

    bool try_lock() {
        int rc = pthread_mutex_trylock(&_mutex);
        if (rc == EBUSY)
            return false;

        posix_assert(rc);
        return true;
    }

    void unlock() {
        int rc = pthread_mutex_unlock(&_mutex);
        posix_assert(rc);
    }

    pthread_mutex_t *get_mutex() { return &_mutex; }

  private:
    pthread_mutex_t _mutex;
    pthread_mutexattr_t _attr;

    SL_NON_COPYABLE_NOR_MOVABLE(mutex_t)
};

}  // namespace slk

#endif


namespace slk {

struct scoped_lock_t {
    scoped_lock_t(mutex_t &mutex) : _mutex(mutex) { _mutex.lock(); }

    ~scoped_lock_t() { _mutex.unlock(); }

  private:
    mutex_t &_mutex;

    SL_NON_COPYABLE_NOR_MOVABLE(scoped_lock_t)
};


struct scoped_optional_lock_t {
    scoped_optional_lock_t(mutex_t *mutex) : _mutex(mutex) {
        if (_mutex != nullptr)
            _mutex->lock();
    }

    ~scoped_optional_lock_t() {
        if (_mutex != nullptr)
            _mutex->unlock();
    }

  private:
    mutex_t *_mutex;

    SL_NON_COPYABLE_NOR_MOVABLE(scoped_optional_lock_t)
};

}  // namespace slk

#endif

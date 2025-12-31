/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "clock.hpp"
#include "config.hpp"
#include "err.hpp"
#include "likely.hpp"
#include "mutex.hpp"

#include <cstddef>

#ifdef _MSC_VER
#include <intrin.h>
#if defined(_M_ARM) || defined(_M_ARM64)
#include <arm_neon.h>
#endif
#endif

#ifndef _WIN32
#include <sys/time.h>
#endif

#if defined(HAVE_CLOCK_GETTIME) || defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <time.h>
#ifndef HAVE_CLOCK_GETTIME
#define HAVE_CLOCK_GETTIME
#endif
#endif

#if defined(__APPLE__)
int alt_clock_gettime(int clock_id, timespec *ts)
{
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), clock_id, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
    return 0;
}
#endif

#ifdef _WIN32
typedef ULONGLONG (*f_compatible_get_tick_count64)();

static slk::mutex_t compatible_get_tick_count64_mutex;

ULONGLONG compatible_get_tick_count64()
{
    slk::scoped_lock_t locker(compatible_get_tick_count64_mutex);

    static DWORD s_wrap = 0;
    static DWORD s_last_tick = 0;
    const DWORD current_tick = ::GetTickCount();

    if (current_tick < s_last_tick)
        ++s_wrap;

    s_last_tick = current_tick;
    const ULONGLONG result = (static_cast<ULONGLONG>(s_wrap) << 32)
                             + static_cast<ULONGLONG>(current_tick);

    return result;
}

f_compatible_get_tick_count64 init_compatible_get_tick_count64()
{
    f_compatible_get_tick_count64 func = nullptr;

    const HMODULE module = ::LoadLibraryA("Kernel32.dll");
    if (module != nullptr)
        func = reinterpret_cast<f_compatible_get_tick_count64>(
            ::GetProcAddress(module, "GetTickCount64"));

    if (func == nullptr)
        func = compatible_get_tick_count64;

    if (module != nullptr)
        ::FreeLibrary(module);

    return func;
}

static f_compatible_get_tick_count64 my_get_tick_count64 =
    init_compatible_get_tick_count64();
#endif

#ifndef _WIN32
const uint64_t usecs_per_msec = 1000;
const uint64_t nsecs_per_usec = 1000;
#endif
const uint64_t usecs_per_sec = 1000000;

slk::clock_t::clock_t() :
    _last_tsc(rdtsc()),
#ifdef _WIN32
    _last_time(static_cast<uint64_t>((*my_get_tick_count64)()))
#else
    _last_time(now_us() / usecs_per_msec)
#endif
{
}

uint64_t slk::clock_t::now_us()
{
#if defined(_WIN32)

    // Get the high resolution counter's accuracy.
    LARGE_INTEGER ticks_per_second;
    QueryPerformanceFrequency(&ticks_per_second);

    // What time is it?
    LARGE_INTEGER tick;
    QueryPerformanceCounter(&tick);

    // Convert the tick number into the number of seconds
    // since the system was started.
    const double ticks_div =
        static_cast<double>(ticks_per_second.QuadPart) / usecs_per_sec;
    return static_cast<uint64_t>(tick.QuadPart / ticks_div);

#elif defined(HAVE_CLOCK_GETTIME)

    // Use POSIX clock_gettime function to get precise monotonic time.
    struct timespec tv;

#if defined(__APPLE__) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101200
    int rc = alt_clock_gettime(SYSTEM_CLOCK, &tv);
#else
    int rc = clock_gettime(CLOCK_MONOTONIC, &tv);
#endif
    // Fix case where system has clock_gettime but CLOCK_MONOTONIC is not supported.
    if (rc != 0) {
        // Use POSIX gettimeofday function to get precise time.
        struct timeval tv2;
        rc = gettimeofday(&tv2, nullptr);
        errno_assert(rc == 0);
        return tv2.tv_sec * usecs_per_sec + tv2.tv_usec;
    }
    return tv.tv_sec * usecs_per_sec + tv.tv_nsec / nsecs_per_usec;

#else

    // Use POSIX gettimeofday function to get precise time.
    struct timeval tv;
    int rc = gettimeofday(&tv, nullptr);
    errno_assert(rc == 0);
    return tv.tv_sec * usecs_per_sec + tv.tv_usec;

#endif
}

uint64_t slk::clock_t::now_ms()
{
    const uint64_t tsc = rdtsc();

    // If TSC is not supported, get precise time and chop off the microseconds.
    if (!tsc) {
#ifdef _WIN32
        return static_cast<uint64_t>((*my_get_tick_count64)());
#else
        return now_us() / usecs_per_msec;
#endif
    }

    // If TSC haven't jumped back (in case of migration to a different
    // CPU core) and if not too much time elapsed since last measurement,
    // we can return cached time value.
    if (likely(tsc - _last_tsc <= (clock_precision / 2) && tsc >= _last_tsc))
        return _last_time;

    _last_tsc = tsc;
#ifdef _WIN32
    _last_time = static_cast<uint64_t>((*my_get_tick_count64)());
#else
    _last_time = now_us() / usecs_per_msec;
#endif
    return _last_time;
}

uint64_t slk::clock_t::rdtsc()
{
#if (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64)))
    return __rdtsc();
#elif defined(_MSC_VER) && defined(_M_ARM)
    return __rdpmccntr64();
#elif defined(_MSC_VER) && defined(_M_ARM64)
    const int64_t pmccntr_el0 = (((3 & 1) << 14) |
                                 ((3 & 7) << 11) |
                                 ((9 & 15) << 7) |
                                 ((13 & 15) << 3) |
                                 ((0 & 7) << 0));
    return _ReadStatusReg(pmccntr_el0);
#elif (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)))
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return static_cast<uint64_t>(high) << 32 | low;
#elif defined(__s390__)
    uint64_t tsc;
    asm("\tstck\t%0\n" : "=Q"(tsc) : : "cc");
    return tsc;
#else
    struct timespec ts;
#if defined(__APPLE__) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101200
    alt_clock_gettime(SYSTEM_CLOCK, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return static_cast<uint64_t>(ts.tv_sec) * nsecs_per_usec * usecs_per_sec
           + ts.tv_nsec;
#endif
}

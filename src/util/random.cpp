/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "random.hpp"
#include "clock.hpp"

#include <cstdlib>

#ifndef _WIN32
#include <unistd.h>
#endif

void slk::seed_random()
{
#ifdef _WIN32
    const int pid = static_cast<int>(GetCurrentProcessId());
#else
    int pid = static_cast<int>(getpid());
#endif
    srand(static_cast<unsigned int>(clock_t::now_us() + pid));
}

uint32_t slk::generate_random()
{
    // Compensate for the fact that rand() returns signed integer.
    const uint32_t low = static_cast<uint32_t>(rand());
    uint32_t high = static_cast<uint32_t>(rand());
    high <<= (sizeof(int) * 8 - 1);
    return high | low;
}

void slk::random_open()
{
    // No crypto library to initialize in ServerLink
}

void slk::random_close()
{
    // No crypto library to close in ServerLink
}

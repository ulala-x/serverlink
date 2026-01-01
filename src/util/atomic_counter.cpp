/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - C API wrapper for atomic_counter_t */

#include "atomic_counter.hpp"
#include <cstdlib>

namespace slk {

// C API wrapper functions for atomic counter

void* atomic_counter_new()
{
    return static_cast<void*>(new (std::nothrow) atomic_counter_t());
}

void atomic_counter_set(void* counter, int value)
{
    if (counter)
        static_cast<atomic_counter_t*>(counter)->set(static_cast<atomic_counter_t::integer_t>(value));
}

int atomic_counter_inc(void* counter)
{
    if (!counter)
        return 0;
    return static_cast<int>(static_cast<atomic_counter_t*>(counter)->add(1));
}

int atomic_counter_dec(void* counter)
{
    if (!counter)
        return 0;
    atomic_counter_t* ac = static_cast<atomic_counter_t*>(counter);
    const atomic_counter_t::integer_t old = ac->add(static_cast<atomic_counter_t::integer_t>(-1));
    return static_cast<int>(old - 1);
}

int atomic_counter_value(void* counter)
{
    if (!counter)
        return 0;
    return static_cast<int>(static_cast<atomic_counter_t*>(counter)->get());
}

void atomic_counter_destroy(void** counter_p)
{
    if (counter_p && *counter_p) {
        delete static_cast<atomic_counter_t*>(*counter_p);
        *counter_p = nullptr;
    }
}

} // namespace slk

/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Test atomic counter functionality */

#include <serverlink/serverlink.h>
#include <cassert>
#include <cstdio>

int main()
{
    printf("Testing atomic counters...\n");

    void *counter = slk_atomic_counter_new();
    assert(counter != nullptr);
    assert(slk_atomic_counter_value(counter) == 0);

    // Test increment
    assert(slk_atomic_counter_inc(counter) == 0);
    assert(slk_atomic_counter_inc(counter) == 1);
    assert(slk_atomic_counter_inc(counter) == 2);
    assert(slk_atomic_counter_value(counter) == 3);

    // Test decrement
    assert(slk_atomic_counter_dec(counter) == 2);
    assert(slk_atomic_counter_dec(counter) == 1);
    assert(slk_atomic_counter_dec(counter) == 0);

    // Test set
    slk_atomic_counter_set(counter, 100);
    assert(slk_atomic_counter_value(counter) == 100);

    slk_atomic_counter_set(counter, 2);
    assert(slk_atomic_counter_dec(counter) == 1);
    assert(slk_atomic_counter_dec(counter) == 0);

    slk_atomic_counter_destroy(&counter);
    assert(counter == nullptr);

    printf("PASSED\n");
    return 0;
}

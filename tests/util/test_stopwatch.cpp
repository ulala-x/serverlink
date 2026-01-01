/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Test stopwatch functionality */

#include <serverlink/serverlink.h>
#include <cassert>
#include <cstdio>

int main()
{
    printf("Testing stopwatch...\n");

    // Test basic stopwatch functionality
    void *watch = slk_stopwatch_start();
    assert(watch != nullptr);

    // Sleep for 50ms
    slk_sleep(50);

    unsigned long elapsed = slk_stopwatch_intermediate(watch);
    printf("Elapsed after 50ms sleep: %lu us\n", elapsed);

    // Should be at least 45ms (45000us) to account for timer inaccuracy
    assert(elapsed >= 45000);
    // Should be less than 100ms (100000us) - generous upper bound
    assert(elapsed < 100000);

    // Sleep for another 50ms
    slk_sleep(50);

    elapsed = slk_stopwatch_stop(watch);
    printf("Total elapsed after 100ms sleep: %lu us\n", elapsed);

    // Should be at least 90ms (90000us) to account for timer inaccuracy
    assert(elapsed >= 90000);
    // Should be less than 200ms (200000us) - generous upper bound
    assert(elapsed < 200000);

    // Test null stopwatch
    elapsed = slk_stopwatch_intermediate(nullptr);
    assert(elapsed == 0);

    elapsed = slk_stopwatch_stop(nullptr);
    assert(elapsed == 0);

    printf("PASSED\n");
    return 0;
}

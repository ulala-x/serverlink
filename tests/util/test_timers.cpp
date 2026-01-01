/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Test timer functionality */

#include <serverlink/serverlink.h>
#include <cassert>
#include <cstdio>
#include <cerrno>

static bool timer_invoked = false;

void handler(int timer_id, void *arg)
{
    (void)timer_id;
    *(static_cast<bool*>(arg)) = true;
}

int sleep_and_execute(void *timers)
{
    int timeout = slk_timers_timeout(timers);

    // Sleep methods are inaccurate, so we sleep in a loop until time arrived
    while (timeout > 0) {
        slk_sleep(timeout);
        timeout = slk_timers_timeout(timers);
    }

    return slk_timers_execute(timers);
}

void test_null_timer_pointers()
{
    printf("Testing null timer pointers...\n");

    void *timers = nullptr;

    // Destroy should fail with null
    assert(slk_timers_destroy(&timers) == -1);
    assert(errno == EFAULT);

    const size_t dummy_interval = 100;
    const int dummy_timer_id = 1;

    // Add should fail with null
    assert(slk_timers_add(timers, dummy_interval, &handler, nullptr) == -1);
    assert(errno == EFAULT);

    // Cancel should fail with null
    assert(slk_timers_cancel(timers, dummy_timer_id) == -1);
    assert(errno == EFAULT);

    // Set interval should fail with null
    assert(slk_timers_set_interval(timers, dummy_timer_id, dummy_interval) == -1);
    assert(errno == EFAULT);

    // Reset should fail with null
    assert(slk_timers_reset(timers, dummy_timer_id) == -1);
    assert(errno == EFAULT);

    // Timeout should fail with null
    assert(slk_timers_timeout(timers) == -1);
    assert(errno == EFAULT);

    // Execute should fail with null
    assert(slk_timers_execute(timers) == -1);
    assert(errno == EFAULT);

    printf("PASSED\n");
}

void test_corner_cases()
{
    printf("Testing corner cases...\n");

    void *timers = slk_timers_new();
    assert(timers != nullptr);

    const size_t dummy_interval = 100000;
    const int dummy_timer_id = 1;

    // Attempt to cancel non-existent timer
    assert(slk_timers_cancel(timers, dummy_timer_id) == -1);
    assert(errno == EINVAL);

    // Attempt to set interval of non-existent timer
    assert(slk_timers_set_interval(timers, dummy_timer_id, dummy_interval) == -1);
    assert(errno == EINVAL);

    // Attempt to reset non-existent timer
    assert(slk_timers_reset(timers, dummy_timer_id) == -1);
    assert(errno == EINVAL);

    // Attempt to add NULL handler
    assert(slk_timers_add(timers, dummy_interval, nullptr, nullptr) == -1);
    assert(errno == EFAULT);

    const int timer_id = slk_timers_add(timers, dummy_interval, handler, nullptr);
    assert(timer_id >= 0);

    // Attempt to cancel timer twice
    assert(slk_timers_cancel(timers, timer_id) == 0);
    assert(slk_timers_cancel(timers, timer_id) == -1);
    assert(errno == EINVAL);

    // Timeout without any active timers
    assert(slk_timers_timeout(timers) == -1);

    // Cleanup
    assert(slk_timers_destroy(&timers) == 0);
    assert(timers == nullptr);

    printf("PASSED\n");
}

void test_timers()
{
    printf("Testing timers...\n");

    void *timers = slk_timers_new();
    assert(timers != nullptr);

    timer_invoked = false;

    const unsigned long full_timeout = 100;
    void *stopwatch = slk_stopwatch_start();

    const int timer_id = slk_timers_add(timers, full_timeout, handler, &timer_invoked);
    assert(timer_id >= 0);

    // Timer should not have been invoked yet
    assert(slk_timers_execute(timers) == 0);

    if (slk_stopwatch_intermediate(stopwatch) < full_timeout * 1000) {
        assert(!timer_invoked);
    }

    // Wait half the time and check again
    long timeout = slk_timers_timeout(timers);
    assert(timeout >= 0);
    slk_sleep(timeout / 2);
    assert(slk_timers_execute(timers) == 0);
    if (slk_stopwatch_intermediate(stopwatch) < full_timeout * 1000) {
        assert(!timer_invoked);
    }

    // Wait until the end
    assert(sleep_and_execute(timers) == 0);
    assert(timer_invoked);
    timer_invoked = false;

    // Wait half the time and check again
    timeout = slk_timers_timeout(timers);
    assert(timeout >= 0);
    slk_sleep(timeout / 2);
    assert(slk_timers_execute(timers) == 0);
    if (slk_stopwatch_intermediate(stopwatch) < 2 * full_timeout * 1000) {
        assert(!timer_invoked);
    }

    // Reset timer and wait half of the time left
    assert(slk_timers_reset(timers, timer_id) == 0);
    slk_sleep(timeout / 2);
    assert(slk_timers_execute(timers) == 0);
    if (slk_stopwatch_stop(stopwatch) < 2 * full_timeout * 1000) {
        assert(!timer_invoked);
    }

    // Wait until the end
    assert(sleep_and_execute(timers) == 0);
    assert(timer_invoked);
    timer_invoked = false;

    // Reschedule
    assert(slk_timers_set_interval(timers, timer_id, 50) == 0);
    assert(sleep_and_execute(timers) == 0);
    assert(timer_invoked);
    timer_invoked = false;

    // Cancel timer
    timeout = slk_timers_timeout(timers);
    assert(timeout >= 0);
    assert(slk_timers_cancel(timers, timer_id) == 0);
    slk_sleep(timeout * 2);
    assert(slk_timers_execute(timers) == 0);
    assert(!timer_invoked);

    assert(slk_timers_destroy(&timers) == 0);
    assert(timers == nullptr);

    printf("PASSED\n");
}

int main()
{
    printf("Testing timer API...\n");

    test_null_timer_pointers();
    test_corner_cases();
    test_timers();

    printf("\nAll timer tests PASSED\n");
    return 0;
}

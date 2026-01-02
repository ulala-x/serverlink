/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq test_ctx_options.cpp */

#include "../testutil.hpp"
#include <limits.h>

#ifdef __linux__
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sched.h>

#define TEST_POLICY (SCHED_OTHER) // Default Linux scheduler

static bool is_allowed_to_raise_priority()
{
    // NOTE: if setrlimit() fails with EPERM, current user doesn't have enough permissions
    // Even for privileged users (e.g., root) getrlimit() would usually return 0 as nice limit
    // The only way to discover if the user can increase the nice value is to try
    struct rlimit rlim;
    rlim.rlim_cur = 40;
    rlim.rlim_max = 40;
    if (setrlimit(RLIMIT_NICE, &rlim) == 0) {
        // rlim_cur == 40 means this process is allowed to set a nice value of -20
        return true;
    }
    return false;
}

#else

#define TEST_POLICY (0)

static bool is_allowed_to_raise_priority()
{
    return false;
}

#endif

// Test IO_THREADS option
static void test_io_threads()
{
    slk_ctx_t *ctx = test_context_new();
    int value;
    size_t len = sizeof(value);

    // Default value should be 1
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_IO_THREADS, &value, &len));
    TEST_ASSERT_EQ(value, 1);

    // Set to 4 threads
    value = 4;
    TEST_SUCCESS(slk_ctx_set(ctx, SLK_IO_THREADS, &value, sizeof(value)));

    // Verify it was set
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_IO_THREADS, &value, &len));
    TEST_ASSERT_EQ(value, 4);

    // Setting to 0 should succeed (means no I/O threads)
    value = 0;
    TEST_SUCCESS(slk_ctx_set(ctx, SLK_IO_THREADS, &value, sizeof(value)));
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_IO_THREADS, &value, &len));
    TEST_ASSERT_EQ(value, 0);

    // Negative values should fail
    value = -1;
    TEST_FAILURE(slk_ctx_set(ctx, SLK_IO_THREADS, &value, sizeof(value)));

    test_context_destroy(ctx);
}

// Test MAX_SOCKETS option
static void test_max_sockets()
{
    slk_ctx_t *ctx = test_context_new();
    int value;
    size_t len = sizeof(value);

    // Get the current max_sockets value
    // This is platform-dependent:
    // - On Windows with select: FD_SETSIZE=64, but clipped_maxsocket applies -1 twice
    //   (once for socket_limit, once for max_sockets), so we get 63
    // - On Linux with epoll: typically 1023
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_MAX_SOCKETS, &value, &len));
    int current_max = value;

    // Verify it's a reasonable positive value
    TEST_ASSERT(current_max > 0 && current_max <= 65535);

    // Set to different value (but within platform limits)
    int new_value = current_max / 2;  // Use half of current max
    if (new_value < 1) new_value = 1;
    value = new_value;
    TEST_SUCCESS(slk_ctx_set(ctx, SLK_MAX_SOCKETS, &value, sizeof(value)));
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_MAX_SOCKETS, &value, &len));
    TEST_ASSERT_EQ(value, new_value);

    // Setting to 0 or negative should fail
    value = 0;
    TEST_FAILURE(slk_ctx_set(ctx, SLK_MAX_SOCKETS, &value, sizeof(value)));

    value = -1;
    TEST_FAILURE(slk_ctx_set(ctx, SLK_MAX_SOCKETS, &value, sizeof(value)));

    test_context_destroy(ctx);
}

// Test SOCKET_LIMIT option (read-only)
static void test_socket_limit()
{
    slk_ctx_t *ctx = test_context_new();
    int value;
    size_t len = sizeof(value);

    // Should be able to get the value
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_SOCKET_LIMIT, &value, &len));
    // Value depends on platform but should be positive
    TEST_ASSERT(value > 0);
    printf("  Socket limit: %d\n", value);

    test_context_destroy(ctx);
}

// Test THREAD_SCHED_POLICY option
static void test_thread_sched_policy()
{
    slk_ctx_t *ctx = test_context_new();
    int value;
    size_t len = sizeof(value);

    // Default value should be -1 (not set)
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_THREAD_SCHED_POLICY, &value, &len));
    TEST_ASSERT_EQ(value, -1);

    // Set to TEST_POLICY
    value = TEST_POLICY;
    TEST_SUCCESS(slk_ctx_set(ctx, SLK_THREAD_SCHED_POLICY, &value, sizeof(value)));
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_THREAD_SCHED_POLICY, &value, &len));
    TEST_ASSERT_EQ(value, TEST_POLICY);

    // Setting default value (-1) should fail
    value = -1;
    TEST_FAILURE(slk_ctx_set(ctx, SLK_THREAD_SCHED_POLICY, &value, sizeof(value)));

    test_context_destroy(ctx);
}

// Test THREAD_PRIORITY option
static void test_thread_priority()
{
    slk_ctx_t *ctx = test_context_new();
    int value;
    size_t len = sizeof(value);

    // Default value should be -1 (not set)
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_THREAD_PRIORITY, &value, &len));
    TEST_ASSERT_EQ(value, -1);

    // Setting priority requires appropriate permissions
    if (is_allowed_to_raise_priority()) {
        value = 1;
        TEST_SUCCESS(slk_ctx_set(ctx, SLK_THREAD_PRIORITY, &value, sizeof(value)));
        TEST_SUCCESS(slk_ctx_get(ctx, SLK_THREAD_PRIORITY, &value, &len));
        TEST_ASSERT_EQ(value, 1);
        printf("  Priority setting allowed (have permissions)\n");
    } else {
        printf("  Priority setting skipped (no permissions)\n");
    }

    // Setting default value (-1) should fail
    value = -1;
    TEST_FAILURE(slk_ctx_set(ctx, SLK_THREAD_PRIORITY, &value, sizeof(value)));

    test_context_destroy(ctx);
}

// Test THREAD_AFFINITY_CPU_ADD and REMOVE options
static void test_thread_affinity()
{
    slk_ctx_t *ctx = test_context_new();
    int value;

    // Add CPU 0 to affinity
    value = 0;
    TEST_SUCCESS(slk_ctx_set(ctx, SLK_THREAD_AFFINITY_CPU_ADD, &value, sizeof(value)));

    // Add CPU 1 to affinity
    value = 1;
    TEST_SUCCESS(slk_ctx_set(ctx, SLK_THREAD_AFFINITY_CPU_ADD, &value, sizeof(value)));

    // Remove CPU 1 from affinity
    value = 1;
    TEST_SUCCESS(slk_ctx_set(ctx, SLK_THREAD_AFFINITY_CPU_REMOVE, &value, sizeof(value)));

    // Removing a CPU that's not in the set should fail
    value = 2;
    TEST_FAILURE(slk_ctx_set(ctx, SLK_THREAD_AFFINITY_CPU_REMOVE, &value, sizeof(value)));

    // Negative CPU values should fail
    value = -1;
    TEST_FAILURE(slk_ctx_set(ctx, SLK_THREAD_AFFINITY_CPU_ADD, &value, sizeof(value)));

    test_context_destroy(ctx);
}

// Test THREAD_NAME_PREFIX option
static void test_thread_name_prefix()
{
    slk_ctx_t *ctx = test_context_new();

    // Set string prefix
    const char prefix[] = "MyPrefix";
    TEST_SUCCESS(slk_ctx_set(ctx, SLK_THREAD_NAME_PREFIX, prefix, sizeof(prefix)));

    // Get string prefix
    char buf[32];
    size_t len = sizeof(buf);
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_THREAD_NAME_PREFIX, buf, &len));
    TEST_ASSERT_STR_EQ(buf, prefix);

    // Prefix longer than 16 chars should fail
    const char long_prefix[] = "ThisIsAVeryLongPrefixThatExceeds16Chars";
    TEST_FAILURE(slk_ctx_set(ctx, SLK_THREAD_NAME_PREFIX, long_prefix, sizeof(long_prefix)));

    test_context_destroy(ctx);
}

// Test MAX_MSGSZ option
static void test_max_msgsz()
{
    slk_ctx_t *ctx = test_context_new();
    int value;
    size_t len = sizeof(value);

    // Default value should be INT_MAX
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_MAX_MSGSZ, &value, &len));
    TEST_ASSERT_EQ(value, INT_MAX);

    // Set to 1MB
    value = 1024 * 1024;
    TEST_SUCCESS(slk_ctx_set(ctx, SLK_MAX_MSGSZ, &value, sizeof(value)));
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_MAX_MSGSZ, &value, &len));
    TEST_ASSERT_EQ(value, 1024 * 1024);

    // Setting to 0 should succeed (means no limit after conversion to INT_MAX)
    value = 0;
    TEST_SUCCESS(slk_ctx_set(ctx, SLK_MAX_MSGSZ, &value, sizeof(value)));

    // Negative values should fail
    value = -1;
    TEST_FAILURE(slk_ctx_set(ctx, SLK_MAX_MSGSZ, &value, sizeof(value)));

    test_context_destroy(ctx);
}

// Test MSG_T_SIZE option (read-only)
static void test_msg_t_size()
{
    slk_ctx_t *ctx = test_context_new();
    int value;
    size_t len = sizeof(value);

    // Should be able to get the value
    TEST_SUCCESS(slk_ctx_get(ctx, SLK_MSG_T_SIZE, &value, &len));
    // Value should be positive and reasonable
    TEST_ASSERT(value > 0);
    TEST_ASSERT(value < 1024); // msg_t should be less than 1KB
    printf("  msg_t size: %d bytes\n", value);

    test_context_destroy(ctx);
}

// Test invalid option
static void test_invalid_option()
{
    slk_ctx_t *ctx = test_context_new();
    int value = 0;
    size_t len = sizeof(value);

    // Invalid option for get should fail
    TEST_FAILURE(slk_ctx_get(ctx, -1, &value, &len));

    // Invalid option for set should fail
    TEST_FAILURE(slk_ctx_set(ctx, -1, &value, sizeof(value)));

    test_context_destroy(ctx);
}

// Test that context options can be set before creating sockets
static void test_options_before_sockets()
{
    slk_ctx_t *ctx = test_context_new();

    // Set options
    int value = 2;
    TEST_SUCCESS(slk_ctx_set(ctx, SLK_IO_THREADS, &value, sizeof(value)));

    // Create socket - should work
    slk_socket_t *socket = test_socket_new(ctx, SLK_ROUTER);
    TEST_ASSERT_NOT_NULL(socket);

    test_socket_close(socket);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink Context Options Tests ===\n\n");

    RUN_TEST(test_io_threads);
    RUN_TEST(test_max_sockets);
    RUN_TEST(test_socket_limit);
    RUN_TEST(test_thread_sched_policy);
    RUN_TEST(test_thread_priority);
    RUN_TEST(test_thread_affinity);
    RUN_TEST(test_thread_name_prefix);
    RUN_TEST(test_max_msgsz);
    RUN_TEST(test_msg_t_size);
    RUN_TEST(test_invalid_option);
    RUN_TEST(test_options_before_sockets);

    printf("\n=== All Context Options Tests Passed ===\n");
    return 0;
}

/* ServerLink Context Unit Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/* Test: Create and destroy context */
static void test_ctx_create_destroy()
{
    slk_ctx_t *ctx = slk_ctx_new();
    TEST_ASSERT_NOT_NULL(ctx);

    slk_ctx_destroy(ctx);
}

/* Test: Create socket from context */
static void test_ctx_socket()
{
    fprintf(stderr, "[TEST] test_ctx_socket: creating context\n");
    slk_ctx_t *ctx = test_context_new();

    fprintf(stderr, "[TEST] test_ctx_socket: creating socket\n");
    slk_socket_t *s = slk_socket(ctx, SLK_ROUTER);
    TEST_ASSERT_NOT_NULL(s);

    fprintf(stderr, "[TEST] test_ctx_socket: closing socket\n");
    test_socket_close(s);
    fprintf(stderr, "[TEST] test_ctx_socket: socket closed\n");

    fprintf(stderr, "[TEST] test_ctx_socket: destroying context\n");
    test_context_destroy(ctx);
    fprintf(stderr, "[TEST] test_ctx_socket: context destroyed\n");
}

/* Test: Multiple sockets from same context */
static void test_ctx_multiple_sockets()
{
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *s1 = test_socket_new(ctx, SLK_ROUTER);
    slk_socket_t *s2 = test_socket_new(ctx, SLK_ROUTER);
    slk_socket_t *s3 = test_socket_new(ctx, SLK_ROUTER);

    TEST_ASSERT_NEQ(s1, s2);
    TEST_ASSERT_NEQ(s2, s3);
    TEST_ASSERT_NEQ(s1, s3);

    test_socket_close(s1);
    test_socket_close(s2);
    test_socket_close(s3);
    test_context_destroy(ctx);
}

/* Test: Invalid socket type */
static void test_ctx_invalid_socket_type()
{
    slk_ctx_t *ctx = test_context_new();

    /* Try to create socket with invalid type */
    slk_socket_t *s = slk_socket(ctx, 999);
    TEST_ASSERT_NULL(s);

    test_context_destroy(ctx);
}

/* Test: Close socket before destroying context */
static void test_ctx_socket_close_order()
{
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *s = test_socket_new(ctx, SLK_ROUTER);

    /* Close socket first */
    test_socket_close(s);

    /* Then destroy context */
    test_context_destroy(ctx);
}

/* Test: Destroy context with open sockets (should handle gracefully) */
static void test_ctx_destroy_with_open_sockets()
{
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *s1 = test_socket_new(ctx, SLK_ROUTER);
    slk_socket_t *s2 = test_socket_new(ctx, SLK_ROUTER);

    /* Destroy context without closing sockets explicitly */
    /* The library should handle this gracefully */
    slk_ctx_destroy(ctx);

    /* Note: Don't close sockets after context is destroyed */
}

/* Test: Create multiple contexts */
static void test_multiple_contexts()
{
    slk_ctx_t *ctx1 = test_context_new();
    slk_ctx_t *ctx2 = test_context_new();
    slk_ctx_t *ctx3 = test_context_new();

    TEST_ASSERT_NEQ(ctx1, ctx2);
    TEST_ASSERT_NEQ(ctx2, ctx3);
    TEST_ASSERT_NEQ(ctx1, ctx3);

    test_context_destroy(ctx1);
    test_context_destroy(ctx2);
    test_context_destroy(ctx3);
}

/* Test: Socket from one context, close after context destroyed */
static void test_socket_outlive_context()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *s = test_socket_new(ctx, SLK_ROUTER);

    /* Destroy context first */
    slk_ctx_destroy(ctx);

    /* Socket operations may fail but shouldn't crash */
    /* Just don't try to close the socket as it's already cleaned up */
}

/* Test: Version information */
static void test_version()
{
    int major = 0, minor = 0, patch = 0;

    slk_version(&major, &minor, &patch);

    /* Version should be set */
    TEST_ASSERT_EQ(major, SLK_VERSION_MAJOR);
    TEST_ASSERT_EQ(minor, SLK_VERSION_MINOR);
    TEST_ASSERT_EQ(patch, SLK_VERSION_PATCH);

    printf("  ServerLink version: %d.%d.%d\n", major, minor, patch);
}

/* Test: Context with NULL pointer */
static void test_ctx_null_operations()
{
    /* Creating socket with NULL context should fail */
    slk_socket_t *s = slk_socket(NULL, SLK_ROUTER);
    TEST_ASSERT_NULL(s);

    /* Destroying NULL context should not crash */
    slk_ctx_destroy(NULL);
}

int main()
{
    printf("=== ServerLink Context Unit Tests ===\n\n");

    RUN_TEST(test_ctx_create_destroy);
    RUN_TEST(test_ctx_socket);
    RUN_TEST(test_ctx_multiple_sockets);
    RUN_TEST(test_ctx_invalid_socket_type);
    RUN_TEST(test_ctx_socket_close_order);
    RUN_TEST(test_ctx_destroy_with_open_sockets);
    RUN_TEST(test_multiple_contexts);
    RUN_TEST(test_socket_outlive_context);
    RUN_TEST(test_version);
    RUN_TEST(test_ctx_null_operations);

    printf("\n=== All Context Tests Passed ===\n");
    return 0;
}

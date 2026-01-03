/* ServerLink Socket Option HWM Tests - Simplified */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/*
 * Note: Original libzmq test used PUSH/PULL sockets.
 * ServerLink only supports ROUTER, so we test HWM option get/set behavior.
 * Complex message flow tests are in other test files.
 */

/* Test: Set and get SNDHWM option */
static void test_sndhwm_option()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *socket = test_socket_new(ctx, SLK_ROUTER);

    /* Default value check */
    int val = 0;
    size_t len = sizeof(val);
    int rc = slk_getsockopt(socket, SLK_SNDHWM, &val, &len);
    TEST_SUCCESS(rc);
    TEST_ASSERT(val > 0);  /* Should have a default value */
    printf("  Default SNDHWM: %d\n", val);

    /* Set new value */
    val = 100;
    rc = slk_setsockopt(socket, SLK_SNDHWM, &val, sizeof(val));
    TEST_SUCCESS(rc);

    /* Verify new value */
    val = 0;
    len = sizeof(val);
    rc = slk_getsockopt(socket, SLK_SNDHWM, &val, &len);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(val, 100);

    /* Change to another value */
    val = 50;
    rc = slk_setsockopt(socket, SLK_SNDHWM, &val, sizeof(val));
    TEST_SUCCESS(rc);

    val = 0;
    len = sizeof(val);
    rc = slk_getsockopt(socket, SLK_SNDHWM, &val, &len);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(val, 50);

    test_socket_close(socket);
    test_context_destroy(ctx);
}

/* Test: Set and get RCVHWM option */
static void test_rcvhwm_option()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *socket = test_socket_new(ctx, SLK_ROUTER);

    /* Default value check */
    int val = 0;
    size_t len = sizeof(val);
    int rc = slk_getsockopt(socket, SLK_RCVHWM, &val, &len);
    TEST_SUCCESS(rc);
    TEST_ASSERT(val > 0);  /* Should have a default value */
    printf("  Default RCVHWM: %d\n", val);

    /* Set new value */
    val = 200;
    rc = slk_setsockopt(socket, SLK_RCVHWM, &val, sizeof(val));
    TEST_SUCCESS(rc);

    /* Verify new value */
    val = 0;
    len = sizeof(val);
    rc = slk_getsockopt(socket, SLK_RCVHWM, &val, &len);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(val, 200);

    test_socket_close(socket);
    test_context_destroy(ctx);
}

/* Test: HWM option persists after bind */
static void test_hwm_after_bind()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *socket = test_socket_new(ctx, SLK_ROUTER);

    /* Set HWM before bind */
    int val = 42;
    int rc = slk_setsockopt(socket, SLK_SNDHWM, &val, sizeof(val));
    TEST_SUCCESS(rc);

    val = 24;
    rc = slk_setsockopt(socket, SLK_RCVHWM, &val, sizeof(val));
    TEST_SUCCESS(rc);

    /* Bind socket */
    test_socket_bind(socket, "inproc://hwm_test");

    /* Wait for bind to complete */
    test_sleep_ms(50);

    /* Verify values persist after bind */
    val = 0;
    size_t len = sizeof(val);
    rc = slk_getsockopt(socket, SLK_SNDHWM, &val, &len);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(val, 42);

    val = 0;
    len = sizeof(val);
    rc = slk_getsockopt(socket, SLK_RCVHWM, &val, &len);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(val, 24);

    test_socket_close(socket);
    test_context_destroy(ctx);
}

/* Test: HWM can be changed after connection */
static void test_hwm_change_after_connect()
{
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *bind_socket = test_socket_new(ctx, SLK_ROUTER);
    slk_socket_t *connect_socket = test_socket_new(ctx, SLK_ROUTER);

    /* Set initial HWM */
    int val = 10;
    int rc = slk_setsockopt(connect_socket, SLK_SNDHWM, &val, sizeof(val));
    TEST_SUCCESS(rc);

    /* Connect sockets */
    test_socket_bind(bind_socket, "inproc://hwm_change");
    test_socket_connect(connect_socket, "inproc://hwm_change");

    test_sleep_ms(50);

    /* Change HWM after connection */
    val = 20;
    rc = slk_setsockopt(connect_socket, SLK_SNDHWM, &val, sizeof(val));
    TEST_SUCCESS(rc);

    /* Verify change */
    val = 0;
    size_t len = sizeof(val);
    rc = slk_getsockopt(connect_socket, SLK_SNDHWM, &val, &len);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(val, 20);

    test_socket_close(bind_socket);
    test_socket_close(connect_socket);
    test_context_destroy(ctx);
}

/* Test: HWM with zero value (unlimited) */
static void test_hwm_zero()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *socket = test_socket_new(ctx, SLK_ROUTER);

    /* Set HWM to zero (unlimited) */
    int val = 0;
    int rc = slk_setsockopt(socket, SLK_SNDHWM, &val, sizeof(val));
    TEST_SUCCESS(rc);

    /* Verify */
    val = -1;
    size_t len = sizeof(val);
    rc = slk_getsockopt(socket, SLK_SNDHWM, &val, &len);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(val, 0);

    test_socket_close(socket);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink Socket Option HWM Tests ===\n\n");

    RUN_TEST(test_sndhwm_option);
    RUN_TEST(test_rcvhwm_option);
    RUN_TEST(test_hwm_after_bind);
    RUN_TEST(test_hwm_change_after_connect);
    RUN_TEST(test_hwm_zero);

    printf("\n=== All Socket Option HWM Tests Passed ===\n");
    return 0;
}

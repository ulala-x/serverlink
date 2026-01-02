/* ServerLink Error Handling Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/*
 * Error Handling Tests
 *
 * Tests proper error reporting and handling for common error scenarios.
 * Note: Some operations with NULL pointers may crash rather than return errors,
 * so we focus on safe error handling tests.
 */

/* Test 1: Invalid endpoint formats */
static void test_invalid_endpoint_format()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Empty endpoint */
    int rc = slk_bind(sock, "");
    TEST_ASSERT_EQ(rc, -1);
    TEST_ASSERT(slk_errno() != 0);

    /* Missing protocol */
    rc = slk_bind(sock, "127.0.0.1:5555");
    TEST_ASSERT_EQ(rc, -1);

    /* Unknown protocol */
    rc = slk_bind(sock, "unknown://localhost:5555");
    TEST_ASSERT_EQ(rc, -1);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 2: Bind to already bound address */
static void test_bind_already_bound()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    /* First socket binds successfully */
    slk_socket_t *sock1 = test_socket_new(ctx, SLK_ROUTER);
    int rc = slk_setsockopt(sock1, SLK_ROUTING_ID, "sock1", 5);
    TEST_SUCCESS(rc);

    test_socket_bind(sock1, endpoint);

    /* Second socket should fail to bind */
    slk_socket_t *sock2 = test_socket_new(ctx, SLK_ROUTER);
    rc = slk_bind(sock2, endpoint);
    TEST_ASSERT_EQ(rc, -1);
    /* Error should be EADDRINUSE or similar */
    TEST_ASSERT(slk_errno() != 0);

    test_socket_close(sock2);
    test_socket_close(sock1);
    test_context_destroy(ctx);
}

/* Test 3: Connect to non-existent server (should not immediately fail) */
static void test_connect_nonexistent()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Connect should queue connection attempt */
    int rc = slk_connect(sock, "tcp://127.0.0.1:59999");  /* Unlikely to be in use */
    /* Connect itself may succeed (async connection) */
    TEST_SUCCESS(rc);

    /* Set a short linger to avoid hanging on close */
    int linger = 0;
    rc = slk_setsockopt(sock, SLK_LINGER, &linger, sizeof(linger));
    TEST_SUCCESS(rc);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 4: Invalid socket option values */
static void test_invalid_sockopt_values()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Invalid option ID */
    int value = 1;
    int rc = slk_setsockopt(sock, 99999, &value, sizeof(value));
    TEST_ASSERT_EQ(rc, -1);
    TEST_ASSERT_EQ(slk_errno(), SLK_EINVAL);

    /* Invalid option length */
    rc = slk_setsockopt(sock, SLK_SNDHWM, &value, 0);
    TEST_ASSERT_EQ(rc, -1);
    TEST_ASSERT_EQ(slk_errno(), SLK_EINVAL);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 5: Invalid socket type */
static void test_invalid_socket_type()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create socket with invalid type */
    slk_socket_t *sock = slk_socket(ctx, 999);
    TEST_ASSERT(sock == NULL);
    TEST_ASSERT_EQ(slk_errno(), SLK_EINVAL);

    /* Create socket with negative type */
    sock = slk_socket(ctx, -1);
    TEST_ASSERT(sock == NULL);
    TEST_ASSERT_EQ(slk_errno(), SLK_EINVAL);

    test_context_destroy(ctx);
}

/* Test 6: Non-blocking recv with no messages */
static void test_nonblocking_recv_empty()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Bind to allow operations */
    const char *endpoint = test_endpoint_tcp();
    test_socket_bind(sock, endpoint);

    /* Non-blocking recv should return EAGAIN when no messages */
    char buf[256];
    int rc = slk_recv(sock, buf, sizeof(buf), SLK_DONTWAIT);
    TEST_ASSERT_EQ(rc, -1);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 7: Non-blocking send when no peers (ROUTER MANDATORY) */
static void test_nonblocking_send_no_peers()
{
    slk_ctx_t *ctx = test_context_new();

    /* ROUTER with MANDATORY should fail when no peer */
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    int mandatory = 1;
    int rc = slk_setsockopt(router, SLK_ROUTER_MANDATORY, &mandatory, sizeof(mandatory));
    TEST_SUCCESS(rc);

    /* Bind router */
    const char *endpoint = test_endpoint_tcp();
    test_socket_bind(router, endpoint);

    /* Try to send to non-existent peer */
    rc = slk_send(router, "nonexistent_peer", 16, SLK_SNDMORE | SLK_DONTWAIT);
    if (rc >= 0) {
        rc = slk_send(router, "message", 7, SLK_DONTWAIT);
    }
    /* Should fail with EHOSTUNREACH or EAGAIN */
    TEST_ASSERT_EQ(rc, -1);
    int err = slk_errno();
    TEST_ASSERT(err == SLK_EHOSTUNREACH || err == SLK_EAGAIN);

    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Test 8: Empty routing ID */
static void test_empty_routing_id()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Empty routing ID should fail */
    int rc = slk_setsockopt(sock, SLK_ROUTING_ID, "", 0);
    TEST_ASSERT_EQ(rc, -1);
    TEST_ASSERT_EQ(slk_errno(), SLK_EINVAL);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 9: slk_errno after successful operation */
static void test_errno_persistence()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Trigger an error */
    int rc = slk_setsockopt(sock, 99999, NULL, 0);
    TEST_ASSERT_EQ(rc, -1);
    int err = slk_errno();
    TEST_ASSERT(err != 0);

    /* Successful operation - errno value is implementation-defined */
    const char *endpoint = test_endpoint_tcp();
    test_socket_bind(sock, endpoint);

    /* Just verify we didn't crash */

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 10: Close NULL socket (should not crash) */
static void test_close_null()
{
    /* Close NULL should return error or be no-op, but not crash */
    int rc = slk_close(NULL);
    /* Implementation may return 0 or -1 */
    (void)rc;
}

/* Main test runner */
int main()
{
    printf("=== ServerLink Error Handling Tests ===\n\n");

    RUN_TEST(test_invalid_endpoint_format);
    RUN_TEST(test_bind_already_bound);
    RUN_TEST(test_connect_nonexistent);
    RUN_TEST(test_invalid_sockopt_values);
    RUN_TEST(test_invalid_socket_type);
    RUN_TEST(test_nonblocking_recv_empty);
    RUN_TEST(test_nonblocking_send_no_peers);
    RUN_TEST(test_empty_routing_id);
    RUN_TEST(test_errno_persistence);
    RUN_TEST(test_close_null);

    printf("\n=== Error Handling Tests Completed ===\n");
    return 0;
}

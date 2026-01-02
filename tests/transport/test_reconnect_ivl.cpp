/* ServerLink Reconnect Interval Option Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/*
 * Reconnect Interval Tests
 *
 * Tests the SLK_RECONNECT_IVL and SLK_RECONNECT_IVL_MAX options
 * for controlling automatic reconnection behavior.
 *
 * Note: Full reconnect behavior testing (unbind/rebind cycles) requires
 * careful timing and may be flaky. These tests focus on option setting/getting
 * and basic connection functionality with the options set.
 */

/* Test 1: SLK_RECONNECT_IVL option setting and getting */
static void test_reconnect_ivl_option()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Get default value */
    int ivl = -999;
    size_t optlen = sizeof(ivl);
    int rc = slk_getsockopt(sock, SLK_RECONNECT_IVL, &ivl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(optlen, sizeof(int));
    /* Default is implementation-specific, just verify we got a value */
    printf("  Default RECONNECT_IVL: %d ms\n", ivl);

    /* Set to 1000ms (1 second) */
    ivl = 1000;
    rc = slk_setsockopt(sock, SLK_RECONNECT_IVL, &ivl, sizeof(ivl));
    TEST_SUCCESS(rc);

    /* Verify */
    ivl = -999;
    optlen = sizeof(ivl);
    rc = slk_getsockopt(sock, SLK_RECONNECT_IVL, &ivl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ivl, 1000);

    /* Set to -1 (disable reconnect) */
    ivl = -1;
    rc = slk_setsockopt(sock, SLK_RECONNECT_IVL, &ivl, sizeof(ivl));
    TEST_SUCCESS(rc);

    ivl = -999;
    optlen = sizeof(ivl);
    rc = slk_getsockopt(sock, SLK_RECONNECT_IVL, &ivl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ivl, -1);

    /* Set to 0 (immediate reconnect) */
    ivl = 0;
    rc = slk_setsockopt(sock, SLK_RECONNECT_IVL, &ivl, sizeof(ivl));
    TEST_SUCCESS(rc);

    ivl = -999;
    optlen = sizeof(ivl);
    rc = slk_getsockopt(sock, SLK_RECONNECT_IVL, &ivl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ivl, 0);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 2: SLK_RECONNECT_IVL_MAX option setting and getting */
static void test_reconnect_ivl_max_option()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Get default value */
    int ivl_max = -999;
    size_t optlen = sizeof(ivl_max);
    int rc = slk_getsockopt(sock, SLK_RECONNECT_IVL_MAX, &ivl_max, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(optlen, sizeof(int));
    printf("  Default RECONNECT_IVL_MAX: %d ms\n", ivl_max);

    /* Set to 30000ms (30 seconds) */
    ivl_max = 30000;
    rc = slk_setsockopt(sock, SLK_RECONNECT_IVL_MAX, &ivl_max, sizeof(ivl_max));
    TEST_SUCCESS(rc);

    /* Verify */
    ivl_max = -999;
    optlen = sizeof(ivl_max);
    rc = slk_getsockopt(sock, SLK_RECONNECT_IVL_MAX, &ivl_max, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ivl_max, 30000);

    /* Set to 0 (disable exponential backoff) */
    ivl_max = 0;
    rc = slk_setsockopt(sock, SLK_RECONNECT_IVL_MAX, &ivl_max, sizeof(ivl_max));
    TEST_SUCCESS(rc);

    ivl_max = -999;
    optlen = sizeof(ivl_max);
    rc = slk_getsockopt(sock, SLK_RECONNECT_IVL_MAX, &ivl_max, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ivl_max, 0);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 3: Connection works with reconnect interval set */
static void test_connection_with_reconnect_ivl()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    /* Server */
    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    int rc = slk_setsockopt(server, SLK_ROUTING_ID, "server", 6);
    TEST_SUCCESS(rc);
    test_socket_bind(server, endpoint);

    /* Client with reconnect settings */
    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);

    /* Set reconnect interval */
    int ivl = 500;
    rc = slk_setsockopt(client, SLK_RECONNECT_IVL, &ivl, sizeof(ivl));
    TEST_SUCCESS(rc);

    int ivl_max = 5000;
    rc = slk_setsockopt(client, SLK_RECONNECT_IVL_MAX, &ivl_max, sizeof(ivl_max));
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(client, SLK_ROUTING_ID, "client", 6);
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(client, SLK_CONNECT_ROUTING_ID, "server", 6);
    TEST_SUCCESS(rc);

    test_socket_connect(client, endpoint);
    test_sleep_ms(100);

    /* Perform a simple handshake */
    rc = slk_send(client, "server", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(client, "HELLO", 5, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* Server receives */
    char buf[256];
    rc = slk_recv(server, buf, sizeof(buf), 0);
    TEST_ASSERT(rc > 0);
    int rid_len = rc;
    char rid[256];
    memcpy(rid, buf, rid_len);

    rc = slk_recv(server, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 5);
    TEST_ASSERT_MEM_EQ(buf, "HELLO", 5);

    /* Server responds */
    rc = slk_send(server, rid, rid_len, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(server, "READY", 5, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* Client receives */
    rc = slk_recv(client, buf, sizeof(buf), 0);
    TEST_ASSERT(rc > 0);
    rc = slk_recv(client, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 5);
    TEST_ASSERT_MEM_EQ(buf, "READY", 5);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test 4: Different socket types support reconnect options */
static void test_reconnect_ivl_socket_types()
{
    slk_ctx_t *ctx = test_context_new();
    int socket_types[] = {SLK_ROUTER, SLK_PUB, SLK_SUB, SLK_PAIR};
    const char *socket_names[] = {"ROUTER", "PUB", "SUB", "PAIR"};
    int num_types = sizeof(socket_types) / sizeof(socket_types[0]);

    for (int i = 0; i < num_types; i++) {
        slk_socket_t *sock = test_socket_new(ctx, socket_types[i]);

        int ivl = 2000;
        int rc = slk_setsockopt(sock, SLK_RECONNECT_IVL, &ivl, sizeof(ivl));
        if (rc != 0) {
            printf("  NOTE: %s socket may not support RECONNECT_IVL\n",
                   socket_names[i]);
            test_socket_close(sock);
            continue;
        }

        /* Verify */
        ivl = -999;
        size_t optlen = sizeof(ivl);
        rc = slk_getsockopt(sock, SLK_RECONNECT_IVL, &ivl, &optlen);
        TEST_SUCCESS(rc);
        TEST_ASSERT_EQ(ivl, 2000);

        test_socket_close(sock);
    }

    test_context_destroy(ctx);
}

/* Test 5: Reconnect options before and after connect */
static void test_reconnect_ivl_timing()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    /* Server */
    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_socket_bind(server, endpoint);

    /* Client - set reconnect BEFORE connect */
    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);

    int ivl = 1500;
    int rc = slk_setsockopt(client, SLK_RECONNECT_IVL, &ivl, sizeof(ivl));
    TEST_SUCCESS(rc);

    test_socket_connect(client, endpoint);

    /* Verify option still correct after connect */
    ivl = -999;
    size_t optlen = sizeof(ivl);
    rc = slk_getsockopt(client, SLK_RECONNECT_IVL, &ivl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ivl, 1500);

    /* Set linger for clean shutdown */
    int linger = 0;
    slk_setsockopt(client, SLK_LINGER, &linger, sizeof(linger));
    slk_setsockopt(server, SLK_LINGER, &linger, sizeof(linger));

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Main test runner */
int main()
{
    printf("=== ServerLink Reconnect Interval Tests ===\n\n");

    RUN_TEST(test_reconnect_ivl_option);
    RUN_TEST(test_reconnect_ivl_max_option);
    RUN_TEST(test_connection_with_reconnect_ivl);
    RUN_TEST(test_reconnect_ivl_socket_types);
    RUN_TEST(test_reconnect_ivl_timing);

    printf("\n=== Reconnect Interval Tests Completed ===\n");
    return 0;
}

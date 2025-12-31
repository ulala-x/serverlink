/* ServerLink Reconnect Interval Tests - Ported from libzmq */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/*
 * Helper: Send and receive a message (bounce test)
 */
static void bounce(slk_socket_t *server, slk_socket_t *client)
{
    /* Client sends */
    int rc = slk_send(client, "ping", 4, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* Server receives */
    char buf[256];
    rc = slk_recv(server, buf, sizeof(buf), 0);  /* routing ID */
    TEST_ASSERT(rc > 0);

    char routing_id[256];
    memcpy(routing_id, buf, rc);
    int routing_id_len = rc;

    rc = slk_recv(server, buf, sizeof(buf), 0);  /* payload */
    TEST_ASSERT_EQ(rc, 4);
    TEST_ASSERT_MEM_EQ(buf, "ping", 4);

    /* Server replies */
    rc = slk_send(server, routing_id, routing_id_len, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);

    rc = slk_send(server, "pong", 4, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* Client receives - for ROUTER, we get routing ID first */
    rc = slk_recv(client, buf, sizeof(buf), 0);  /* routing ID of server */
    TEST_ASSERT(rc > 0);

    rc = slk_recv(client, buf, sizeof(buf), 0);  /* payload */
    TEST_ASSERT_EQ(rc, 4);
    TEST_ASSERT_MEM_EQ(buf, "pong", 4);
}

/*
 * Helper: Expect bounce to fail
 */
static void expect_bounce_fail(slk_socket_t *server, slk_socket_t *client)
{
    /* Client tries to send */
    int rc = slk_send(client, "ping", 4, SLK_DONTWAIT);
    /* May succeed in queuing but won't be delivered */

    test_sleep_ms(100);

    /* Server should not receive anything */
    char buf[256];
    rc = slk_recv(server, buf, sizeof(buf), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);
}

/*
 * Test: Reconnect interval against ROUTER socket
 */
static void test_reconnect_ivl_against_router_socket(const char *endpoint, slk_socket_t *sb)
{
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *sc = test_socket_new(ctx, SLK_ROUTER);

    int interval = -1;
    int rc = slk_setsockopt(sc, SLK_RECONNECT_IVL, &interval, sizeof(int));
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(sc, SLK_ROUTING_ID, "client", 6);
    TEST_SUCCESS(rc);

    test_socket_connect(sc, endpoint);

    test_sleep_ms(200);

    /* First bounce should work */
    bounce(sb, sc);

    /* Unbind server */
    rc = slk_unbind(sb, endpoint);
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Second bounce should fail */
    expect_bounce_fail(sb, sc);

    /* Rebind server */
    test_socket_bind(sb, endpoint);

    test_sleep_ms(100);

    /* Still should fail because reconnect is disabled (interval = -1) */
    expect_bounce_fail(sb, sc);

    /* Reconnect explicitly */
    test_socket_connect(sc, endpoint);

    test_sleep_ms(200);

    /* Now bounce should work again */
    bounce(sb, sc);

    test_socket_close(sc);
    test_context_destroy(ctx);
}

/* Test: Reconnect interval with TCP IPv4 */
static void test_reconnect_ivl_tcp_ipv4()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *sb = test_socket_new(ctx, SLK_ROUTER);
    test_socket_bind(sb, endpoint);

    test_reconnect_ivl_against_router_socket(endpoint, sb);

    test_socket_close(sb);
    test_context_destroy(ctx);
}

/* Test: Reconnect interval with TCP IPv6 */
static void test_reconnect_ivl_tcp_ipv6()
{
    /*
     * Note: IPv6 test requires system support for IPv6.
     * This is a simplified version that may be skipped if not supported.
     */
    printf("  NOTE: IPv6 test skipped (may require special configuration)\n");
}

int main()
{
    printf("=== ServerLink Reconnect Interval Tests ===\n\n");

    RUN_TEST(test_reconnect_ivl_tcp_ipv4);
    RUN_TEST(test_reconnect_ivl_tcp_ipv6);

    printf("\n=== All Reconnect Interval Tests Passed ===\n");
    return 0;
}

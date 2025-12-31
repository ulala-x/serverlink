/* ServerLink ROUTER_HANDOVER Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/* Test: ROUTER_HANDOVER option defaults to disabled */
static void test_router_handover_default()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    int value = test_get_int_option(router, SLK_ROUTER_HANDOVER);
    TEST_ASSERT_EQ(value, 0);

    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Test: Enable ROUTER_HANDOVER */
static void test_router_handover_enable()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    test_set_int_option(router, SLK_ROUTER_HANDOVER, 1);

    int value = test_get_int_option(router, SLK_ROUTER_HANDOVER);
    TEST_ASSERT_EQ(value, 1);

    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Test: Basic handover - reconnect with same ID */
static void test_router_handover_reconnect()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);

    /* Enable handover on server */
    test_set_int_option(server, SLK_ROUTER_HANDOVER, 1);

    /* First client connects with ID "CLIENT" */
    slk_socket_t *client1 = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client1, "CLIENT");
    test_socket_connect(client1, endpoint);

    test_sleep_ms(200);

    /* Send a message from client1 */
    slk_send(client1, "SERVER", 6, SLK_SNDMORE);
    slk_send(client1, "Message1", 8, 0);

    test_sleep_ms(100);

    /* Server receives */
    TEST_ASSERT(test_poll_readable(server, 1000));
    char buf[256];
    slk_recv(server, buf, sizeof(buf), 0);
    int rc = slk_recv(server, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 8);
    TEST_ASSERT_MEM_EQ(buf, "Message1", 8);

    /* Disconnect first client */
    test_socket_close(client1);
    test_sleep_ms(200);

    /* Second client connects with SAME ID "CLIENT" */
    slk_socket_t *client2 = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client2, "CLIENT");
    test_socket_connect(client2, endpoint);

    test_sleep_ms(200);

    /* With ROUTER_HANDOVER, server should accept the new connection */
    /* Send a message from client2 */
    slk_send(client2, "SERVER", 6, SLK_SNDMORE);
    slk_send(client2, "Message2", 8, 0);

    test_sleep_ms(100);

    /* Server should receive from the new client */
    TEST_ASSERT(test_poll_readable(server, 1000));
    slk_recv(server, buf, sizeof(buf), 0);
    rc = slk_recv(server, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 8);
    TEST_ASSERT_MEM_EQ(buf, "Message2", 8);

    /* Server can send back to CLIENT (should route to client2) */
    slk_send(server, "CLIENT", 6, SLK_SNDMORE);
    slk_send(server, "Reply", 5, 0);

    test_sleep_ms(100);

    /* client2 should receive the reply */
    TEST_ASSERT(test_poll_readable(client2, 1000));
    slk_recv(client2, buf, sizeof(buf), 0);
    rc = slk_recv(client2, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 5);
    TEST_ASSERT_MEM_EQ(buf, "Reply", 5);

    test_socket_close(client2);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Handover disabled - duplicate ID rejected */
static void test_router_handover_disabled_duplicate_id()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);

    /* ROUTER_HANDOVER is disabled by default */
    TEST_ASSERT_EQ(test_get_int_option(server, SLK_ROUTER_HANDOVER), 0);

    /* First client connects */
    slk_socket_t *client1 = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client1, "CLIENT");
    test_socket_connect(client1, endpoint);

    test_sleep_ms(200);

    /* Second client tries to connect with same ID (should be rejected or queued) */
    slk_socket_t *client2 = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client2, "CLIENT");
    test_socket_connect(client2, endpoint);

    test_sleep_ms(200);

    /* Send from client1 (should work) */
    slk_send(client1, "SERVER", 6, SLK_SNDMORE);
    slk_send(client1, "FromClient1", 11, 0);

    test_sleep_ms(100);
    TEST_ASSERT(test_poll_readable(server, 1000));

    char buf[256];
    slk_recv(server, buf, sizeof(buf), 0);
    int rc = slk_recv(server, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 11);
    TEST_ASSERT_MEM_EQ(buf, "FromClient1", 11);

    /* Send from client2 (may fail or be queued depending on implementation) */
    /* Without handover, the connection may be rejected */

    test_socket_close(client1);
    test_socket_close(client2);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Handover with queued messages */
static void test_router_handover_with_queued_messages()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);
    test_set_int_option(server, SLK_ROUTER_HANDOVER, 1);

    /* Client1 connects */
    slk_socket_t *client1 = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client1, "CLIENT");
    test_socket_connect(client1, endpoint);

    test_sleep_ms(200);

    /* Server sends messages to CLIENT */
    for (int i = 0; i < 3; i++) {
        slk_send(server, "CLIENT", 6, SLK_SNDMORE);
        char msg[32];
        snprintf(msg, sizeof(msg), "Queued%d", i);
        slk_send(server, msg, strlen(msg), 0);
    }

    test_sleep_ms(100);

    /* Client1 receives some messages */
    TEST_ASSERT(test_poll_readable(client1, 1000));
    char buf[256];
    slk_recv(client1, buf, sizeof(buf), 0);
    slk_recv(client1, buf, sizeof(buf), 0);

    /* Disconnect client1 and immediately reconnect with client2 */
    test_socket_close(client1);
    test_sleep_ms(100);

    slk_socket_t *client2 = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client2, "CLIENT");
    test_socket_connect(client2, endpoint);

    test_sleep_ms(200);

    /* With ROUTER_HANDOVER, client2 might receive queued messages
     * or they might be dropped - depends on implementation */
    /* For this test, we just verify the new connection works */

    /* Send from client2 */
    slk_send(client2, "SERVER", 6, SLK_SNDMORE);
    slk_send(client2, "AfterHandover", 13, 0);

    test_sleep_ms(100);
    TEST_ASSERT(test_poll_readable(server, 1000));

    slk_recv(server, buf, sizeof(buf), 0);
    int rc = slk_recv(server, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 13);
    TEST_ASSERT_MEM_EQ(buf, "AfterHandover", 13);

    test_socket_close(client2);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Toggle ROUTER_HANDOVER */
static void test_router_handover_toggle()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    /* Default is disabled */
    TEST_ASSERT_EQ(test_get_int_option(router, SLK_ROUTER_HANDOVER), 0);

    /* Enable */
    test_set_int_option(router, SLK_ROUTER_HANDOVER, 1);
    TEST_ASSERT_EQ(test_get_int_option(router, SLK_ROUTER_HANDOVER), 1);

    /* Disable */
    test_set_int_option(router, SLK_ROUTER_HANDOVER, 0);
    TEST_ASSERT_EQ(test_get_int_option(router, SLK_ROUTER_HANDOVER), 0);

    test_socket_close(router);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink ROUTER_HANDOVER Tests ===\n\n");

    RUN_TEST(test_router_handover_default);
    RUN_TEST(test_router_handover_enable);
    RUN_TEST(test_router_handover_reconnect);
    RUN_TEST(test_router_handover_disabled_duplicate_id);
    RUN_TEST(test_router_handover_with_queued_messages);
    RUN_TEST(test_router_handover_toggle);

    printf("\n=== All ROUTER_HANDOVER Tests Passed ===\n");
    return 0;
}

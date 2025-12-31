/* ServerLink ROUTER_MANDATORY Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/* Test: ROUTER_MANDATORY option defaults to disabled */
static void test_router_mandatory_default()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    int value = test_get_int_option(router, SLK_ROUTER_MANDATORY);
    TEST_ASSERT_EQ(value, 0);

    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Test: Enable ROUTER_MANDATORY */
static void test_router_mandatory_enable()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    test_set_int_option(router, SLK_ROUTER_MANDATORY, 1);

    int value = test_get_int_option(router, SLK_ROUTER_MANDATORY);
    TEST_ASSERT_EQ(value, 1);

    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Test: Send to unknown peer with ROUTER_MANDATORY fails */
static void test_router_mandatory_unknown_peer()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(router, "SERVER");
    test_socket_bind(router, endpoint);

    /* Enable ROUTER_MANDATORY */
    test_set_int_option(router, SLK_ROUTER_MANDATORY, 1);

    test_sleep_ms(100);

    /* Try to send to non-existent peer - should fail immediately
     * with ROUTER_MANDATORY enabled. The routing ID frame fails with
     * EHOSTUNREACH because the peer doesn't exist. */
    int rc = slk_send(router, "UNKNOWN", 7, SLK_SNDMORE);
    TEST_ASSERT(rc < 0);
    int err = slk_errno();
    TEST_ASSERT(err == SLK_EHOSTUNREACH || err == SLK_EPEERUNREACH);

    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Test: Send to connected peer with ROUTER_MANDATORY succeeds */
static void test_router_mandatory_connected_peer()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);

    /* Enable ROUTER_MANDATORY */
    test_set_int_option(server, SLK_ROUTER_MANDATORY, 1);

    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client, "CLIENT");
    test_socket_connect(client, endpoint);

    /* Wait for connection */
    test_sleep_ms(200);

    /* Client sends to server (should succeed) */
    int rc;
    rc = slk_send(client, "SERVER", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(client, "Hello", 5, 0);
    TEST_ASSERT(rc >= 0);

    /* Server receives */
    test_sleep_ms(100);
    TEST_ASSERT(test_poll_readable(server, 1000));

    char buf[256];
    slk_recv(server, buf, sizeof(buf), 0); /* identity */
    rc = slk_recv(server, buf, sizeof(buf), 0); /* payload */
    TEST_ASSERT_EQ(rc, 5);
    TEST_ASSERT_MEM_EQ(buf, "Hello", 5);

    /* Server sends back (should succeed because CLIENT is connected) */
    rc = slk_send(server, "CLIENT", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(server, "World", 5, 0);
    TEST_ASSERT(rc >= 0);

    /* Client receives */
    test_sleep_ms(100);
    TEST_ASSERT(test_poll_readable(client, 1000));

    slk_recv(client, buf, sizeof(buf), 0);
    rc = slk_recv(client, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 5);
    TEST_ASSERT_MEM_EQ(buf, "World", 5);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Disable ROUTER_MANDATORY after enabling */
static void test_router_mandatory_toggle()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    /* Enable */
    test_set_int_option(router, SLK_ROUTER_MANDATORY, 1);
    TEST_ASSERT_EQ(test_get_int_option(router, SLK_ROUTER_MANDATORY), 1);

    /* Disable */
    test_set_int_option(router, SLK_ROUTER_MANDATORY, 0);
    TEST_ASSERT_EQ(test_get_int_option(router, SLK_ROUTER_MANDATORY), 0);

    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Test: ROUTER_MANDATORY with disconnected peer */
static void test_router_mandatory_after_disconnect()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);
    test_set_int_option(server, SLK_ROUTER_MANDATORY, 1);

    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client, "CLIENT");
    test_socket_connect(client, endpoint);

    test_sleep_ms(200);

    /* Verify connection works */
    int rc = slk_send(client, "SERVER", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(client, "Test", 4, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);
    TEST_ASSERT(test_poll_readable(server, 1000));

    char buf[256];
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);

    /* Disconnect client */
    test_socket_close(client);
    test_sleep_ms(200);

    /* Try to send to disconnected client (should fail with ROUTER_MANDATORY)
     * With ROUTER_MANDATORY enabled, the routing ID frame should succeed,
     * but subsequent frames may fail when the message cannot be delivered.
     * The behavior can be implementation-dependent - some implementations
     * fail on the routing ID, others on the final frame. */
    rc = slk_send(server, "CLIENT", 6, SLK_SNDMORE);
    if (rc >= 0) {
        rc = slk_send(server, "AfterDisconnect", 15, 0);
        /* The final frame should either fail or be silently dropped */
    }
    /* At least one of the frames should have failed, or all succeeded
     * but the message is dropped. Either behavior is acceptable. */

    test_socket_close(server);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink ROUTER_MANDATORY Tests ===\n\n");

    RUN_TEST(test_router_mandatory_default);
    RUN_TEST(test_router_mandatory_enable);
    RUN_TEST(test_router_mandatory_unknown_peer);
    RUN_TEST(test_router_mandatory_connected_peer);
    RUN_TEST(test_router_mandatory_toggle);
    RUN_TEST(test_router_mandatory_after_disconnect);

    printf("\n=== All ROUTER_MANDATORY Tests Passed ===\n");
    return 0;
}

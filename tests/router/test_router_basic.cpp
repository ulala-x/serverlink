/* ServerLink Basic ROUTER Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/* Test: Create ROUTER socket */
static void test_router_create()
{
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);
    TEST_ASSERT_NOT_NULL(router);

    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Test: Bind ROUTER socket */
static void test_router_bind()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    const char *endpoint = test_endpoint_tcp();
    test_socket_bind(router, endpoint);

    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Test: Connect ROUTER socket */
static void test_router_connect()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create server */
    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    const char *endpoint = test_endpoint_tcp();
    test_socket_bind(server, endpoint);

    /* Create client */
    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);
    test_socket_connect(client, endpoint);

    /* Give connection time to establish */
    test_sleep_ms(100);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Set routing ID */
static void test_router_routing_id()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    const char *id = "MYROUTER";
    test_set_routing_id(router, id);

    /* Verify we can read it back */
    char buffer[256];
    size_t len = sizeof(buffer);
    int rc = slk_getsockopt(router, SLK_ROUTING_ID, buffer, &len);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(len, strlen(id));
    TEST_ASSERT_MEM_EQ(buffer, id, len);

    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Test: ROUTER-to-ROUTER basic send/receive */
static void test_router_to_router_basic()
{
    slk_ctx_t *ctx = test_context_new();

    const char *endpoint = test_endpoint_tcp();

    /* Create server ROUTER */
    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);

    /* Create client ROUTER */
    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client, "CLIENT");
    test_socket_connect(client, endpoint);

    /* Wait for connection */
    test_sleep_ms(200);

    /* Client sends to server: [SERVER][empty][payload] */
    slk_send(client, "SERVER", 6, SLK_SNDMORE);
    slk_send(client, "", 0, SLK_SNDMORE);
    slk_send(client, "Hello", 5, 0);

    /* Server receives */
    test_sleep_ms(100);
    TEST_ASSERT(test_poll_readable(server, 1000));

    char identity[256];
    char empty[256];
    char payload[256];

    int rc;
    rc = slk_recv(server, identity, sizeof(identity), 0);
    TEST_ASSERT(rc > 0);

    rc = slk_recv(server, empty, sizeof(empty), 0);
    TEST_ASSERT_EQ(rc, 0);

    rc = slk_recv(server, payload, sizeof(payload), 0);
    TEST_ASSERT_EQ(rc, 5);
    TEST_ASSERT_MEM_EQ(payload, "Hello", 5);

    /* Server sends back: [CLIENT][empty][reply] */
    slk_send(server, "CLIENT", 6, SLK_SNDMORE);
    slk_send(server, "", 0, SLK_SNDMORE);
    slk_send(server, "World", 5, 0);

    /* Client receives */
    test_sleep_ms(100);
    TEST_ASSERT(test_poll_readable(client, 1000));

    rc = slk_recv(client, identity, sizeof(identity), 0);
    TEST_ASSERT(rc > 0);

    rc = slk_recv(client, empty, sizeof(empty), 0);
    TEST_ASSERT_EQ(rc, 0);

    rc = slk_recv(client, payload, sizeof(payload), 0);
    TEST_ASSERT_EQ(rc, 5);
    TEST_ASSERT_MEM_EQ(payload, "World", 5);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Multiple messages */
static void test_router_multiple_messages()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);

    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client, "CLIENT");
    test_socket_connect(client, endpoint);

    test_sleep_ms(200);

    /* Send multiple messages */
    for (int i = 0; i < 10; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Message %d", i);

        slk_send(client, "SERVER", 6, SLK_SNDMORE);
        slk_send(client, "", 0, SLK_SNDMORE);
        slk_send(client, msg, strlen(msg), 0);
    }

    /* Receive all messages */
    test_sleep_ms(200);

    for (int i = 0; i < 10; i++) {
        TEST_ASSERT(test_poll_readable(server, 1000));

        char identity[256], empty[256], payload[256];
        char expected[32];
        snprintf(expected, sizeof(expected), "Message %d", i);

        slk_recv(server, identity, sizeof(identity), 0);
        slk_recv(server, empty, sizeof(empty), 0);
        int rc = slk_recv(server, payload, sizeof(payload), 0);

        payload[rc] = '\0';
        TEST_ASSERT_STR_EQ(payload, expected);
    }

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Bidirectional communication */
static void test_router_bidirectional()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);

    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client, "CLIENT");
    test_socket_connect(client, endpoint);

    test_sleep_ms(200);

    /* Exchange messages back and forth */
    for (int i = 0; i < 5; i++) {
        /* Client -> Server */
        slk_send(client, "SERVER", 6, SLK_SNDMORE);
        slk_send(client, "", 0, SLK_SNDMORE);
        slk_send(client, "PING", 4, 0);

        test_sleep_ms(50);
        TEST_ASSERT(test_poll_readable(server, 1000));

        char buf[256];
        slk_recv(server, buf, sizeof(buf), 0);
        slk_recv(server, buf, sizeof(buf), 0);
        int rc = slk_recv(server, buf, sizeof(buf), 0);
        TEST_ASSERT_EQ(rc, 4);
        TEST_ASSERT_MEM_EQ(buf, "PING", 4);

        /* Server -> Client */
        slk_send(server, "CLIENT", 6, SLK_SNDMORE);
        slk_send(server, "", 0, SLK_SNDMORE);
        slk_send(server, "PONG", 4, 0);

        test_sleep_ms(50);
        TEST_ASSERT(test_poll_readable(client, 1000));

        slk_recv(client, buf, sizeof(buf), 0);
        slk_recv(client, buf, sizeof(buf), 0);
        rc = slk_recv(client, buf, sizeof(buf), 0);
        TEST_ASSERT_EQ(rc, 4);
        TEST_ASSERT_MEM_EQ(buf, "PONG", 4);
    }

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Disconnect and cleanup */
static void test_router_disconnect()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_socket_bind(server, endpoint);

    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);
    test_socket_connect(client, endpoint);

    test_sleep_ms(200);

    /* Disconnect client */
    int rc = slk_disconnect(client, endpoint);
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink Basic ROUTER Tests ===\n\n");

    RUN_TEST(test_router_create);
    RUN_TEST(test_router_bind);
    RUN_TEST(test_router_connect);
    RUN_TEST(test_router_routing_id);
    RUN_TEST(test_router_to_router_basic);
    RUN_TEST(test_router_multiple_messages);
    RUN_TEST(test_router_bidirectional);
    RUN_TEST(test_router_disconnect);

    printf("\n=== All Basic ROUTER Tests Passed ===\n");
    return 0;
}

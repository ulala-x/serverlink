/* ServerLink Router-to-Router Integration Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"
#include <stdio.h>

/* Test: Basic Router-to-Router communication */
static void test_router_to_router_basic()
{
    printf("  Testing basic Router-to-Router communication...\n");

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

    /* Client sends to server: [SERVER][data] */
    printf("  Client -> Server: Sending message\n");
    slk_send(client, "SERVER", 6, SLK_SNDMORE);
    slk_send(client, "Hello from client", 17, 0);

    /* Server receives */
    test_sleep_ms(100);
    TEST_ASSERT(test_poll_readable(server, 2000));

    char identity[256], payload[256];
    int rc;

    rc = slk_recv(server, identity, sizeof(identity), 0);
    TEST_ASSERT(rc > 0);
    printf("  Server received identity: %.*s (len=%d)\n", rc, identity, rc);

    rc = slk_recv(server, payload, sizeof(payload), 0);
    TEST_ASSERT_EQ(rc, 17);
    printf("  Server received payload: %.*s\n", rc, payload);
    TEST_ASSERT_MEM_EQ(payload, "Hello from client", 17);

    /* Server sends back: [CLIENT][data] */
    printf("  Server -> Client: Sending reply\n");
    slk_send(server, "CLIENT", 6, SLK_SNDMORE);
    slk_send(server, "Hello from server", 17, 0);

    /* Client receives */
    test_sleep_ms(100);
    TEST_ASSERT(test_poll_readable(client, 2000));

    rc = slk_recv(client, identity, sizeof(identity), 0);
    TEST_ASSERT(rc > 0);
    printf("  Client received identity: %.*s (len=%d)\n", rc, identity, rc);

    rc = slk_recv(client, payload, sizeof(payload), 0);
    TEST_ASSERT_EQ(rc, 17);
    printf("  Client received payload: %.*s\n", rc, payload);
    TEST_ASSERT_MEM_EQ(payload, "Hello from server", 17);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Multiple clients to one server */
static void test_router_multiple_clients()
{
    printf("  Testing multiple clients...\n");

    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    /* Create server */
    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);

    /* Create multiple clients */
    slk_socket_t *client1 = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client1, "CLIENT1");
    test_socket_connect(client1, endpoint);

    slk_socket_t *client2 = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client2, "CLIENT2");
    test_socket_connect(client2, endpoint);

    slk_socket_t *client3 = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client3, "CLIENT3");
    test_socket_connect(client3, endpoint);

    test_sleep_ms(300);

    /* All clients send messages */
    printf("  Clients sending messages...\n");
    slk_send(client1, "SERVER", 6, SLK_SNDMORE);
    slk_send(client1, "From CLIENT1", 12, 0);

    slk_send(client2, "SERVER", 6, SLK_SNDMORE);
    slk_send(client2, "From CLIENT2", 12, 0);

    slk_send(client3, "SERVER", 6, SLK_SNDMORE);
    slk_send(client3, "From CLIENT3", 12, 0);

    test_sleep_ms(200);

    /* Server receives all three messages */
    printf("  Server receiving messages...\n");
    char identity[256], buf[256];
    int received_count = 0;

    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(test_poll_readable(server, 2000));

        int rc = slk_recv(server, identity, sizeof(identity), 0);
        TEST_ASSERT(rc > 0);
        identity[rc] = '\0';

        rc = slk_recv(server, buf, sizeof(buf), 0);
        buf[rc] = '\0';

        printf("  Server received from %s: %s\n", identity, buf);
        received_count++;

        /* Send reply back to specific client */
        slk_send(server, identity, strlen(identity), SLK_SNDMORE);
        char reply[256];
        snprintf(reply, sizeof(reply), "Reply to %s", identity);
        slk_send(server, reply, strlen(reply), 0);
    }

    TEST_ASSERT_EQ(received_count, 3);

    test_sleep_ms(200);

    /* Each client receives its reply */
    printf("  Clients receiving replies...\n");
    TEST_ASSERT(test_poll_readable(client1, 2000));
    slk_recv(client1, buf, sizeof(buf), 0);
    int rc = slk_recv(client1, buf, sizeof(buf), 0);
    buf[rc] = '\0';
    printf("  CLIENT1 received: %s\n", buf);

    TEST_ASSERT(test_poll_readable(client2, 2000));
    slk_recv(client2, buf, sizeof(buf), 0);
    rc = slk_recv(client2, buf, sizeof(buf), 0);
    buf[rc] = '\0';
    printf("  CLIENT2 received: %s\n", buf);

    TEST_ASSERT(test_poll_readable(client3, 2000));
    slk_recv(client3, buf, sizeof(buf), 0);
    rc = slk_recv(client3, buf, sizeof(buf), 0);
    buf[rc] = '\0';
    printf("  CLIENT3 received: %s\n", buf);

    test_socket_close(client1);
    test_socket_close(client2);
    test_socket_close(client3);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Request-reply pattern */
static void test_router_request_reply()
{
    printf("  Testing request-reply pattern...\n");

    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);

    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client, "CLIENT");
    test_socket_connect(client, endpoint);

    test_sleep_ms(200);

    /* Multiple request-reply cycles */
    for (int i = 0; i < 5; i++) {
        printf("  Cycle %d...\n", i + 1);

        /* Client sends request */
        char request[256];
        snprintf(request, sizeof(request), "Request %d", i);

        slk_send(client, "SERVER", 6, SLK_SNDMORE);
        slk_send(client, request, strlen(request), 0);

        /* Server receives and processes */
        test_sleep_ms(50);
        TEST_ASSERT(test_poll_readable(server, 2000));

        char identity[256], buf[256];
        int rc = slk_recv(server, identity, sizeof(identity), 0);
        rc = slk_recv(server, buf, sizeof(buf), 0);
        buf[rc] = '\0';

        TEST_ASSERT_STR_EQ(buf, request);

        /* Server sends reply */
        char reply[256];
        snprintf(reply, sizeof(reply), "Reply %d", i);

        slk_send(server, "CLIENT", 6, SLK_SNDMORE);
        slk_send(server, reply, strlen(reply), 0);

        /* Client receives reply */
        test_sleep_ms(50);
        TEST_ASSERT(test_poll_readable(client, 2000));

        slk_recv(client, identity, sizeof(identity), 0);
        rc = slk_recv(client, buf, sizeof(buf), 0);
        buf[rc] = '\0';

        TEST_ASSERT_STR_EQ(buf, reply);
    }

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: High volume message exchange */
static void test_router_high_volume()
{
    printf("  Testing high volume message exchange...\n");

    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);

    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client, "CLIENT");
    test_socket_connect(client, endpoint);

    test_sleep_ms(200);

    const int message_count = 100;

    /* Client sends burst of messages */
    printf("  Sending %d messages...\n", message_count);
    for (int i = 0; i < message_count; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Message %d", i);

        slk_send(client, "SERVER", 6, SLK_SNDMORE);
        slk_send(client, msg, strlen(msg), 0);
    }

    /* Server receives all messages */
    printf("  Receiving %d messages...\n", message_count);
    test_sleep_ms(500);

    for (int i = 0; i < message_count; i++) {
        TEST_ASSERT(test_poll_readable(server, 5000));

        char identity[256], buf[256];
        slk_recv(server, identity, sizeof(identity), 0);
        int rc = slk_recv(server, buf, sizeof(buf), 0);
        buf[rc] = '\0';

        char expected[256];
        snprintf(expected, sizeof(expected), "Message %d", i);
        TEST_ASSERT_STR_EQ(buf, expected);

        if ((i + 1) % 25 == 0) {
            printf("  Received %d/%d messages\n", i + 1, message_count);
        }
    }

    printf("  All messages received successfully!\n");

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Reconnection handling */
static void test_router_reconnection()
{
    printf("  Testing reconnection handling...\n");

    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);

    /* First connection */
    printf("  First connection...\n");
    slk_socket_t *client1 = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client1, "CLIENT");
    test_socket_connect(client1, endpoint);

    test_sleep_ms(200);

    /* Send message */
    slk_send(client1, "SERVER", 6, SLK_SNDMORE);
    slk_send(client1, "First connection", 16, 0);

    test_sleep_ms(100);
    TEST_ASSERT(test_poll_readable(server, 2000));

    char buf[256];
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);

    /* Disconnect */
    printf("  Disconnecting client...\n");
    test_socket_close(client1);
    test_sleep_ms(300);

    /* Reconnect with same ID (requires ROUTER_HANDOVER) */
    printf("  Reconnecting client...\n");
    test_set_int_option(server, SLK_ROUTER_HANDOVER, 1);

    slk_socket_t *client2 = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client2, "CLIENT");
    test_socket_connect(client2, endpoint);

    test_sleep_ms(200);

    /* Send message from reconnected client */
    slk_send(client2, "SERVER", 6, SLK_SNDMORE);
    slk_send(client2, "After reconnect", 15, 0);

    test_sleep_ms(100);
    TEST_ASSERT(test_poll_readable(server, 2000));

    slk_recv(server, buf, sizeof(buf), 0);
    int rc = slk_recv(server, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 15);
    TEST_ASSERT_MEM_EQ(buf, "After reconnect", 15);

    printf("  Reconnection successful!\n");

    test_socket_close(client2);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Bidirectional simultaneous communication */
static void test_router_bidirectional_simultaneous()
{
    printf("  Testing bidirectional simultaneous communication...\n");

    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);

    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(client, "CLIENT");
    test_socket_connect(client, endpoint);

    test_sleep_ms(200);

    /* Both sides send messages simultaneously */
    printf("  Sending messages from both sides...\n");

    /* Client -> Server */
    slk_send(client, "SERVER", 6, SLK_SNDMORE);
    slk_send(client, "From CLIENT", 11, 0);

    /* Server -> Client */
    slk_send(server, "CLIENT", 6, SLK_SNDMORE);
    slk_send(server, "From SERVER", 11, 0);

    test_sleep_ms(200);

    /* Both sides receive */
    printf("  Receiving on both sides...\n");

    TEST_ASSERT(test_poll_readable(server, 2000));
    TEST_ASSERT(test_poll_readable(client, 2000));

    char buf[256];

    /* Server receives */
    slk_recv(server, buf, sizeof(buf), 0);
    int rc = slk_recv(server, buf, sizeof(buf), 0);
    buf[rc] = '\0';
    printf("  Server received: %s\n", buf);
    TEST_ASSERT_STR_EQ(buf, "From CLIENT");

    /* Client receives */
    slk_recv(client, buf, sizeof(buf), 0);
    rc = slk_recv(client, buf, sizeof(buf), 0);
    buf[rc] = '\0';
    printf("  Client received: %s\n", buf);
    TEST_ASSERT_STR_EQ(buf, "From SERVER");

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

int main()
{
    printf("\n");
    printf("===============================================\n");
    printf("  ServerLink Router-to-Router Integration Test\n");
    printf("===============================================\n\n");

    RUN_TEST(test_router_to_router_basic);
    RUN_TEST(test_router_multiple_clients);
    RUN_TEST(test_router_request_reply);
    RUN_TEST(test_router_high_volume);
    RUN_TEST(test_router_reconnection);
    RUN_TEST(test_router_bidirectional_simultaneous);

    printf("\n");
    printf("===============================================\n");
    printf("  All Integration Tests Passed Successfully!\n");
    printf("===============================================\n");

    return 0;
}

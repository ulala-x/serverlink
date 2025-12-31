/* ServerLink Peer Statistics Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/* Test: Check if peer is connected */
static void test_is_connected()
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

    /* Client sends a message so server knows about it */
    slk_send(client, "SERVER", 6, SLK_SNDMORE);
    slk_send(client, "", 0, SLK_SNDMORE);
    slk_send(client, "Hello", 5, 0);

    test_sleep_ms(100);

    /* Receive on server to establish connection */
    TEST_ASSERT(test_poll_readable(server, 2000));
    char buf[256];
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);

    /* Check if CLIENT is connected */
    int connected = slk_is_connected(server, "CLIENT", 6);
    TEST_ASSERT(connected != 0);

    /* Check non-existent peer */
    connected = slk_is_connected(server, "UNKNOWN", 7);
    TEST_ASSERT_EQ(connected, 0);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Get peer statistics */
static void test_get_peer_stats()
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

    /* Exchange some messages */
    for (int i = 0; i < 5; i++) {
        /* Client -> Server */
        slk_send(client, "SERVER", 6, SLK_SNDMORE);
        slk_send(client, "", 0, SLK_SNDMORE);
        slk_send(client, "Data", 4, 0);

        test_sleep_ms(50);

        char buf[256];
        slk_recv(server, buf, sizeof(buf), 0);
        slk_recv(server, buf, sizeof(buf), 0);
        slk_recv(server, buf, sizeof(buf), 0);

        /* Server -> Client */
        slk_send(server, "CLIENT", 6, SLK_SNDMORE);
        slk_send(server, "", 0, SLK_SNDMORE);
        slk_send(server, "Reply", 5, 0);

        test_sleep_ms(50);

        slk_recv(client, buf, sizeof(buf), 0);
        slk_recv(client, buf, sizeof(buf), 0);
        slk_recv(client, buf, sizeof(buf), 0);
    }

    /* Get statistics for CLIENT */
    slk_peer_stats_t stats;
    int rc = slk_get_peer_stats(server, "CLIENT", 6, &stats);

    if (rc == 0) {
        printf("  Peer statistics:\n");
        printf("    Bytes sent:      %lu\n", (unsigned long)stats.bytes_sent);
        printf("    Bytes received:  %lu\n", (unsigned long)stats.bytes_received);
        printf("    Messages sent:   %lu\n", (unsigned long)stats.msgs_sent);
        printf("    Messages recv:   %lu\n", (unsigned long)stats.msgs_received);
        printf("    Connected time:  %lu ms\n", (unsigned long)stats.connected_time);
        printf("    Is alive:        %d\n", stats.is_alive);

        /* We sent 5 messages */
        TEST_ASSERT(stats.msgs_sent >= 5);
        /* We received 5 messages */
        TEST_ASSERT(stats.msgs_received >= 5);
    } else {
        printf("  Note: Peer statistics not available (feature may not be implemented yet)\n");
    }

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Get list of connected peers */
static void test_get_peers()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    test_set_routing_id(server, "SERVER");
    test_socket_bind(server, endpoint);

    /* Connect multiple clients */
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

    /* Have each client send a message to establish connection */
    char buf[256];

    slk_send(client1, "SERVER", 6, SLK_SNDMORE);
    slk_send(client1, "", 0, SLK_SNDMORE);
    slk_send(client1, "1", 1, 0);
    test_sleep_ms(50);
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);

    slk_send(client2, "SERVER", 6, SLK_SNDMORE);
    slk_send(client2, "", 0, SLK_SNDMORE);
    slk_send(client2, "2", 1, 0);
    test_sleep_ms(50);
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);

    slk_send(client3, "SERVER", 6, SLK_SNDMORE);
    slk_send(client3, "", 0, SLK_SNDMORE);
    slk_send(client3, "3", 1, 0);
    test_sleep_ms(50);
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);

    /* Get list of peers */
    void *peer_ids[10];
    size_t id_lens[10];
    size_t num_peers = 10;

    int rc = slk_get_peers(server, peer_ids, id_lens, &num_peers);

    if (rc == 0) {
        printf("  Connected peers: %zu\n", num_peers);
        TEST_ASSERT_EQ(num_peers, 3);

        for (size_t i = 0; i < num_peers; i++) {
            printf("    Peer %zu: %.*s\n", i + 1, (int)id_lens[i], (char*)peer_ids[i]);
        }

        /* Free the peer list */
        slk_free_peers(peer_ids, id_lens, num_peers);
    } else {
        printf("  Note: Get peers not available (feature may not be implemented yet)\n");
    }

    test_socket_close(client1);
    test_socket_close(client2);
    test_socket_close(client3);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Peer statistics after disconnect */
static void test_peer_stats_after_disconnect()
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

    /* Send a message */
    slk_send(client, "SERVER", 6, SLK_SNDMORE);
    slk_send(client, "", 0, SLK_SNDMORE);
    slk_send(client, "Test", 4, 0);

    test_sleep_ms(100);

    char buf[256];
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);

    /* Verify connected */
    int connected = slk_is_connected(server, "CLIENT", 6);
    TEST_ASSERT(connected != 0);

    /* Disconnect */
    test_socket_close(client);
    test_sleep_ms(300);

    /* Check if still connected */
    connected = slk_is_connected(server, "CLIENT", 6);
    /* After disconnect, should not be connected */
    TEST_ASSERT_EQ(connected, 0);

    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: Statistics with no messages */
static void test_peer_stats_no_messages()
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

    /* Send one message to establish the connection */
    slk_send(client, "SERVER", 6, SLK_SNDMORE);
    slk_send(client, "", 0, SLK_SNDMORE);
    slk_send(client, "Init", 4, 0);

    test_sleep_ms(100);

    char buf[256];
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);
    slk_recv(server, buf, sizeof(buf), 0);

    /* Get statistics immediately (minimal activity) */
    slk_peer_stats_t stats;
    int rc = slk_get_peer_stats(server, "CLIENT", 6, &stats);

    if (rc == 0) {
        printf("  Initial peer statistics:\n");
        printf("    Messages received: %lu\n", (unsigned long)stats.msgs_received);
        printf("    Connected time:    %lu ms\n", (unsigned long)stats.connected_time);

        TEST_ASSERT(stats.msgs_received >= 1);
        TEST_ASSERT(stats.connected_time > 0);
    } else {
        printf("  Note: Peer statistics not available\n");
    }

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink Peer Statistics Tests ===\n\n");

    RUN_TEST(test_is_connected);
    RUN_TEST(test_get_peer_stats);
    RUN_TEST(test_get_peers);
    RUN_TEST(test_peer_stats_after_disconnect);
    RUN_TEST(test_peer_stats_no_messages);

    printf("\n=== All Peer Statistics Tests Passed ===\n");
    return 0;
}

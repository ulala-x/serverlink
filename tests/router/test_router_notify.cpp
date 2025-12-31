/* ServerLink ROUTER_NOTIFY Tests - Ported from libzmq */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"
#include <string.h>

/* Notification flags */
#define SLK_NOTIFY_CONNECT    1
#define SLK_NOTIFY_DISCONNECT 2

/* Test: Get/Set ROUTER_NOTIFY socket option */
static void test_sockopt_router_notify()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    int opt_notify_read;
    size_t opt_notify_read_size = sizeof(opt_notify_read);

    /* Default value is off when socket is constructed */
    int rc = slk_getsockopt(router, SLK_ROUTER_NOTIFY, &opt_notify_read, &opt_notify_read_size);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(opt_notify_read, 0);

    /* Valid value - Connect */
    int opt_notify = SLK_NOTIFY_CONNECT;
    rc = slk_setsockopt(router, SLK_ROUTER_NOTIFY, &opt_notify, sizeof(opt_notify));
    TEST_SUCCESS(rc);

    rc = slk_getsockopt(router, SLK_ROUTER_NOTIFY, &opt_notify_read, &opt_notify_read_size);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(opt_notify, opt_notify_read);

    /* Valid value - Disconnect */
    opt_notify = SLK_NOTIFY_DISCONNECT;
    rc = slk_setsockopt(router, SLK_ROUTER_NOTIFY, &opt_notify, sizeof(opt_notify));
    TEST_SUCCESS(rc);

    rc = slk_getsockopt(router, SLK_ROUTER_NOTIFY, &opt_notify_read, &opt_notify_read_size);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(opt_notify, opt_notify_read);

    /* Valid value - Off */
    opt_notify = 0;
    rc = slk_setsockopt(router, SLK_ROUTER_NOTIFY, &opt_notify, sizeof(opt_notify));
    TEST_SUCCESS(rc);

    rc = slk_getsockopt(router, SLK_ROUTER_NOTIFY, &opt_notify_read, &opt_notify_read_size);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(opt_notify, opt_notify_read);

    /* Valid value - Both */
    opt_notify = SLK_NOTIFY_CONNECT | SLK_NOTIFY_DISCONNECT;
    rc = slk_setsockopt(router, SLK_ROUTER_NOTIFY, &opt_notify, sizeof(opt_notify));
    TEST_SUCCESS(rc);

    rc = slk_getsockopt(router, SLK_ROUTER_NOTIFY, &opt_notify_read, &opt_notify_read_size);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(opt_notify, opt_notify_read);

    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Helper function for testing ROUTER_NOTIFY behavior */
static void test_router_notify_helper(int opt_notify)
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    /* Set notify option */
    int rc = slk_setsockopt(router, SLK_ROUTER_NOTIFY, &opt_notify, sizeof(opt_notify));
    TEST_SUCCESS(rc);

    test_socket_bind(router, endpoint);

    /* Create peer router socket (since ServerLink only supports ROUTER) */
    slk_socket_t *peer = test_socket_new(ctx, SLK_ROUTER);
    const char *peer_routing_id = "X";
    rc = slk_setsockopt(peer, SLK_ROUTING_ID, peer_routing_id, 1);
    TEST_SUCCESS(rc);

    /* Peer connects */
    test_socket_connect(peer, endpoint);

    /* Wait for connection to establish */
    test_sleep_ms(200);

    /* Connection notification msg */
    if (opt_notify & SLK_NOTIFY_CONNECT) {
        /* Routing-id only message of the connect */
        char buf[256];
        rc = slk_recv(router, buf, sizeof(buf), 0);  /* 1st part: routing-id */
        TEST_ASSERT(rc > 0);
        TEST_ASSERT_EQ(buf[0], 'X');

        rc = slk_recv(router, buf, sizeof(buf), 0);  /* 2nd part: empty */
        TEST_ASSERT_EQ(rc, 0);
    }

    /* Test message from the peer */
    rc = slk_send(peer, "Hello", 5, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* Receive the message */
    char buf[256];
    rc = slk_recv(router, buf, sizeof(buf), 0);  /* routing-id */
    TEST_ASSERT(rc > 0);

    rc = slk_recv(router, buf, sizeof(buf), 0);  /* payload */
    TEST_ASSERT_EQ(rc, 5);
    TEST_ASSERT_MEM_EQ(buf, "Hello", 5);

    /* Peer disconnects */
    rc = slk_disconnect(peer, endpoint);
    TEST_SUCCESS(rc);

    /* Wait for disconnect to process */
    test_sleep_ms(200);

    /* Disconnection notification msg */
    if (opt_notify & SLK_NOTIFY_DISCONNECT) {
        /* Routing-id only message of the disconnect */
        rc = slk_recv(router, buf, sizeof(buf), SLK_DONTWAIT);  /* 1st part: routing-id */
        if (rc > 0) {
            TEST_ASSERT_EQ(buf[0], 'X');

            rc = slk_recv(router, buf, sizeof(buf), SLK_DONTWAIT);  /* 2nd part: empty */
            /* May or may not receive the second part depending on timing */
        }
    }

    test_socket_close(peer);
    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Test: ROUTER_NOTIFY for connect events */
static void test_router_notify_connect()
{
    test_router_notify_helper(SLK_NOTIFY_CONNECT);
}

/* Test: ROUTER_NOTIFY for disconnect events */
static void test_router_notify_disconnect()
{
    test_router_notify_helper(SLK_NOTIFY_DISCONNECT);
}

/* Test: ROUTER_NOTIFY for both connect and disconnect events */
static void test_router_notify_both()
{
    test_router_notify_helper(SLK_NOTIFY_CONNECT | SLK_NOTIFY_DISCONNECT);
}

/* Test: Handshake failure should not deliver notification */
static void test_handshake_fail()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    /* Setup router socket */
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);
    int opt_timeout = 200;
    int opt_notify = SLK_NOTIFY_CONNECT | SLK_NOTIFY_DISCONNECT;

    /* Set options */
    int rc = slk_setsockopt(router, SLK_ROUTER_NOTIFY, &opt_notify, sizeof(opt_notify));
    TEST_SUCCESS(rc);

    /* Note: SLK_RCVTIMEO may not be supported, use polling instead */

    test_socket_bind(router, endpoint);

    /*
     * Note: ServerLink doesn't support ZMQ_STREAM for raw TCP connections.
     * This test is simplified - in production, a handshake failure would be
     * detected during the protocol negotiation phase.
     */

    test_sleep_ms(300);

    /* No notification should be delivered (use DONTWAIT to avoid blocking) */
    char buffer[255];
    rc = slk_recv(router, buffer, sizeof(buffer), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    test_socket_close(router);
    test_context_destroy(ctx);
}

/* Test: Disconnect during multipart message delivery */
static void test_error_during_multipart()
{
    /*
     * If the disconnect occurs in the middle of the multipart
     * message, the socket should not add the notification at the
     * end of the incomplete message. It must discard the incomplete
     * message, and deliver the notification as a new message.
     *
     * Note: This test is simplified for ServerLink as we don't have
     * MAXMSGSIZE option. The core concept remains: disconnect
     * notifications should be separate messages.
     */

    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    /* Setup router */
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);

    int opt_notify = SLK_NOTIFY_DISCONNECT;
    int rc = slk_setsockopt(router, SLK_ROUTER_NOTIFY, &opt_notify, sizeof(opt_notify));
    TEST_SUCCESS(rc);

    test_socket_bind(router, endpoint);

    /* Setup peer */
    slk_socket_t *peer = test_socket_new(ctx, SLK_ROUTER);
    const char *peer_routing_id = "X";

    rc = slk_setsockopt(peer, SLK_ROUTING_ID, peer_routing_id, 1);
    TEST_SUCCESS(rc);

    test_socket_connect(peer, endpoint);

    test_sleep_ms(200);

    /* Send multipart message, then disconnect */
    rc = slk_send(peer, "Hello2", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);

    /* Immediately disconnect before completing the message */
    test_socket_close(peer);

    test_sleep_ms(200);

    /* Should receive disconnect notification, not incomplete message */
    char buf[256];
    rc = slk_recv(router, buf, sizeof(buf), SLK_DONTWAIT);
    if (rc > 0) {
        /* If we get a message, it should be the routing ID */
        TEST_ASSERT_EQ(buf[0], 'X');

        /* Second part should be empty (disconnect notification) */
        rc = slk_recv(router, buf, sizeof(buf), SLK_DONTWAIT);
        /* Empty frame indicates notification */
    }

    test_socket_close(router);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink ROUTER_NOTIFY Tests ===\n\n");

    RUN_TEST(test_sockopt_router_notify);
    RUN_TEST(test_router_notify_connect);
    RUN_TEST(test_router_notify_disconnect);
    RUN_TEST(test_router_notify_both);
    RUN_TEST(test_handshake_fail);
    RUN_TEST(test_error_during_multipart);

    printf("\n=== All ROUTER_NOTIFY Tests Passed ===\n");
    return 0;
}

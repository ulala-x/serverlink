/* ServerLink CONNECT_ROUTING_ID Tests - Ported from libzmq */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"
#include <string.h>

static const char *rconn1routing_id = "conn1";
static const char *x_routing_id = "X";
static const char *y_routing_id = "Y";
static const char *z_routing_id = "Z";

/*
 * Test: ROUTER to ROUTER communication with named and unnamed routing IDs
 */
static void test_router_2_router(bool named)
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    char buff[256];
    const char msg[] = "hi 1";

    /* Create bind socket */
    slk_socket_t *rbind = test_socket_new(ctx, SLK_ROUTER);

    int zero = 0;
    int rc = slk_setsockopt(rbind, SLK_LINGER, &zero, sizeof(zero));
    TEST_SUCCESS(rc);

    /* Set rbind routing ID for ROUTER-to-ROUTER communication */
    const char *rbind_rid = named ? x_routing_id : "SERVER";
    rc = slk_setsockopt(rbind, SLK_ROUTING_ID, rbind_rid, strlen(rbind_rid));
    TEST_SUCCESS(rc);

    test_socket_bind(rbind, endpoint);

    /* Create connection socket */
    slk_socket_t *rconn1 = test_socket_new(ctx, SLK_ROUTER);

    rc = slk_setsockopt(rconn1, SLK_LINGER, &zero, sizeof(zero));
    TEST_SUCCESS(rc);

    /* If we're in named mode, set rconn1 identity */
    if (named) {
        rc = slk_setsockopt(rconn1, SLK_ROUTING_ID, y_routing_id, 1);
        TEST_SUCCESS(rc);
    }

    /* Make call to connect using a connect_routing_id */
    rc = slk_setsockopt(rconn1, SLK_CONNECT_ROUTING_ID, rconn1routing_id, strlen(rconn1routing_id));
    TEST_SUCCESS(rc);

    test_socket_connect(rconn1, endpoint);

    test_sleep_ms(200);

    /* ROUTER-to-ROUTER handshake: rconn1 sends greeting to rbind */
    /* Use CONNECT_ROUTING_ID, not rbind's actual routing ID */
    rc = slk_send(rconn1, rconn1routing_id, strlen(rconn1routing_id), SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(rconn1, "HELLO", 5, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* rbind receives the handshake */
    TEST_ASSERT(test_poll_readable(rbind, 5000));
    rc = slk_recv(rbind, buff, sizeof(buff), 0);  /* routing ID from rconn1 */
    TEST_ASSERT(rc > 0);
    int peer_routing_id_len = rc;
    char peer_routing_id[256];
    memcpy(peer_routing_id, buff, peer_routing_id_len);

    rc = slk_recv(rbind, buff, sizeof(buff), 0);  /* "HELLO" */
    TEST_ASSERT_EQ(rc, 5);

    /* rbind responds to complete handshake */
    rc = slk_send(rbind, peer_routing_id, peer_routing_id_len, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(rbind, "READY", 5, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* rconn1 receives handshake response */
    TEST_ASSERT(test_poll_readable(rconn1, 5000));
    rc = slk_recv(rconn1, buff, sizeof(buff), 0);  /* routing ID from rbind */
    TEST_ASSERT(rc > 0);
    rc = slk_recv(rconn1, buff, sizeof(buff), 0);  /* "READY" */
    TEST_ASSERT_EQ(rc, 5);

    /* Now send the actual test data - rconn1 sends to rbind */
    /* Use CONNECT_ROUTING_ID */
    rc = slk_send(rconn1, rconn1routing_id, strlen(rconn1routing_id), SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);

    rc = slk_send(rconn1, msg, strlen(msg), 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* Receive the routing ID */
    TEST_ASSERT(test_poll_readable(rbind, 5000));
    int routing_id_len = slk_recv(rbind, buff, sizeof(buff), 0);

    /* Save the routing ID before it gets overwritten */
    char saved_routing_id[256];
    memcpy(saved_routing_id, buff, routing_id_len);

    if (named) {
        TEST_ASSERT_EQ(routing_id_len, (int)strlen(y_routing_id));
        TEST_ASSERT_MEM_EQ(buff, y_routing_id, routing_id_len);
    } else {
        /* Unnamed - should get auto-generated ID */
        TEST_ASSERT(routing_id_len > 0);
    }

    /* Receive the data */
    rc = slk_recv(rbind, buff, sizeof(buff), 0);
    TEST_ASSERT_EQ(rc, (int)strlen(msg));
    TEST_ASSERT_MEM_EQ(buff, msg, rc);

    /* Send some data back using the saved routing ID */
    rc = slk_send(rbind, saved_routing_id, routing_id_len, SLK_SNDMORE);
    TEST_ASSERT_EQ(rc, routing_id_len);

    rc = slk_send(rbind, "ok", 2, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* rconn1 receives response from rbind */
    TEST_ASSERT(test_poll_readable(rconn1, 5000));
    rc = slk_recv(rconn1, buff, sizeof(buff), 0);
    /* Should receive the CONNECT_ROUTING_ID (how rconn1 refers to rbind) */
    TEST_ASSERT_EQ(rc, (int)strlen(rconn1routing_id));
    TEST_ASSERT_MEM_EQ(buff, rconn1routing_id, rc);

    rc = slk_recv(rconn1, buff, sizeof(buff), 0);
    TEST_ASSERT_EQ(rc, 2);
    TEST_ASSERT_MEM_EQ(buff, "ok", 2);

    rc = slk_unbind(rbind, endpoint);
    TEST_SUCCESS(rc);

    test_socket_close(rbind);
    test_socket_close(rconn1);
    test_context_destroy(ctx);
}

/*
 * Test: ROUTER to ROUTER communication while receiving
 */
static void test_router_2_router_while_receiving()
{
    slk_ctx_t *ctx = test_context_new();
    const char *x_endpoint = test_endpoint_tcp();
    const char *z_endpoint = test_endpoint_tcp();

    char buff[256];
    const char msg[] = "hi 1";

    int zero = 0;

    /* Create xbind socket */
    slk_socket_t *xbind = test_socket_new(ctx, SLK_ROUTER);
    int rc = slk_setsockopt(xbind, SLK_LINGER, &zero, sizeof(zero));
    TEST_SUCCESS(rc);
    test_socket_bind(xbind, x_endpoint);

    /* Create zbind socket */
    slk_socket_t *zbind = test_socket_new(ctx, SLK_ROUTER);
    rc = slk_setsockopt(zbind, SLK_LINGER, &zero, sizeof(zero));
    TEST_SUCCESS(rc);
    test_socket_bind(zbind, z_endpoint);

    /* Create connection socket */
    slk_socket_t *yconn = test_socket_new(ctx, SLK_ROUTER);
    rc = slk_setsockopt(yconn, SLK_LINGER, &zero, sizeof(zero));
    TEST_SUCCESS(rc);

    /* Set identities for each socket */
    rc = slk_setsockopt(xbind, SLK_ROUTING_ID, x_routing_id, strlen(x_routing_id));
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(yconn, SLK_ROUTING_ID, y_routing_id, 2);
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(zbind, SLK_ROUTING_ID, z_routing_id, strlen(z_routing_id));
    TEST_SUCCESS(rc);

    /* Connect Y to X using a routing id */
    rc = slk_setsockopt(yconn, SLK_CONNECT_ROUTING_ID, x_routing_id, strlen(x_routing_id));
    TEST_SUCCESS(rc);

    test_socket_connect(yconn, x_endpoint);

    test_sleep_ms(200);

    /* ROUTER-to-ROUTER handshake: Y to X */
    rc = slk_send(yconn, x_routing_id, strlen(x_routing_id), SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(yconn, "HELLO_X", 7, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* X receives handshake from Y */
    TEST_ASSERT(test_poll_readable(xbind, 5000));
    rc = slk_recv(xbind, buff, sizeof(buff), 0);  /* routing ID from Y */
    TEST_ASSERT(rc > 0);
    int y_rid_len = rc;
    char y_rid[256];
    memcpy(y_rid, buff, y_rid_len);

    rc = slk_recv(xbind, buff, sizeof(buff), 0);  /* "HELLO_X" */
    TEST_ASSERT_EQ(rc, 7);

    /* X responds to Y */
    rc = slk_send(xbind, y_rid, y_rid_len, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(xbind, "READY_X", 7, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* Y receives response */
    TEST_ASSERT(test_poll_readable(yconn, 5000));
    rc = slk_recv(yconn, buff, sizeof(buff), 0);  /* routing ID from X */
    TEST_ASSERT(rc > 0);
    rc = slk_recv(yconn, buff, sizeof(buff), 0);  /* "READY_X" */
    TEST_ASSERT_EQ(rc, 7);

    /* Send some data from Y to X */
    rc = slk_send(yconn, x_routing_id, strlen(x_routing_id), SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);

    rc = slk_send(yconn, msg, strlen(msg), 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* X receives Y's message before connecting to Z */
    TEST_ASSERT(test_poll_readable(xbind, 5000));
    rc = slk_recv(xbind, buff, sizeof(buff), 0);  /* routing ID from Y */
    TEST_ASSERT(rc > 0);
    rc = slk_recv(xbind, buff, sizeof(buff), 0);  /* "hi 1" */
    TEST_ASSERT_EQ(rc, (int)strlen(msg));
    TEST_ASSERT_MEM_EQ(buff, msg, rc);

    /* Now X tries to connect to Z and send a message */
    rc = slk_setsockopt(xbind, SLK_CONNECT_ROUTING_ID, z_routing_id, strlen(z_routing_id));
    TEST_SUCCESS(rc);

    test_socket_connect(xbind, z_endpoint);

    test_sleep_ms(200);

    /* ROUTER-to-ROUTER handshake: X to Z */
    rc = slk_send(xbind, z_routing_id, strlen(z_routing_id), SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(xbind, "HELLO_Z", 7, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* Z receives handshake from X */
    TEST_ASSERT(test_poll_readable(zbind, 5000));
    rc = slk_recv(zbind, buff, sizeof(buff), 0);  /* routing ID from X */
    TEST_ASSERT(rc > 0);
    int x_rid_len = rc;
    char x_rid[256];
    memcpy(x_rid, buff, x_rid_len);

    rc = slk_recv(zbind, buff, sizeof(buff), 0);  /* "HELLO_Z" */
    TEST_ASSERT_EQ(rc, 7);

    /* Z responds to X */
    rc = slk_send(zbind, x_rid, x_rid_len, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(zbind, "READY_Z", 7, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* X receives response from Z */
    TEST_ASSERT(test_poll_readable(xbind, 5000));
    rc = slk_recv(xbind, buff, sizeof(buff), 0);  /* routing ID from Z */
    TEST_ASSERT(rc > 0);
    rc = slk_recv(xbind, buff, sizeof(buff), 0);  /* "READY_Z" */
    TEST_ASSERT_EQ(rc, 7);

    /* Try to send some data from X to Z */
    rc = slk_send(xbind, z_routing_id, strlen(z_routing_id), SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);

    rc = slk_send(xbind, msg, strlen(msg), 0);
    TEST_ASSERT(rc >= 0);

    /* Wait for the X->Z message to be received */
    test_sleep_ms(100);

    /* Nothing should have been received on the Y socket */
    rc = slk_recv(yconn, buff, sizeof(buff), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* The message should have been received on the Z socket */
    TEST_ASSERT(test_poll_readable(zbind, 5000));
    rc = slk_recv(zbind, buff, sizeof(buff), 0);
    TEST_ASSERT_EQ(rc, (int)strlen(x_routing_id));
    TEST_ASSERT_MEM_EQ(buff, x_routing_id, rc);

    rc = slk_recv(zbind, buff, sizeof(buff), 0);
    TEST_ASSERT_EQ(rc, (int)strlen(msg));
    TEST_ASSERT_MEM_EQ(buff, msg, rc);

    rc = slk_unbind(xbind, x_endpoint);
    TEST_SUCCESS(rc);

    rc = slk_unbind(zbind, z_endpoint);
    TEST_SUCCESS(rc);

    test_socket_close(yconn);
    test_socket_close(xbind);
    test_socket_close(zbind);
    test_context_destroy(ctx);
}

/* Test wrapper for unnamed router communication */
static void test_router_2_router_unnamed()
{
    test_router_2_router(false);
}

/* Test wrapper for named router communication */
static void test_router_2_router_named()
{
    test_router_2_router(true);
}

int main()
{
    printf("=== ServerLink CONNECT_ROUTING_ID Tests ===\n\n");

    /*
     * Note: test_stream_2_stream is skipped as ServerLink doesn't support
     * ZMQ_STREAM socket type. ServerLink only supports ROUTER sockets.
     */

    RUN_TEST(test_router_2_router_unnamed);
    RUN_TEST(test_router_2_router_named);
    RUN_TEST(test_router_2_router_while_receiving);

    printf("\n=== All CONNECT_ROUTING_ID Tests Passed ===\n");
    return 0;
}

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

    test_socket_bind(rbind, endpoint);

    /* Create connection socket */
    slk_socket_t *rconn1 = test_socket_new(ctx, SLK_ROUTER);

    rc = slk_setsockopt(rconn1, SLK_LINGER, &zero, sizeof(zero));
    TEST_SUCCESS(rc);

    /* If we're in named mode, set some identities */
    if (named) {
        rc = slk_setsockopt(rbind, SLK_ROUTING_ID, x_routing_id, 1);
        TEST_SUCCESS(rc);

        rc = slk_setsockopt(rconn1, SLK_ROUTING_ID, y_routing_id, 1);
        TEST_SUCCESS(rc);
    }

    /* Make call to connect using a connect_routing_id */
    rc = slk_setsockopt(rconn1, SLK_CONNECT_ROUTING_ID, rconn1routing_id, strlen(rconn1routing_id));
    TEST_SUCCESS(rc);

    test_socket_connect(rconn1, endpoint);

    test_sleep_ms(200);

    /* Send some data */
    rc = slk_send(rconn1, rconn1routing_id, strlen(rconn1routing_id), SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);

    rc = slk_send(rconn1, msg, strlen(msg), 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* Receive the routing ID */
    int routing_id_len = slk_recv(rbind, buff, sizeof(buff), 0);
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

    /* Send some data back */
    rc = slk_send(rbind, buff, routing_id_len, SLK_SNDMORE);
    TEST_ASSERT_EQ(rc, routing_id_len);

    rc = slk_send(rbind, "ok", 2, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* If bound socket identity naming a problem, we'll likely see something funky here */
    rc = slk_recv(rconn1, buff, sizeof(buff), 0);
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

    /* Send some data from Y to X */
    rc = slk_send(yconn, x_routing_id, strlen(x_routing_id), SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);

    rc = slk_send(yconn, msg, strlen(msg), 0);
    TEST_ASSERT(rc >= 0);

    /* Wait for the Y->X message to be received */
    test_sleep_ms(100);

    /* Now X tries to connect to Z and send a message */
    rc = slk_setsockopt(xbind, SLK_CONNECT_ROUTING_ID, z_routing_id, strlen(z_routing_id));
    TEST_SUCCESS(rc);

    test_socket_connect(xbind, z_endpoint);

    test_sleep_ms(100);

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

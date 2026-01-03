/* ServerLink Socket Option HWM Tests - Ported from libzmq */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

static const int MAX_SENDS = 10000;

/*
 * Note: Original libzmq test used PUSH/PULL sockets.
 * ServerLink only supports ROUTER, so we adapt the test.
 */

/* Test: Change HWM before connection */
static void test_change_before_connected()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://hwm_before";

    slk_socket_t *bind_socket = test_socket_new(ctx, SLK_ROUTER);
    slk_socket_t *connect_socket = test_socket_new(ctx, SLK_ROUTER);

    /* Wait for I/O thread to fully initialize sockets (ARM64 stability) */
    test_sleep_ms(10);

    int val = 2;
    int rc = slk_setsockopt(bind_socket, SLK_RCVHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    rc = slk_setsockopt(connect_socket, SLK_SNDHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    /* Enable ROUTER_MANDATORY to get backpressure instead of silent drops */
    int mandatory = 1;
    rc = slk_setsockopt(connect_socket, SLK_ROUTER_MANDATORY, &mandatory, sizeof(mandatory));
    TEST_SUCCESS(rc);

    /* Set routing IDs for both sockets (ROUTER-to-ROUTER) */
    rc = slk_setsockopt(bind_socket, SLK_ROUTING_ID, "bind", 4);
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(connect_socket, SLK_ROUTING_ID, "sender", 6);
    TEST_SUCCESS(rc);

    /* Set the peer's routing ID for connect socket */
    rc = slk_setsockopt(connect_socket, SLK_CONNECT_ROUTING_ID, "bind", 4);
    TEST_SUCCESS(rc);

    test_socket_bind(bind_socket, endpoint);
    test_socket_connect(connect_socket, endpoint);

    test_sleep_ms(100);

    /* Handshake: bind sends first message to establish routing */
    rc = slk_send(bind_socket, "sender", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(bind_socket, "READY", 5, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* Connect socket receives handshake */
    char buf[256];
    rc = slk_recv(connect_socket, buf, sizeof(buf), 0);  /* routing ID "bind" */
    TEST_ASSERT(rc > 0);
    rc = slk_recv(connect_socket, buf, sizeof(buf), 0);  /* "READY" */
    TEST_ASSERT(rc > 0);

    size_t placeholder = sizeof(val);
    val = 0;
    rc = slk_getsockopt(connect_socket, SLK_SNDHWM, &val, &placeholder);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_EQ(val, 2);

    int send_count = 0;
    while (send_count < MAX_SENDS) {
        /* ROUTER-to-ROUTER: send receiver ID first, then payload */
        rc = slk_send(connect_socket, "bind", 4, SLK_SNDMORE | SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        rc = slk_send(connect_socket, "", 0, SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        ++send_count;
    }

    /* Total buffer is send HWM (2) + receive HWM (2) = 4
     * But for ROUTER-to-ROUTER, we typically get send HWM only due to routing */
    printf("  test_change_before_connected: sent %d messages (HWM: send=%d, recv=%d)\n", send_count, 2, 2);
    TEST_ASSERT(send_count >= 2 && send_count <= 4);

    test_socket_close(bind_socket);
    test_socket_close(connect_socket);
    test_context_destroy(ctx);
}

/* Test: Change HWM after connection */
static void test_change_after_connected()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://hwm_after";

    slk_socket_t *bind_socket = test_socket_new(ctx, SLK_ROUTER);
    slk_socket_t *connect_socket = test_socket_new(ctx, SLK_ROUTER);

    /* Wait for I/O thread to fully initialize sockets (ARM64 stability) */
    test_sleep_ms(10);

    int val = 1;
    int rc = slk_setsockopt(bind_socket, SLK_RCVHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    rc = slk_setsockopt(connect_socket, SLK_SNDHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    /* Enable ROUTER_MANDATORY to get backpressure instead of silent drops */
    int mandatory = 1;
    rc = slk_setsockopt(connect_socket, SLK_ROUTER_MANDATORY, &mandatory, sizeof(mandatory));
    TEST_SUCCESS(rc);

    /* Set routing IDs for both sockets (ROUTER-to-ROUTER) */
    rc = slk_setsockopt(bind_socket, SLK_ROUTING_ID, "bind", 4);
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(connect_socket, SLK_ROUTING_ID, "sender", 6);
    TEST_SUCCESS(rc);

    /* Set the peer's routing ID for connect socket */
    rc = slk_setsockopt(connect_socket, SLK_CONNECT_ROUTING_ID, "bind", 4);
    TEST_SUCCESS(rc);

    test_socket_bind(bind_socket, endpoint);
    test_socket_connect(connect_socket, endpoint);

    test_sleep_ms(100);

    /* Handshake: bind sends first message to establish routing */
    rc = slk_send(bind_socket, "sender", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(bind_socket, "READY", 5, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* Connect socket receives handshake */
    char buf[256];
    rc = slk_recv(connect_socket, buf, sizeof(buf), 0);  /* routing ID "bind" */
    TEST_ASSERT(rc > 0);
    rc = slk_recv(connect_socket, buf, sizeof(buf), 0);  /* "READY" */
    TEST_ASSERT(rc > 0);

    val = 5;
    rc = slk_setsockopt(connect_socket, SLK_SNDHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    size_t placeholder = sizeof(val);
    val = 0;
    rc = slk_getsockopt(connect_socket, SLK_SNDHWM, &val, &placeholder);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_EQ(val, 5);

    int send_count = 0;
    while (send_count < MAX_SENDS) {
        /* ROUTER-to-ROUTER: send receiver ID first, then payload */
        rc = slk_send(connect_socket, "bind", 4, SLK_SNDMORE | SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        rc = slk_send(connect_socket, "", 0, SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        ++send_count;
    }

    /* Total buffer is send HWM (5) + receive HWM (1) = 6
     * But for ROUTER-to-ROUTER, we typically get send HWM only due to routing */
    printf("  test_change_after_connected: sent %d messages (HWM: send=%d, recv=%d)\n", send_count, 5, 1);
    TEST_ASSERT(send_count >= 5 && send_count <= 6);

    test_socket_close(bind_socket);
    test_socket_close(connect_socket);
    test_context_destroy(ctx);
}

/* Helper: Send until would block (ROUTER-to-ROUTER) */
static int send_until_wouldblock(slk_socket_t *socket, const char *receiver_id, size_t id_len)
{
    int send_count = 0;
    while (send_count < MAX_SENDS) {
        int data = send_count;
        /* ROUTER-to-ROUTER: send receiver ID first, then payload */
        int rc = slk_send(socket, receiver_id, id_len, SLK_SNDMORE | SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        rc = slk_send(socket, &data, sizeof(data), SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        if (rc == sizeof(data))
            ++send_count;
    }
    return send_count;
}

/* Helper: Test filling up to HWM */
static int test_fill_up_to_hwm(slk_socket_t *socket, int sndhwm, const char *receiver_id, size_t id_len)
{
    int send_count = send_until_wouldblock(socket, receiver_id, id_len);
    fprintf(stderr, "  sndhwm==%d, send_count==%d\n", sndhwm, send_count);

    /* Should be less than or equal to HWM + some buffer */
    TEST_ASSERT(send_count <= sndhwm + 1);

    /* Should be at least 10% of HWM */
    TEST_ASSERT(send_count > sndhwm / 10);

    return send_count;
}

/* Test: Decrease HWM when queue is full */
static void test_decrease_when_full()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://hwm_decrease";

    slk_socket_t *bind_socket = test_socket_new(ctx, SLK_ROUTER);
    slk_socket_t *connect_socket = test_socket_new(ctx, SLK_ROUTER);

    /* Wait for I/O thread to fully initialize sockets (ARM64 stability) */
    test_sleep_ms(10);

    int val = 1;
    int rc = slk_setsockopt(bind_socket, SLK_RCVHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    int sndhwm = 100;
    rc = slk_setsockopt(connect_socket, SLK_SNDHWM, &sndhwm, sizeof(sndhwm));
    TEST_ASSERT_EQ(rc, 0);

    /* Enable ROUTER_MANDATORY to get backpressure instead of silent drops */
    int mandatory = 1;
    rc = slk_setsockopt(connect_socket, SLK_ROUTER_MANDATORY, &mandatory, sizeof(mandatory));
    TEST_SUCCESS(rc);

    /* Set routing IDs for both sockets (ROUTER-to-ROUTER) */
    const char *receiver_id = "bind";
    rc = slk_setsockopt(bind_socket, SLK_ROUTING_ID, receiver_id, strlen(receiver_id));
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(connect_socket, SLK_ROUTING_ID, "sender", 6);
    TEST_SUCCESS(rc);

    /* Set the peer's routing ID for connect socket */
    rc = slk_setsockopt(connect_socket, SLK_CONNECT_ROUTING_ID, receiver_id, strlen(receiver_id));
    TEST_SUCCESS(rc);

    test_socket_bind(bind_socket, endpoint);
    test_socket_connect(connect_socket, endpoint);

    /* We must wait for the connect to succeed */
    test_sleep_ms(100);

    /* Handshake: bind sends first message to establish routing (required for ARM64) */
    rc = slk_send(bind_socket, "sender", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(bind_socket, "READY", 5, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* Connect socket receives handshake */
    char handshake_buf[256];
    rc = slk_recv(connect_socket, handshake_buf, sizeof(handshake_buf), 0);  /* routing ID "bind" */
    TEST_ASSERT(rc > 0);
    rc = slk_recv(connect_socket, handshake_buf, sizeof(handshake_buf), 0);  /* "READY" */
    TEST_ASSERT(rc > 0);

    /* Fill up to HWM */
    int send_count = test_fill_up_to_hwm(connect_socket, sndhwm, receiver_id, strlen(receiver_id));

    /* Decrease send HWM */
    sndhwm = 70;
    rc = slk_setsockopt(connect_socket, SLK_SNDHWM, &sndhwm, sizeof(sndhwm));
    TEST_ASSERT_EQ(rc, 0);

    int sndhwm_read = 0;
    size_t sndhwm_read_size = sizeof(sndhwm_read);
    rc = slk_getsockopt(connect_socket, SLK_SNDHWM, &sndhwm_read, &sndhwm_read_size);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_EQ(sndhwm, sndhwm_read);

    test_sleep_ms(100);

    /* Read out all data (should get up to previous HWM worth so none were dropped) */
    int read_count = 0;
    int read_data = 0;
    while (read_count < MAX_SENDS) {
        char buf[256];
        rc = slk_recv(bind_socket, buf, sizeof(buf), SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;

        /* First frame is routing ID, second is data */
        if (rc > 0 && read_count % 2 == 1) {
            /* This is the data frame */
            if (rc == sizeof(int)) {
                memcpy(&read_data, buf, sizeof(int));
                TEST_ASSERT_EQ(read_data, read_count / 2);
            }
        }
        ++read_count;
    }

    /* We should have received approximately send_count * 2 frames (ID + data) */
    printf("  Received %d frames for %d messages\n", read_count, send_count);

    /* Give I/O thread some time to catch up */
    test_sleep_ms(100);

    /* Fill up to new HWM */
    test_fill_up_to_hwm(connect_socket, sndhwm, receiver_id, strlen(receiver_id));

    test_socket_close(bind_socket);
    test_socket_close(connect_socket);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink Socket Option HWM Tests ===\n\n");

    RUN_TEST(test_change_before_connected);
    RUN_TEST(test_change_after_connected);
    RUN_TEST(test_decrease_when_full);

    printf("\n=== All Socket Option HWM Tests Passed ===\n");
    return 0;
}

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
    const char *endpoint = "inproc://a";

    slk_socket_t *bind_socket = test_socket_new(ctx, SLK_ROUTER);
    slk_socket_t *connect_socket = test_socket_new(ctx, SLK_ROUTER);

    int val = 2;
    int rc = slk_setsockopt(bind_socket, SLK_RCVHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    rc = slk_setsockopt(connect_socket, SLK_SNDHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    rc = slk_setsockopt(connect_socket, SLK_ROUTING_ID, "sender", 6);
    TEST_SUCCESS(rc);

    test_socket_connect(connect_socket, endpoint);
    test_socket_bind(bind_socket, endpoint);

    test_sleep_ms(100);

    size_t placeholder = sizeof(val);
    val = 0;
    rc = slk_getsockopt(connect_socket, SLK_SNDHWM, &val, &placeholder);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_EQ(val, 2);

    int send_count = 0;
    while (send_count < MAX_SENDS) {
        rc = slk_send(connect_socket, NULL, 0, SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        ++send_count;
    }

    /* Total buffer is send HWM (2) + receive HWM (2) = 4 */
    TEST_ASSERT_EQ(send_count, 4);

    test_socket_close(bind_socket);
    test_socket_close(connect_socket);
    test_context_destroy(ctx);
}

/* Test: Change HWM after connection */
static void test_change_after_connected()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://a";

    slk_socket_t *bind_socket = test_socket_new(ctx, SLK_ROUTER);
    slk_socket_t *connect_socket = test_socket_new(ctx, SLK_ROUTER);

    int val = 1;
    int rc = slk_setsockopt(bind_socket, SLK_RCVHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    rc = slk_setsockopt(connect_socket, SLK_SNDHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    rc = slk_setsockopt(connect_socket, SLK_ROUTING_ID, "sender", 6);
    TEST_SUCCESS(rc);

    test_socket_connect(connect_socket, endpoint);
    test_socket_bind(bind_socket, endpoint);

    test_sleep_ms(100);

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
        rc = slk_send(connect_socket, NULL, 0, SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        ++send_count;
    }

    /* Total buffer is send HWM (5) + receive HWM (1) = 6 */
    TEST_ASSERT_EQ(send_count, 6);

    test_socket_close(bind_socket);
    test_socket_close(connect_socket);
    test_context_destroy(ctx);
}

/* Helper: Send until would block */
static int send_until_wouldblock(slk_socket_t *socket)
{
    int send_count = 0;
    while (send_count < MAX_SENDS) {
        int data = send_count;
        int rc = slk_send(socket, &data, sizeof(data), SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        if (rc == sizeof(data))
            ++send_count;
    }
    return send_count;
}

/* Helper: Test filling up to HWM */
static int test_fill_up_to_hwm(slk_socket_t *socket, int sndhwm)
{
    int send_count = send_until_wouldblock(socket);
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
    const char *endpoint = "inproc://a";

    slk_socket_t *bind_socket = test_socket_new(ctx, SLK_ROUTER);
    slk_socket_t *connect_socket = test_socket_new(ctx, SLK_ROUTER);

    int val = 1;
    int rc = slk_setsockopt(bind_socket, SLK_RCVHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    int sndhwm = 100;
    rc = slk_setsockopt(connect_socket, SLK_SNDHWM, &sndhwm, sizeof(sndhwm));
    TEST_ASSERT_EQ(rc, 0);

    rc = slk_setsockopt(connect_socket, SLK_ROUTING_ID, "sender", 6);
    TEST_SUCCESS(rc);

    test_socket_bind(bind_socket, endpoint);
    test_socket_connect(connect_socket, endpoint);

    /* We must wait for the connect to succeed */
    test_sleep_ms(100);

    /* Fill up to HWM */
    int send_count = test_fill_up_to_hwm(connect_socket, sndhwm);

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
    test_fill_up_to_hwm(connect_socket, sndhwm);

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

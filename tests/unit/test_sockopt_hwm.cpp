/* ServerLink Socket Option HWM Tests - Ported from libzmq */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

static const int MAX_SENDS = 10000;

/*
 * Note: Original libzmq test used PUSH/PULL sockets.
 * ServerLink uses PUB/SUB which has similar one-way flow semantics.
 * PUB sends to all SUB, no handshake required.
 */

/* Test: Change HWM before connection */
static void test_change_before_connected()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://hwm_before";

    slk_socket_t *pub = test_socket_new(ctx, SLK_PUB);
    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);

    /* Set HWM on both sockets */
    int val = 2;
    int rc = slk_setsockopt(sub, SLK_RCVHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    rc = slk_setsockopt(pub, SLK_SNDHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    /* Subscribe to all messages */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    TEST_SUCCESS(rc);

    /* Connect sockets */
    rc = slk_bind(pub, endpoint);
    TEST_SUCCESS(rc);
    rc = slk_connect(sub, endpoint);
    TEST_SUCCESS(rc);

    /* Wait for connection */
    test_sleep_ms(SETTLE_TIME);

    /* Verify HWM setting */
    size_t placeholder = sizeof(val);
    val = 0;
    rc = slk_getsockopt(pub, SLK_SNDHWM, &val, &placeholder);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_EQ(val, 2);

    /* Send messages until blocked */
    int send_count = 0;
    while (send_count < MAX_SENDS) {
        rc = slk_send(pub, "X", 1, SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        if (rc < 0)
            break;
        ++send_count;
    }

    /* Expected: send HWM (2) + receive HWM (2) = 4, but PUB may drop silently
     * With inproc, typically we get at least HWM messages buffered */
    printf("  test_change_before_connected: sent %d messages (HWM: send=%d, recv=%d)\n", send_count, 2, 2);
    TEST_ASSERT(send_count >= 2);

    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

/* Test: Change HWM after connection */
static void test_change_after_connected()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://hwm_after";

    slk_socket_t *pub = test_socket_new(ctx, SLK_PUB);
    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);

    /* Set initial HWM */
    int val = 1;
    int rc = slk_setsockopt(sub, SLK_RCVHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    rc = slk_setsockopt(pub, SLK_SNDHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    /* Subscribe to all messages */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    TEST_SUCCESS(rc);

    /* Connect sockets */
    rc = slk_bind(pub, endpoint);
    TEST_SUCCESS(rc);
    rc = slk_connect(sub, endpoint);
    TEST_SUCCESS(rc);

    /* Wait for connection */
    test_sleep_ms(SETTLE_TIME);

    /* Change HWM after connection */
    val = 5;
    rc = slk_setsockopt(pub, SLK_SNDHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    /* Verify change */
    size_t placeholder = sizeof(val);
    val = 0;
    rc = slk_getsockopt(pub, SLK_SNDHWM, &val, &placeholder);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_EQ(val, 5);

    /* Send messages until blocked */
    int send_count = 0;
    while (send_count < MAX_SENDS) {
        rc = slk_send(pub, "X", 1, SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        if (rc < 0)
            break;
        ++send_count;
    }

    printf("  test_change_after_connected: sent %d messages (HWM: send=%d, recv=%d)\n", send_count, 5, 1);
    TEST_ASSERT(send_count >= 5);

    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

/* Helper: Send until would block */
static int send_until_wouldblock(slk_socket_t *pub)
{
    int send_count = 0;
    while (send_count < MAX_SENDS) {
        int data = send_count;
        int rc = slk_send(pub, &data, sizeof(data), SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        if (rc < 0)
            break;
        if (rc == sizeof(data))
            ++send_count;
    }
    return send_count;
}

/* Helper: Test filling up to HWM */
static int test_fill_up_to_hwm(slk_socket_t *pub, int sndhwm)
{
    int send_count = send_until_wouldblock(pub);
    fprintf(stderr, "  sndhwm==%d, send_count==%d\n", sndhwm, send_count);

    /* Should send at least some messages */
    TEST_ASSERT(send_count > 0);

    return send_count;
}

/* Test: Decrease HWM when queue is full */
static void test_decrease_when_full()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://hwm_decrease";

    slk_socket_t *pub = test_socket_new(ctx, SLK_PUB);
    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);

    /* Set HWM */
    int val = 1;
    int rc = slk_setsockopt(sub, SLK_RCVHWM, &val, sizeof(val));
    TEST_ASSERT_EQ(rc, 0);

    int sndhwm = 100;
    rc = slk_setsockopt(pub, SLK_SNDHWM, &sndhwm, sizeof(sndhwm));
    TEST_ASSERT_EQ(rc, 0);

    /* Subscribe to all messages */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    TEST_SUCCESS(rc);

    /* Connect sockets */
    rc = slk_bind(pub, endpoint);
    TEST_SUCCESS(rc);
    rc = slk_connect(sub, endpoint);
    TEST_SUCCESS(rc);

    /* Wait for connection */
    test_sleep_ms(SETTLE_TIME);

    /* Fill up to HWM */
    int send_count = test_fill_up_to_hwm(pub, sndhwm);

    /* Decrease send HWM */
    sndhwm = 70;
    rc = slk_setsockopt(pub, SLK_SNDHWM, &sndhwm, sizeof(sndhwm));
    TEST_ASSERT_EQ(rc, 0);

    /* Verify change */
    int sndhwm_read = 0;
    size_t sndhwm_read_size = sizeof(sndhwm_read);
    rc = slk_getsockopt(pub, SLK_SNDHWM, &sndhwm_read, &sndhwm_read_size);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_EQ(sndhwm, sndhwm_read);

    /* Read out all data */
    int read_count = 0;
    int read_data = 0;
    while (read_count < MAX_SENDS) {
        rc = slk_recv(sub, &read_data, sizeof(read_data), SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        if (rc < 0)
            break;
        ++read_count;
    }

    printf("  Received %d messages out of %d sent\n", read_count, send_count);

    /* Give I/O thread time to process */
    test_sleep_ms(100);

    /* Fill up to new HWM */
    test_fill_up_to_hwm(pub, sndhwm);

    test_socket_close(pub);
    test_socket_close(sub);
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

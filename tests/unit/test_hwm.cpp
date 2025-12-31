/* ServerLink HWM (High Water Mark) Tests - Ported from libzmq */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

static const int MAX_SENDS = 10000;

enum TestType {
    BIND_FIRST,
    CONNECT_FIRST
};

/*
 * Note: Original libzmq test used PUSH/PULL sockets.
 * ServerLink only supports ROUTER, so we adapt the test to use ROUTER sockets.
 */

/* Test: Default HWM values */
static void test_defaults()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://a";

    /* Set up bind socket */
    slk_socket_t *bind_socket = test_socket_new(ctx, SLK_ROUTER);
    test_socket_bind(bind_socket, endpoint);

    /* Set up connect socket */
    slk_socket_t *connect_socket = test_socket_new(ctx, SLK_ROUTER);
    int rc = slk_setsockopt(connect_socket, SLK_ROUTING_ID, "sender", 6);
    TEST_SUCCESS(rc);
    test_socket_connect(connect_socket, endpoint);

    test_sleep_ms(100);

    /* Send until we block */
    int send_count = 0;
    while (send_count < MAX_SENDS) {
        rc = slk_send(connect_socket, NULL, 0, SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        ++send_count;
    }

    test_sleep_ms(100);

    /* Now receive all sent messages */
    int recv_count = 0;
    while (recv_count < send_count) {
        char buf[256];
        rc = slk_recv(bind_socket, buf, sizeof(buf), SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        ++recv_count;
    }

    TEST_ASSERT_EQ(send_count, recv_count);

    /* Clean up */
    test_socket_close(connect_socket);
    test_socket_close(bind_socket);
    test_context_destroy(ctx);

    /* Default values are 1000 on send and 1000 on receive, so 2000 total */
    printf("  Sent/Received %d messages (expected ~2000 with default HWM)\n", send_count);
}

/* Helper function to count messages with specific HWM settings */
static int count_msg(slk_ctx_t *ctx, int send_hwm, int recv_hwm, TestType test_type)
{
    const char *endpoint = "inproc://a";
    slk_socket_t *bind_socket;
    slk_socket_t *connect_socket;

    if (test_type == BIND_FIRST) {
        /* Set up bind socket */
        bind_socket = test_socket_new(ctx, SLK_ROUTER);
        int rc = slk_setsockopt(bind_socket, SLK_RCVHWM, &recv_hwm, sizeof(recv_hwm));
        TEST_SUCCESS(rc);
        test_socket_bind(bind_socket, endpoint);

        /* Set up connect socket */
        connect_socket = test_socket_new(ctx, SLK_ROUTER);
        rc = slk_setsockopt(connect_socket, SLK_ROUTING_ID, "sender", 6);
        TEST_SUCCESS(rc);
        rc = slk_setsockopt(connect_socket, SLK_SNDHWM, &send_hwm, sizeof(send_hwm));
        TEST_SUCCESS(rc);
        test_socket_connect(connect_socket, endpoint);

        /* We must wait for the connect to succeed */
        test_sleep_ms(100);
    } else {
        /* Set up connect socket */
        connect_socket = test_socket_new(ctx, SLK_ROUTER);
        int rc = slk_setsockopt(connect_socket, SLK_ROUTING_ID, "sender", 6);
        TEST_SUCCESS(rc);
        rc = slk_setsockopt(connect_socket, SLK_SNDHWM, &send_hwm, sizeof(send_hwm));
        TEST_SUCCESS(rc);
        test_socket_connect(connect_socket, endpoint);

        /* Set up bind socket */
        bind_socket = test_socket_new(ctx, SLK_ROUTER);
        rc = slk_setsockopt(bind_socket, SLK_RCVHWM, &recv_hwm, sizeof(recv_hwm));
        TEST_SUCCESS(rc);
        test_socket_bind(bind_socket, endpoint);

        test_sleep_ms(100);
    }

    /* Send until we block */
    int send_count = 0;
    while (send_count < MAX_SENDS) {
        int rc = slk_send(connect_socket, NULL, 0, SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        ++send_count;
    }

    test_sleep_ms(50);

    /* Now receive all sent messages */
    int recv_count = 0;
    while (recv_count < send_count) {
        char buf[256];
        int rc = slk_recv(bind_socket, buf, sizeof(buf), SLK_DONTWAIT);
        if (rc < 0 && slk_errno() == SLK_EAGAIN)
            break;
        ++recv_count;
    }

    TEST_ASSERT_EQ(send_count, recv_count);

    /* Now it should be possible to send one more */
    int rc = slk_send(connect_socket, NULL, 0, 0);
    TEST_ASSERT(rc >= 0);

    /* Consume the remaining message */
    char buf[256];
    rc = slk_recv(bind_socket, buf, sizeof(buf), 0);
    TEST_ASSERT(rc >= 0);

    /* Clean up */
    test_socket_close(connect_socket);
    test_socket_close(bind_socket);

    return send_count;
}

/* Test: Infinite HWM on both send and receive */
static void test_infinite_both()
{
    slk_ctx_t *ctx = test_context_new();
    int count = count_msg(ctx, 0, 0, BIND_FIRST);
    TEST_ASSERT_EQ(count, MAX_SENDS);
    test_context_destroy(ctx);
}

/* Test: Infinite receive HWM */
static void test_infinite_receive()
{
    slk_ctx_t *ctx = test_context_new();
    int count = count_msg(ctx, 1, 0, BIND_FIRST);
    TEST_ASSERT_EQ(count, MAX_SENDS);
    test_context_destroy(ctx);
}

/* Test: Infinite send HWM */
static void test_infinite_send()
{
    slk_ctx_t *ctx = test_context_new();
    int count = count_msg(ctx, 0, 1, BIND_FIRST);
    TEST_ASSERT_EQ(count, MAX_SENDS);
    test_context_destroy(ctx);
}

/* Test: Finite HWM on both sides */
static void test_finite_both()
{
    slk_ctx_t *ctx = test_context_new();
    /* Send and recv buffers hwm 1, so total that can be queued is 2 */
    int count = count_msg(ctx, 1, 1, BIND_FIRST);
    TEST_ASSERT_EQ(count, 2);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink HWM Tests ===\n\n");

    printf("Note: HWM tests adapted for ROUTER sockets (ServerLink only supports ROUTER)\n\n");

    RUN_TEST(test_defaults);
    RUN_TEST(test_infinite_both);
    RUN_TEST(test_infinite_receive);
    RUN_TEST(test_infinite_send);
    RUN_TEST(test_finite_both);

    printf("\n=== All HWM Tests Passed ===\n");
    return 0;
}

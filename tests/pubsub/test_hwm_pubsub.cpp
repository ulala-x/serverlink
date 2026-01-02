/* ServerLink PUB/SUB HWM Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Use SETTLE_TIME from testutil.hpp (300ms) */

/* Test: Default HWM behavior - send until mute, verify all received */
static int test_defaults(int send_hwm, int msg_cnt, const char *endpoint)
{
    slk_ctx_t *ctx = test_context_new();

    /* Set up and bind XPUB socket */
    slk_socket_t *pub_socket = test_socket_new(ctx, SLK_XPUB);
    test_socket_bind(pub_socket, endpoint);

    /* Set up and connect SUB socket */
    slk_socket_t *sub_socket = test_socket_new(ctx, SLK_SUB);
    int rc = slk_connect(sub_socket, endpoint);
    TEST_SUCCESS(rc);

    /* Set HWM on publisher */
    rc = slk_setsockopt(pub_socket, SLK_SNDHWM, &send_hwm, sizeof(send_hwm));
    TEST_SUCCESS(rc);

    /* Subscribe to all messages */
    rc = slk_setsockopt(sub_socket, SLK_SUBSCRIBE, "", 0);
    TEST_SUCCESS(rc);

    /* Wait before starting TX operations till subscriber has subscribed */
    /* Expect subscription message: {1, 0} for empty topic */
    char sub_msg[2];
    rc = slk_recv(pub_socket, sub_msg, sizeof(sub_msg), 0);
    TEST_ASSERT(rc >= 1);
    TEST_ASSERT_EQ(sub_msg[0], 1);

    /* Send until we reach "mute" state */
    int send_count = 0;
    while (send_count < msg_cnt) {
        rc = slk_send(pub_socket, "test message", 12, SLK_DONTWAIT);
        if (rc < 0) {
            break;
        }
        send_count++;
    }

    TEST_ASSERT_EQ(send_hwm, send_count);
    test_sleep_ms(SETTLE_TIME);

    /* Now receive all sent messages */
    int recv_count = 0;
    char dummybuff[64];
    while (1) {
        rc = slk_recv(sub_socket, dummybuff, sizeof(dummybuff), SLK_DONTWAIT);
        if (rc < 0) {
            break;
        }
        recv_count++;
    }

    TEST_ASSERT_EQ(send_hwm, recv_count);

    /* Clean up */
    test_socket_close(sub_socket);
    test_socket_close(pub_socket);
    test_context_destroy(ctx);

    return recv_count;
}

/* Helper: receive messages until termination message or error */
static int receive(slk_socket_t *socket, int *is_termination)
{
    int recv_count = 0;
    *is_termination = 0;

    /* Receive all sent messages with timeout to avoid hanging */
    char buffer[255];
    int len;
    while (1) {
        /* Poll with 10ms timeout (matches libzmq's RCVTIMEO) */
        if (!test_poll_readable(socket, 10)) {
            break;  /* Timeout - no more messages */
        }

        len = slk_recv(socket, buffer, sizeof(buffer), 0);
        if (len < 0) {
            break;
        }
        recv_count++;

        if (len == 3 && strncmp(buffer, "end", len) == 0) {
            *is_termination = 1;
            return recv_count;
        }
    }

    return recv_count;
}

/* Test: Blocking behavior with XPUB_NODROP */
static int test_blocking(int send_hwm, int msg_cnt, const char *endpoint)
{
    slk_ctx_t *ctx = test_context_new();

    /* Set up bind socket */
    slk_socket_t *pub_socket = test_socket_new(ctx, SLK_XPUB);
    test_socket_bind(pub_socket, endpoint);

    /* Set up connect socket */
    slk_socket_t *sub_socket = test_socket_new(ctx, SLK_SUB);
    int rc = slk_connect(sub_socket, endpoint);
    TEST_SUCCESS(rc);

    /* Set HWM on publisher */
    rc = slk_setsockopt(pub_socket, SLK_SNDHWM, &send_hwm, sizeof(send_hwm));
    TEST_SUCCESS(rc);

    /* Set XPUB_NODROP to enable blocking */
    int wait = 1;
    rc = slk_setsockopt(pub_socket, SLK_XPUB_NODROP, &wait, sizeof(wait));
    TEST_SUCCESS(rc);

    /* Note: ServerLink doesn't have SLK_RCVTIMEO, use polling instead */

    /* Subscribe to all messages */
    rc = slk_setsockopt(sub_socket, SLK_SUBSCRIBE, "", 0);
    TEST_SUCCESS(rc);

    /* Wait before starting TX operations till subscriber has subscribed */
    char sub_msg[2];
    rc = slk_recv(pub_socket, sub_msg, sizeof(sub_msg), 0);
    TEST_ASSERT(rc >= 1);

    /* Send until we block */
    int send_count = 0;
    int recv_count = 0;
    int blocked_count = 0;
    int is_termination = 0;

    while (send_count < msg_cnt) {
        rc = slk_send(pub_socket, NULL, 0, SLK_DONTWAIT);
        if (rc == 0) {
            send_count++;
        } else if (rc == -1) {
            /* If the PUB socket blocks due to HWM, errno should be SLK_EAGAIN */
            blocked_count++;
            TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);
            recv_count += receive(sub_socket, &is_termination);
        }
    }

    /* If send_hwm < msg_cnt, we should block at least once */
    TEST_ASSERT(blocked_count > 0);

    /* Dequeue SUB socket again, to make sure XPUB has space to send the termination message */
    recv_count += receive(sub_socket, &is_termination);

    /* Send termination message */
    rc = slk_send(pub_socket, "end", 3, 0);
    TEST_ASSERT(rc >= 0);

    /* Now block on the SUB side till we get the termination message */
    while (is_termination == 0) {
        recv_count += receive(sub_socket, &is_termination);
    }

    /* Remove termination message from the count */
    recv_count--;

    TEST_ASSERT_EQ(send_count, recv_count);

    /* Clean up */
    test_socket_close(sub_socket);
    test_socket_close(pub_socket);
    test_context_destroy(ctx);

    return recv_count;
}

/* Test: HWM should apply to the messages that have already been received
 * With HWM 11024: send 9999 msg, receive 9999, send 1100, receive 1100 */
static void test_reset_hwm()
{
    const int first_count = 9999;
    const int second_count = 1100;
    int hwm = 11024;

    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    /* Set up bind socket */
    slk_socket_t *pub_socket = test_socket_new(ctx, SLK_PUB);
    int rc = slk_setsockopt(pub_socket, SLK_SNDHWM, &hwm, sizeof(hwm));
    TEST_SUCCESS(rc);
    test_socket_bind(pub_socket, endpoint);

    /* Set up connect socket */
    slk_socket_t *sub_socket = test_socket_new(ctx, SLK_SUB);
    rc = slk_setsockopt(sub_socket, SLK_RCVHWM, &hwm, sizeof(hwm));
    TEST_SUCCESS(rc);
    rc = slk_connect(sub_socket, endpoint);
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(sub_socket, SLK_SUBSCRIBE, "", 0);
    TEST_SUCCESS(rc);

    test_sleep_ms(SETTLE_TIME);

    /* Send messages */
    int send_count = 0;
    while (send_count < first_count) {
        rc = slk_send(pub_socket, NULL, 0, SLK_DONTWAIT);
        if (rc < 0) {
            break;
        }
        send_count++;
    }
    TEST_ASSERT_EQ(first_count, send_count);

    test_sleep_ms(SETTLE_TIME);

    /* Now receive all sent messages */
    int recv_count = 0;
    while (1) {
        rc = slk_recv(sub_socket, NULL, 0, SLK_DONTWAIT);
        if (rc < 0) {
            break;
        }
        recv_count++;
    }
    TEST_ASSERT_EQ(first_count, recv_count);

    test_sleep_ms(SETTLE_TIME);

    /* Send messages */
    send_count = 0;
    while (send_count < second_count) {
        rc = slk_send(pub_socket, NULL, 0, SLK_DONTWAIT);
        if (rc < 0) {
            break;
        }
        send_count++;
    }
    TEST_ASSERT_EQ(second_count, send_count);

    test_sleep_ms(SETTLE_TIME);

    /* Now receive all sent messages */
    recv_count = 0;
    while (1) {
        rc = slk_recv(sub_socket, NULL, 0, SLK_DONTWAIT);
        if (rc < 0) {
            break;
        }
        recv_count++;
    }
    TEST_ASSERT_EQ(second_count, recv_count);

    /* Clean up */
    test_socket_close(sub_socket);
    test_socket_close(pub_socket);
    test_context_destroy(ctx);
}

/* Test: Send 1000 msg on hwm 1000, receive 1000 */
static void test_defaults_large_tcp()
{
    TEST_ASSERT_EQ(1000, test_defaults(1000, 1000, test_endpoint_tcp()));
}

/* Test: Send 100 msg on hwm 100, receive 100 */
static void test_defaults_small_tcp()
{
    TEST_ASSERT_EQ(100, test_defaults(100, 100, test_endpoint_tcp()));
}

/* Test: Send 6000 msg on hwm 2000, with blocking behavior */
static void test_blocking_tcp()
{
    TEST_ASSERT_EQ(6000, test_blocking(2000, 6000, test_endpoint_tcp()));
}

/* Test: Send 1000 msg on hwm 1000, receive 1000 (inproc) */
static void test_defaults_large_inproc()
{
    TEST_ASSERT_EQ(1000, test_defaults(1000, 1000, "inproc://test_hwm_pubsub"));
}

/* Test: Send 100 msg on hwm 100, receive 100 (inproc) */
static void test_defaults_small_inproc()
{
    TEST_ASSERT_EQ(100, test_defaults(100, 100, "inproc://test_hwm_pubsub2"));
}

/* Test: Send 6000 msg on hwm 2000, with blocking behavior (inproc) */
static void test_blocking_inproc()
{
    TEST_ASSERT_EQ(6000, test_blocking(2000, 6000, "inproc://test_hwm_pubsub3"));
}

int main()
{
#ifdef _WIN32
    /* Suppress Windows error dialogs for critical errors, GPF, and file open errors */
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif

    printf("=== ServerLink PUB/SUB HWM Tests ===\n\n");

    RUN_TEST(test_defaults_large_tcp);
    RUN_TEST(test_defaults_small_tcp);
    RUN_TEST(test_blocking_tcp);
    RUN_TEST(test_defaults_large_inproc);
    RUN_TEST(test_defaults_small_inproc);
    RUN_TEST(test_blocking_inproc);
    RUN_TEST(test_reset_hwm);

    printf("\n=== All PUB/SUB HWM Tests Passed ===\n");
    return 0;
}

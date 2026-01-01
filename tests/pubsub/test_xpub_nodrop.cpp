/* ServerLink XPUB NODROP Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/* Test: XPUB_NODROP prevents message loss by blocking */
static void test_xpub_nodrop()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create a publisher */
    slk_socket_t *pub = test_socket_new(ctx, SLK_XPUB);

    int hwm = 2000;
    int rc = slk_setsockopt(pub, SLK_SNDHWM, &hwm, sizeof(hwm));
    TEST_SUCCESS(rc);

    rc = slk_bind(pub, "inproc://test_xpub_nodrop");
    TEST_SUCCESS(rc);

    /* Set pub socket options - XPUB_NODROP to enable blocking */
    int wait = 1;
    rc = slk_setsockopt(pub, SLK_XPUB_NODROP, &wait, sizeof(wait));
    TEST_SUCCESS(rc);

    /* Create a subscriber */
    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);

    /* Set matching RCVHWM to avoid bottleneck - must be set BEFORE connect */
    rc = slk_setsockopt(sub, SLK_RCVHWM, &hwm, sizeof(hwm));
    TEST_SUCCESS(rc);

    rc = slk_connect(sub, "inproc://test_xpub_nodrop");
    TEST_SUCCESS(rc);

    /* Subscribe for all messages */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    TEST_SUCCESS(rc);

    /* We must wait for the subscription to be processed here, otherwise some
     * or all published messages might be lost */
    char sub_msg[2];
    rc = slk_recv(pub, sub_msg, sizeof(sub_msg), 0);
    TEST_ASSERT(rc >= 1);
    TEST_ASSERT_EQ(sub_msg[0], 1);  /* Subscription message */

    int hwmlimit = hwm - 1;
    int send_count = 0;

    /* Send empty messages up to HWM limit with DONTWAIT to avoid deadlock */
    /* Note: With blocking send on inproc + XPUB_NODROP, we can deadlock */
    /* if the receiver doesn't consume messages concurrently */
    for (int i = 0; i < hwmlimit; i++) {
        rc = slk_send(pub, NULL, 0, SLK_DONTWAIT);
        if (rc != 0) {
            /* If send fails, we've hit HWM earlier than expected */
            TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);
            break;
        }
        send_count++;
    }

    /* Give time for inproc messages to be delivered */
    test_sleep_ms(50);

    int recv_count = 0;
    /* Receive all messages using DONTWAIT */
    while (1) {
        rc = slk_recv(sub, NULL, 0, SLK_DONTWAIT);
        if (rc == -1) {
            TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);
            break;
        }
        TEST_ASSERT_EQ(0, rc);
        recv_count++;
    }

    TEST_ASSERT_EQ(send_count, recv_count);

    /* Now test real blocking behavior */
    /* Note: ServerLink doesn't have SLK_SNDTIMEO, use DONTWAIT */
    send_count = 0;
    recv_count = 0;
    hwmlimit = hwm;

    /* Send an empty message until we get an error, which must be SLK_EAGAIN */
    while (slk_send(pub, "", 0, SLK_DONTWAIT) == 0) {
        send_count++;
    }
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    if (send_count > 0) {
        /* Receive first message with blocking */
        rc = slk_recv(sub, NULL, 0, 0);
        TEST_SUCCESS(rc);
        recv_count++;

        while (slk_recv(sub, NULL, 0, SLK_DONTWAIT) == 0) {
            recv_count++;
        }
    }

    TEST_ASSERT_EQ(send_count, recv_count);

    /* Clean up */
    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

/* Test: XPUB without NODROP (default) - messages can be dropped */
static void test_xpub_default_drop()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create a publisher without NODROP */
    slk_socket_t *pub = test_socket_new(ctx, SLK_XPUB);

    int hwm = 100;
    int rc = slk_setsockopt(pub, SLK_SNDHWM, &hwm, sizeof(hwm));
    TEST_SUCCESS(rc);

    const char *endpoint = test_endpoint_tcp();
    test_socket_bind(pub, endpoint);

    /* Create a subscriber */
    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub, endpoint);
    TEST_SUCCESS(rc);

    /* Subscribe for all messages */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    TEST_SUCCESS(rc);

    /* Wait for subscription */
    char sub_msg[2];
    rc = slk_recv(pub, sub_msg, sizeof(sub_msg), 0);
    TEST_ASSERT(rc >= 1);

    test_sleep_ms(50);

    /* Send many messages without blocking (some will be dropped) */
    int send_count = 0;
    for (int i = 0; i < hwm * 3; i++) {
        rc = slk_send(pub, "test", 4, SLK_DONTWAIT);
        if (rc >= 0) {
            send_count++;
        }
    }

    /* We should have been able to send at least HWM messages */
    TEST_ASSERT(send_count >= hwm);

    test_sleep_ms(100);

    /* Receive messages - may be less than sent due to dropping */
    int recv_count = 0;
    char buff[16];
    while (1) {
        rc = slk_recv(sub, buff, sizeof(buff), SLK_DONTWAIT);
        if (rc < 0) {
            break;
        }
        recv_count++;
    }

    /* With default behavior (no NODROP), messages can be dropped */
    /* So recv_count might be less than send_count */
    printf("  Sent: %d, Received: %d (with drop)\n", send_count, recv_count);

    /* Clean up */
    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

/* Test: XPUB_NODROP with slow consumer */
static void test_xpub_nodrop_slow_consumer()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create a publisher with NODROP */
    slk_socket_t *pub = test_socket_new(ctx, SLK_XPUB);

    int hwm = 50;
    int rc = slk_setsockopt(pub, SLK_SNDHWM, &hwm, sizeof(hwm));
    TEST_SUCCESS(rc);

    int wait = 1;
    rc = slk_setsockopt(pub, SLK_XPUB_NODROP, &wait, sizeof(wait));
    TEST_SUCCESS(rc);

    const char *endpoint = test_endpoint_tcp();
    test_socket_bind(pub, endpoint);

    /* Create a subscriber */
    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub, endpoint);
    TEST_SUCCESS(rc);

    /* Subscribe for all messages */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    TEST_SUCCESS(rc);

    /* Wait for subscription */
    char sub_msg[2];
    rc = slk_recv(pub, sub_msg, sizeof(sub_msg), 0);
    TEST_ASSERT(rc >= 1);

    test_sleep_ms(50);

    /* Note: ServerLink doesn't have SLK_SNDTIMEO, use DONTWAIT */
    /* Send messages with DONTWAIT - should get EAGAIN when HWM is reached */
    int send_count = 0;
    int blocked = 0;
    for (int i = 0; i < hwm * 2; i++) {
        rc = slk_send(pub, "msg", 3, SLK_DONTWAIT);
        if (rc >= 0) {
            send_count++;
        } else {
            /* Should get SLK_EAGAIN when blocking on HWM */
            TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);
            blocked = 1;
            break;
        }
    }

    /* Should have blocked before sending all messages */
    TEST_ASSERT(blocked);
    printf("  Sent %d messages before blocking (HWM: %d)\n", send_count, hwm);

    /* Wait for TCP messages to be delivered */
    test_sleep_ms(100);

    /* Now drain the subscriber */
    int recv_count = 0;
    char buff[16];
    while (1) {
        rc = slk_recv(sub, buff, sizeof(buff), SLK_DONTWAIT);
        if (rc < 0) {
            break;
        }
        recv_count++;
    }

    /* With NODROP, all sent messages should be received */
    TEST_ASSERT_EQ(send_count, recv_count);

    /* Clean up */
    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

/* Test: XPUB_NODROP option can be toggled */
static void test_xpub_nodrop_toggle()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create a publisher */
    slk_socket_t *pub = test_socket_new(ctx, SLK_XPUB);

    /* Start with NODROP disabled (default) */
    int nodrop = 0;
    int rc = slk_setsockopt(pub, SLK_XPUB_NODROP, &nodrop, sizeof(nodrop));
    TEST_SUCCESS(rc);

    /* Verify the option is set correctly */
    int value = -1;
    size_t value_size = sizeof(value);
    rc = slk_getsockopt(pub, SLK_XPUB_NODROP, &value, &value_size);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(value, 0);

    /* Enable NODROP */
    nodrop = 1;
    rc = slk_setsockopt(pub, SLK_XPUB_NODROP, &nodrop, sizeof(nodrop));
    TEST_SUCCESS(rc);

    /* Verify it's enabled */
    rc = slk_getsockopt(pub, SLK_XPUB_NODROP, &value, &value_size);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(value, 1);

    /* Disable again */
    nodrop = 0;
    rc = slk_setsockopt(pub, SLK_XPUB_NODROP, &nodrop, sizeof(nodrop));
    TEST_SUCCESS(rc);

    /* Verify it's disabled */
    rc = slk_getsockopt(pub, SLK_XPUB_NODROP, &value, &value_size);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(value, 0);

    /* Clean up */
    test_socket_close(pub);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink XPUB NODROP Tests ===\n\n");

    RUN_TEST(test_xpub_nodrop);
    RUN_TEST(test_xpub_default_drop);
    RUN_TEST(test_xpub_nodrop_slow_consumer);
    RUN_TEST(test_xpub_nodrop_toggle);

    printf("\n=== All XPUB NODROP Tests Passed ===\n");
    return 0;
}

/* ServerLink XPUB MANUAL Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/* Helper: receive and verify subscription message with retry for DONTWAIT */
static void recv_subscription(slk_socket_t *socket, const uint8_t *expected, size_t len, int flags)
{
    char buff[32];
    int rc;

    // If DONTWAIT is specified, retry a few times with small delays
    // This handles the case where TCP messages haven't arrived yet
    if (flags & SLK_DONTWAIT) {
        for (int i = 0; i < 10; i++) {
            rc = slk_recv(socket, buff, sizeof(buff), flags);
            if (rc > 0) break;
            if (slk_errno() != SLK_EAGAIN) break;
            test_sleep_ms(50);  // Wait 50ms between retries
        }
    } else {
        rc = slk_recv(socket, buff, sizeof(buff), flags);
    }

    TEST_ASSERT_EQ(rc, (int)len);
    TEST_ASSERT_MEM_EQ(buff, expected, len);
}

/* Helper: send subscription message */
static void send_subscription(slk_socket_t *socket, const uint8_t *data, size_t len, int flags)
{
    int rc = slk_send(socket, data, len, flags);
    TEST_ASSERT(rc >= 0);
}

/* Helper: send string */
static void send_string(slk_socket_t *socket, const char *str, int flags)
{
    int rc = slk_send(socket, str, strlen(str), flags);
    TEST_ASSERT(rc >= 0);
}

/* Helper: receive string (with retry for timing issues) */
static void recv_string_dontwait(slk_socket_t *socket, const char *expected)
{
    char buff[32];
    int rc;

    // Retry with DONTWAIT to handle TCP timing issues
    for (int i = 0; i < 10; i++) {
        rc = slk_recv(socket, buff, sizeof(buff), SLK_DONTWAIT);
        if (rc > 0) break;
        if (slk_errno() != SLK_EAGAIN) break;
        test_sleep_ms(50);  // Wait 50ms between retries
    }

    TEST_ASSERT_EQ(rc, (int)strlen(expected));
    TEST_ASSERT_MEM_EQ(buff, expected, rc);
}

/* Test: Basic XPUB_MANUAL mode */
static void test_basic()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create a publisher */
    slk_socket_t *pub = test_socket_new(ctx, SLK_XPUB);
    int manual = 1;
    int rc = slk_setsockopt(pub, SLK_XPUB_MANUAL, &manual, sizeof(manual));
    TEST_SUCCESS(rc);
    rc = slk_bind(pub, "inproc://test_xpub_manual");
    TEST_SUCCESS(rc);

    /* Create a subscriber */
    slk_socket_t *sub = test_socket_new(ctx, SLK_XSUB);
    rc = slk_connect(sub, "inproc://test_xpub_manual");
    TEST_SUCCESS(rc);

    /* Subscribe for A */
    const uint8_t subscription[] = {1, 'A', 0};
    send_subscription(sub, subscription, sizeof(subscription), 0);

    /* Receive subscriptions from subscriber */
    recv_subscription(pub, subscription, sizeof(subscription), 0);

    /* Subscribe socket for B instead (manual override) */
    rc = slk_setsockopt(pub, SLK_SUBSCRIBE, "B", 1);
    TEST_SUCCESS(rc);

    /* Sending A message and B Message */
    send_string(pub, "A", 0);
    send_string(pub, "B", 0);

    /* Should only receive B (manual subscription) */
    recv_string_dontwait(sub, "B");

    /* Clean up */
    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

/* Test: Unsubscribe with manual mode */
static void test_unsubscribe_manual()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create a publisher */
    slk_socket_t *pub = test_socket_new(ctx, SLK_XPUB);
    int rc = slk_bind(pub, "inproc://test_xpub_manual_unsub");
    TEST_SUCCESS(rc);

    /* Set pub socket options */
    int manual = 1;
    rc = slk_setsockopt(pub, SLK_XPUB_MANUAL, &manual, sizeof(manual));
    TEST_SUCCESS(rc);

    /* Create a subscriber */
    slk_socket_t *sub = test_socket_new(ctx, SLK_XSUB);
    rc = slk_connect(sub, "inproc://test_xpub_manual_unsub");
    TEST_SUCCESS(rc);

    /* Subscribe for A */
    const uint8_t subscription1[] = {1, 'A'};
    send_subscription(sub, subscription1, sizeof(subscription1), 0);

    /* Subscribe for B */
    const uint8_t subscription2[] = {1, 'B'};
    send_subscription(sub, subscription2, sizeof(subscription2), 0);

    char buffer[3];

    /* Receive subscription "A" from subscriber */
    recv_subscription(pub, subscription1, sizeof(subscription1), 0);

    /* Subscribe socket for XA instead */
    rc = slk_setsockopt(pub, SLK_SUBSCRIBE, "XA", 2);
    TEST_SUCCESS(rc);

    /* Receive subscription "B" from subscriber */
    recv_subscription(pub, subscription2, sizeof(subscription2), 0);

    /* Subscribe socket for XB instead */
    rc = slk_setsockopt(pub, SLK_SUBSCRIBE, "XB", 2);
    TEST_SUCCESS(rc);

    /* Unsubscribe from A */
    const uint8_t unsubscription1[2] = {0, 'A'};
    send_subscription(sub, unsubscription1, sizeof(unsubscription1), 0);

    /* Receive unsubscription "A" from subscriber */
    recv_subscription(pub, unsubscription1, sizeof(unsubscription1), 0);

    /* Unsubscribe socket from XA instead */
    rc = slk_setsockopt(pub, SLK_UNSUBSCRIBE, "XA", 2);
    TEST_SUCCESS(rc);

    /* Sending messages XA, XB */
    send_string(pub, "XA", 0);
    send_string(pub, "XB", 0);

    /* Subscriber should receive XB only */
    recv_string_dontwait(sub, "XB");

    /* Close subscriber */
    test_socket_close(sub);

    /* Receive unsubscription "B" */
    const uint8_t unsubscription2[2] = {0, 'B'};
    rc = slk_recv(pub, buffer, sizeof(buffer), 0);
    TEST_ASSERT_EQ(rc, (int)sizeof(unsubscription2));
    TEST_ASSERT_MEM_EQ(buffer, unsubscription2, sizeof(unsubscription2));

    /* Unsubscribe socket from XB instead */
    rc = slk_setsockopt(pub, SLK_UNSUBSCRIBE, "XB", 2);
    TEST_SUCCESS(rc);

    /* Clean up */
    test_socket_close(pub);
    test_context_destroy(ctx);
}

/* Test: XPUB proxy unsubscribe on disconnect */
static void test_xpub_proxy_unsubscribe_on_disconnect()
{
    slk_ctx_t *ctx = test_context_new();

    const uint8_t topic_buff[] = {"1"};
    const uint8_t payload_buff[] = {"X"};

    const char *endpoint_backend = test_endpoint_tcp();
    const char *endpoint_frontend = test_endpoint_tcp();

    int manual = 1;

    /* Proxy frontend */
    slk_socket_t *xsub_proxy = test_socket_new(ctx, SLK_XSUB);
    test_socket_bind(xsub_proxy, endpoint_frontend);

    /* Proxy backend */
    slk_socket_t *xpub_proxy = test_socket_new(ctx, SLK_XPUB);
    int rc = slk_setsockopt(xpub_proxy, SLK_XPUB_MANUAL, &manual, sizeof(manual));
    TEST_SUCCESS(rc);
    test_socket_bind(xpub_proxy, endpoint_backend);

    /* Publisher */
    slk_socket_t *pub = test_socket_new(ctx, SLK_PUB);
    rc = slk_connect(pub, endpoint_frontend);
    TEST_SUCCESS(rc);

    /* First subscriber subscribes */
    slk_socket_t *sub1 = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub1, endpoint_backend);
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(sub1, SLK_SUBSCRIBE, topic_buff, 1);
    TEST_SUCCESS(rc);

    /* Wait */
    test_sleep_ms(SETTLE_TIME);

    /* Proxy reroutes and confirms subscriptions */
    const uint8_t subscription[2] = {1, *topic_buff};
    recv_subscription(xpub_proxy, subscription, sizeof(subscription), SLK_DONTWAIT);
    rc = slk_setsockopt(xpub_proxy, SLK_SUBSCRIBE, topic_buff, 1);
    TEST_SUCCESS(rc);
    send_subscription(xsub_proxy, subscription, sizeof(subscription), 0);

    /* Second subscriber subscribes */
    slk_socket_t *sub2 = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub2, endpoint_backend);
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(sub2, SLK_SUBSCRIBE, topic_buff, 1);
    TEST_SUCCESS(rc);

    /* Wait */
    test_sleep_ms(SETTLE_TIME);

    /* Proxy reroutes */
    recv_subscription(xpub_proxy, subscription, sizeof(subscription), SLK_DONTWAIT);
    rc = slk_setsockopt(xpub_proxy, SLK_SUBSCRIBE, topic_buff, 1);
    TEST_SUCCESS(rc);
    send_subscription(xsub_proxy, subscription, sizeof(subscription), 0);

    /* Wait */
    test_sleep_ms(SETTLE_TIME);

    /* Let publisher send a msg */
    send_subscription(pub, topic_buff, 1, SLK_SNDMORE);
    send_subscription(pub, payload_buff, 1, 0);

    /* Wait */
    test_sleep_ms(SETTLE_TIME);

    /* Proxy reroutes data messages to subscribers */
    recv_subscription(xsub_proxy, topic_buff, 1, SLK_DONTWAIT);
    recv_subscription(xsub_proxy, payload_buff, 1, SLK_DONTWAIT);
    send_subscription(xpub_proxy, topic_buff, 1, SLK_SNDMORE);
    send_subscription(xpub_proxy, payload_buff, 1, 0);

    /* Wait */
    test_sleep_ms(SETTLE_TIME);

    /* Each subscriber should now get a message */
    recv_subscription(sub2, topic_buff, 1, SLK_DONTWAIT);
    recv_subscription(sub2, payload_buff, 1, SLK_DONTWAIT);

    recv_subscription(sub1, topic_buff, 1, SLK_DONTWAIT);
    recv_subscription(sub1, payload_buff, 1, SLK_DONTWAIT);

    /* Disconnect both subscribers */
    test_socket_close(sub1);
    test_socket_close(sub2);

    /* Wait */
    test_sleep_ms(SETTLE_TIME);

    /* Unsubscribe messages are passed from proxy to publisher */
    const uint8_t unsubscription[] = {0, *topic_buff};
    recv_subscription(xpub_proxy, unsubscription, sizeof(unsubscription), 0);
    rc = slk_setsockopt(xpub_proxy, SLK_UNSUBSCRIBE, topic_buff, 1);
    TEST_SUCCESS(rc);
    send_subscription(xsub_proxy, unsubscription, sizeof(unsubscription), 0);

    /* Should receive another unsubscribe msg */
    recv_subscription(xpub_proxy, unsubscription, sizeof(unsubscription), 0);
    rc = slk_setsockopt(xpub_proxy, SLK_UNSUBSCRIBE, topic_buff, 1);
    TEST_SUCCESS(rc);
    send_subscription(xsub_proxy, unsubscription, sizeof(unsubscription), 0);

    /* Wait */
    test_sleep_ms(SETTLE_TIME);

    /* Let publisher send a msg */
    send_subscription(pub, topic_buff, 1, SLK_SNDMORE);
    send_subscription(pub, payload_buff, 1, 0);

    /* Wait */
    test_sleep_ms(SETTLE_TIME);

    /* Nothing should come to the proxy */
    char buffer[1];
    rc = slk_recv(xsub_proxy, buffer, sizeof(buffer), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    test_socket_close(pub);
    test_socket_close(xpub_proxy);
    test_socket_close(xsub_proxy);
    test_context_destroy(ctx);
}

/* Test: Unsubscribe cleanup */
static void test_unsubscribe_cleanup()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    /* Create a publisher */
    slk_socket_t *pub = test_socket_new(ctx, SLK_XPUB);
    int manual = 1;
    int rc = slk_setsockopt(pub, SLK_XPUB_MANUAL, &manual, sizeof(manual));
    TEST_SUCCESS(rc);
    test_socket_bind(pub, endpoint);

    /* Create a subscriber */
    slk_socket_t *sub = test_socket_new(ctx, SLK_XSUB);
    rc = slk_connect(sub, endpoint);
    TEST_SUCCESS(rc);

    /* Subscribe for A */
    const uint8_t subscription1[2] = {1, 'A'};
    send_subscription(sub, subscription1, sizeof(subscription1), 0);

    /* Receive subscriptions from subscriber */
    recv_subscription(pub, subscription1, sizeof(subscription1), 0);
    rc = slk_setsockopt(pub, SLK_SUBSCRIBE, "XA", 2);
    TEST_SUCCESS(rc);

    /* Send 2 messages */
    send_string(pub, "XA", 0);
    send_string(pub, "XB", 0);

    /* Receive the single message */
    recv_string_dontwait(sub, "XA");

    /* Should be nothing left in the queue */
    char buffer[2];
    rc = slk_recv(sub, buffer, sizeof(buffer), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Close the socket */
    test_socket_close(sub);

    /* Closing the socket will result in an unsubscribe event */
    const uint8_t unsubscription[2] = {0, 'A'};
    recv_subscription(pub, unsubscription, sizeof(unsubscription), 0);

    /* This doesn't really do anything */
    /* There is no last_pipe set, it will just fail silently */
    rc = slk_setsockopt(pub, SLK_UNSUBSCRIBE, "XA", 2);
    TEST_SUCCESS(rc);

    /* Reconnect */
    sub = test_socket_new(ctx, SLK_XSUB);
    rc = slk_connect(sub, endpoint);
    TEST_SUCCESS(rc);

    /* Send a subscription for B */
    const uint8_t subscription2[2] = {1, 'B'};
    send_subscription(sub, subscription2, sizeof(subscription2), 0);

    /* Receive the subscription, overwrite it to XB */
    recv_subscription(pub, subscription2, sizeof(subscription2), 0);
    rc = slk_setsockopt(pub, SLK_SUBSCRIBE, "XB", 2);
    TEST_SUCCESS(rc);

    /* Send 2 messages */
    send_string(pub, "XA", 0);
    send_string(pub, "XB", 0);

    /* Receive the single message */
    recv_string_dontwait(sub, "XB");

    /* Should be nothing left in the queue */
    rc = slk_recv(sub, buffer, sizeof(buffer), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Clean up */
    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink XPUB MANUAL Tests ===\n\n");
    fflush(stdout);

    RUN_TEST(test_basic);
    RUN_TEST(test_unsubscribe_manual);
    RUN_TEST(test_xpub_proxy_unsubscribe_on_disconnect);
    RUN_TEST(test_unsubscribe_cleanup);

    printf("\n=== All XPUB MANUAL Tests Passed ===\n");
    return 0;
}

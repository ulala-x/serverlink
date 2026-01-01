/* ServerLink SUB Forward (XPUB-XSUB Proxy) Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

#define SETTLE_TIME 100

/* Test: XPUB-XSUB proxy pattern - subscription forwarding */
static void test_sub_forward()
{
    slk_ctx_t *ctx = test_context_new();

    const char *endpoint1 = test_endpoint_tcp();
    const char *endpoint2 = test_endpoint_tcp();

    /* First, create an intermediate device (proxy) */
    slk_socket_t *xpub = test_socket_new(ctx, SLK_XPUB);
    test_socket_bind(xpub, endpoint1);

    slk_socket_t *xsub = test_socket_new(ctx, SLK_XSUB);
    test_socket_bind(xsub, endpoint2);

    /* Create a publisher */
    slk_socket_t *pub = test_socket_new(ctx, SLK_PUB);
    int rc = slk_connect(pub, endpoint2);
    TEST_SUCCESS(rc);

    /* Create a subscriber */
    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub, endpoint1);
    TEST_SUCCESS(rc);

    /* Subscribe for all messages */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    TEST_SUCCESS(rc);

    /* Pass the subscription upstream through the device */
    char buff[32];
    int size;

    /* Receive subscription from SUB on XPUB */
    size = slk_recv(xpub, buff, sizeof(buff), 0);
    TEST_ASSERT(size > 0);
    TEST_ASSERT_EQ(buff[0], 1);  /* Subscribe message */

    /* Forward subscription to XSUB */
    rc = slk_send(xsub, buff, size, 0);
    TEST_ASSERT(rc >= 0);

    /* Wait a bit till the subscription gets to the publisher */
    test_sleep_ms(SETTLE_TIME);

    /* Send an empty message */
    rc = slk_send(pub, "", 0, 0);
    TEST_ASSERT(rc >= 0);

    /* Pass the message downstream through the device */
    size = slk_recv(xsub, buff, sizeof(buff), 0);
    TEST_SUCCESS(size);

    rc = slk_send(xpub, buff, size, 0);
    TEST_ASSERT(rc >= 0);

    /* Receive the message in the subscriber */
    char msg[32];
    size = slk_recv(sub, msg, sizeof(msg), 0);
    TEST_ASSERT_EQ(size, 0);  /* Empty message */

    /* Clean up */
    test_socket_close(xpub);
    test_socket_close(xsub);
    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

/* Test: XPUB-XSUB proxy with multiple topics */
static void test_sub_forward_multi_topic()
{
    slk_ctx_t *ctx = test_context_new();

    const char *endpoint1 = test_endpoint_tcp();
    const char *endpoint2 = test_endpoint_tcp();

    /* Create intermediate device (proxy) */
    slk_socket_t *xpub = test_socket_new(ctx, SLK_XPUB);
    test_socket_bind(xpub, endpoint1);

    slk_socket_t *xsub = test_socket_new(ctx, SLK_XSUB);
    test_socket_bind(xsub, endpoint2);

    /* Create a publisher */
    slk_socket_t *pub = test_socket_new(ctx, SLK_PUB);
    int rc = slk_connect(pub, endpoint2);
    TEST_SUCCESS(rc);

    /* Create a subscriber */
    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub, endpoint1);
    TEST_SUCCESS(rc);

    /* Subscribe for topic "A" */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "A", 1);
    TEST_SUCCESS(rc);

    /* Pass the subscription upstream through the device */
    char buff[32];
    int size;

    size = slk_recv(xpub, buff, sizeof(buff), 0);
    TEST_ASSERT(size >= 2);
    TEST_ASSERT_EQ(buff[0], 1);  /* Subscribe */
    TEST_ASSERT_EQ(buff[1], 'A'); /* Topic A */

    rc = slk_send(xsub, buff, size, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(SETTLE_TIME);

    /* Send message with topic "A" */
    rc = slk_send(pub, "A", 1, 0);
    TEST_ASSERT(rc >= 0);

    /* Forward through proxy */
    size = slk_recv(xsub, buff, sizeof(buff), 0);
    TEST_ASSERT(size == 1);
    TEST_ASSERT_EQ(buff[0], 'A');

    rc = slk_send(xpub, buff, size, 0);
    TEST_ASSERT(rc >= 0);

    /* Receive on subscriber */
    char msg[32];
    size = slk_recv(sub, msg, sizeof(msg), 0);
    TEST_ASSERT_EQ(size, 1);
    TEST_ASSERT_EQ(msg[0], 'A');

    /* Send message with topic "B" (not subscribed) */
    rc = slk_send(pub, "B", 1, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* Should not receive on XSUB since no subscription for B */
    size = slk_recv(xsub, buff, sizeof(buff), SLK_DONTWAIT);
    TEST_ASSERT(size < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Clean up */
    test_socket_close(xpub);
    test_socket_close(xsub);
    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

/* Test: XPUB-XSUB proxy with multipart messages */
static void test_sub_forward_multipart()
{
    slk_ctx_t *ctx = test_context_new();

    const char *endpoint1 = test_endpoint_tcp();
    const char *endpoint2 = test_endpoint_tcp();

    /* Create intermediate device (proxy) */
    slk_socket_t *xpub = test_socket_new(ctx, SLK_XPUB);
    test_socket_bind(xpub, endpoint1);

    slk_socket_t *xsub = test_socket_new(ctx, SLK_XSUB);
    test_socket_bind(xsub, endpoint2);

    /* Create a publisher */
    slk_socket_t *pub = test_socket_new(ctx, SLK_PUB);
    int rc = slk_connect(pub, endpoint2);
    TEST_SUCCESS(rc);

    /* Create a subscriber */
    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub, endpoint1);
    TEST_SUCCESS(rc);

    /* Subscribe for all messages */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    TEST_SUCCESS(rc);

    /* Pass subscription through proxy */
    char buff[32];
    int size = slk_recv(xpub, buff, sizeof(buff), 0);
    TEST_ASSERT(size > 0);
    rc = slk_send(xsub, buff, size, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(SETTLE_TIME);

    /* Send a multipart message */
    rc = slk_send(pub, "TOPIC", 5, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(pub, "PAYLOAD", 7, 0);
    TEST_ASSERT(rc >= 0);

    /* Forward both parts through proxy */
    /* First part */
    size = slk_recv(xsub, buff, sizeof(buff), 0);
    TEST_ASSERT_EQ(size, 5);
    TEST_ASSERT_MEM_EQ(buff, "TOPIC", 5);

    rc = slk_send(xpub, buff, size, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);

    /* Second part */
    size = slk_recv(xsub, buff, sizeof(buff), 0);
    TEST_ASSERT_EQ(size, 7);
    TEST_ASSERT_MEM_EQ(buff, "PAYLOAD", 7);

    rc = slk_send(xpub, buff, size, 0);
    TEST_ASSERT(rc >= 0);

    /* Receive on subscriber */
    char msg[32];
    size = slk_recv(sub, msg, sizeof(msg), 0);
    TEST_ASSERT_EQ(size, 5);
    TEST_ASSERT_MEM_EQ(msg, "TOPIC", 5);

    size = slk_recv(sub, msg, sizeof(msg), 0);
    TEST_ASSERT_EQ(size, 7);
    TEST_ASSERT_MEM_EQ(msg, "PAYLOAD", 7);

    /* Clean up */
    test_socket_close(xpub);
    test_socket_close(xsub);
    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink SUB Forward (XPUB-XSUB Proxy) Tests ===\n\n");

    RUN_TEST(test_sub_forward);
    RUN_TEST(test_sub_forward_multi_topic);
    RUN_TEST(test_sub_forward_multipart);

    printf("\n=== All SUB Forward Tests Passed ===\n");
    return 0;
}

/* ServerLink PUB/SUB Topics Count Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"
#include <string.h>

#define SETTLE_TIME 100

/* Helper: settle subscriptions and get topic count */
static void settle_subscriptions(slk_socket_t *skt)
{
    /* To kick the application thread, just sleep */
    /* Users should use the monitor and other sockets in a poll */
    test_sleep_ms(SETTLE_TIME);
    (void)skt;  /* Suppress unused parameter warning */
}

static int get_subscription_count(slk_socket_t *skt)
{
    int num_subs = 0;
    size_t num_subs_len = sizeof(num_subs);

    settle_subscriptions(skt);
    int rc = slk_getsockopt(skt, SLK_TOPICS_COUNT, &num_subs, &num_subs_len);
    TEST_SUCCESS(rc);

    return num_subs;
}

/* Test: Independent topic prefixes */
static void test_independent_topic_prefixes()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create a publisher */
    slk_socket_t *publisher = test_socket_new(ctx, SLK_PUB);
    const char *endpoint = "inproc://test_topics_independent";

    /* Bind publisher */
    test_socket_bind(publisher, endpoint);

    /* Create a subscriber */
    slk_socket_t *subscriber = test_socket_new(ctx, SLK_SUB);
    int rc = slk_connect(subscriber, endpoint);
    TEST_SUCCESS(rc);

    /* Subscribe to 3 topics */
    rc = slk_setsockopt(subscriber, SLK_SUBSCRIBE, "topicprefix1", strlen("topicprefix1"));
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(subscriber, SLK_SUBSCRIBE, "topicprefix2", strlen("topicprefix2"));
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(subscriber, SLK_SUBSCRIBE, "topicprefix3", strlen("topicprefix3"));
    TEST_SUCCESS(rc);

    TEST_ASSERT_EQ(get_subscription_count(subscriber), 3);
    TEST_ASSERT_EQ(get_subscription_count(publisher), 3);

    /* Remove first subscription and check subscriptions went 3 -> 2 */
    rc = slk_setsockopt(subscriber, SLK_UNSUBSCRIBE, "topicprefix3", strlen("topicprefix3"));
    TEST_SUCCESS(rc);

    TEST_ASSERT_EQ(get_subscription_count(subscriber), 2);
    TEST_ASSERT_EQ(get_subscription_count(publisher), 2);

    /* Remove other 2 subscriptions and check we're back to 0 subscriptions */
    rc = slk_setsockopt(subscriber, SLK_UNSUBSCRIBE, "topicprefix1", strlen("topicprefix1"));
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(subscriber, SLK_UNSUBSCRIBE, "topicprefix2", strlen("topicprefix2"));
    TEST_SUCCESS(rc);

    TEST_ASSERT_EQ(get_subscription_count(subscriber), 0);
    TEST_ASSERT_EQ(get_subscription_count(publisher), 0);

    /* Clean up */
    test_socket_close(publisher);
    test_socket_close(subscriber);
    test_context_destroy(ctx);
}

/* Test: Nested topic prefixes */
static void test_nested_topic_prefixes()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create a publisher */
    slk_socket_t *publisher = test_socket_new(ctx, SLK_PUB);
    const char *endpoint = "inproc://test_topics_nested";

    /* Bind publisher */
    test_socket_bind(publisher, endpoint);

    /* Create a subscriber */
    slk_socket_t *subscriber = test_socket_new(ctx, SLK_SUB);
    int rc = slk_connect(subscriber, endpoint);
    TEST_SUCCESS(rc);

    /* Subscribe to 3 (nested) topics */
    rc = slk_setsockopt(subscriber, SLK_SUBSCRIBE, "a", strlen("a"));
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(subscriber, SLK_SUBSCRIBE, "ab", strlen("ab"));
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(subscriber, SLK_SUBSCRIBE, "abc", strlen("abc"));
    TEST_SUCCESS(rc);

    /* Even if the subscriptions are nested one into the other, the number of subscriptions
     * received on the subscriber/publisher socket will be 3: */
    TEST_ASSERT_EQ(get_subscription_count(subscriber), 3);
    TEST_ASSERT_EQ(get_subscription_count(publisher), 3);

    /* Subscribe to other 3 (nested) topics */
    rc = slk_setsockopt(subscriber, SLK_SUBSCRIBE, "xyz", strlen("xyz"));
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(subscriber, SLK_SUBSCRIBE, "xy", strlen("xy"));
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(subscriber, SLK_SUBSCRIBE, "x", strlen("x"));
    TEST_SUCCESS(rc);

    TEST_ASSERT_EQ(get_subscription_count(subscriber), 6);
    TEST_ASSERT_EQ(get_subscription_count(publisher), 6);

    /* Clean up */
    test_socket_close(publisher);
    test_socket_close(subscriber);
    test_context_destroy(ctx);
}

/* Test: Verify message delivery with topics */
static void test_topic_message_delivery()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create publisher and subscriber */
    slk_socket_t *publisher = test_socket_new(ctx, SLK_PUB);
    const char *endpoint = test_endpoint_tcp();
    test_socket_bind(publisher, endpoint);

    slk_socket_t *subscriber = test_socket_new(ctx, SLK_SUB);
    int rc = slk_connect(subscriber, endpoint);
    TEST_SUCCESS(rc);

    /* Subscribe to topic "weather" */
    rc = slk_setsockopt(subscriber, SLK_SUBSCRIBE, "weather", strlen("weather"));
    TEST_SUCCESS(rc);

    /* Wait for subscription to propagate */
    test_sleep_ms(SETTLE_TIME);

    /* Send message with matching topic */
    rc = slk_send(publisher, "weather sunny", 13, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* Receive message */
    char msg[64];
    rc = slk_recv(subscriber, msg, sizeof(msg), 0);
    TEST_ASSERT_EQ(rc, 13);
    msg[rc] = '\0';
    TEST_ASSERT_STR_EQ(msg, "weather sunny");

    /* Send message with non-matching topic */
    rc = slk_send(publisher, "news breaking", 13, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* Should not receive */
    rc = slk_recv(subscriber, msg, sizeof(msg), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Clean up */
    test_socket_close(publisher);
    test_socket_close(subscriber);
    test_context_destroy(ctx);
}

/* Test: Multiple subscribers with different topics */
static void test_multiple_subscribers()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create publisher */
    slk_socket_t *publisher = test_socket_new(ctx, SLK_PUB);
    const char *endpoint = test_endpoint_tcp();
    test_socket_bind(publisher, endpoint);

    /* Create first subscriber for "A" */
    slk_socket_t *sub_a = test_socket_new(ctx, SLK_SUB);
    int rc = slk_connect(sub_a, endpoint);
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(sub_a, SLK_SUBSCRIBE, "A", 1);
    TEST_SUCCESS(rc);

    /* Create second subscriber for "B" */
    slk_socket_t *sub_b = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub_b, endpoint);
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(sub_b, SLK_SUBSCRIBE, "B", 1);
    TEST_SUCCESS(rc);

    /* Wait for subscriptions */
    test_sleep_ms(SETTLE_TIME);

    /* Send message with topic "A" */
    rc = slk_send(publisher, "A-message", 9, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* sub_a should receive */
    char msg[64];
    rc = slk_recv(sub_a, msg, sizeof(msg), 0);
    TEST_ASSERT_EQ(rc, 9);
    TEST_ASSERT_MEM_EQ(msg, "A-message", 9);

    /* sub_b should not receive */
    rc = slk_recv(sub_b, msg, sizeof(msg), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Send message with topic "B" */
    rc = slk_send(publisher, "B-message", 9, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* sub_b should receive */
    rc = slk_recv(sub_b, msg, sizeof(msg), 0);
    TEST_ASSERT_EQ(rc, 9);
    TEST_ASSERT_MEM_EQ(msg, "B-message", 9);

    /* sub_a should not receive */
    rc = slk_recv(sub_a, msg, sizeof(msg), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Clean up */
    test_socket_close(publisher);
    test_socket_close(sub_a);
    test_socket_close(sub_b);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink PUB/SUB Topics Count Tests ===\n\n");

    RUN_TEST(test_independent_topic_prefixes);
    RUN_TEST(test_nested_topic_prefixes);
    RUN_TEST(test_topic_message_delivery);
    RUN_TEST(test_multiple_subscribers);

    printf("\n=== All PUB/SUB Topics Count Tests Passed ===\n");
    return 0;
}

/* ServerLink SPOT PUB/SUB Basic Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/* Test: Create and destroy SPOT instance */
static void test_spot_create_destroy()
{
    slk_ctx_t *ctx = test_context_new();

    slk_spot_t *spot = slk_spot_new(ctx);
    TEST_ASSERT_NOT_NULL(spot);

    slk_spot_destroy(&spot);
    TEST_ASSERT_NULL(spot);

    test_context_destroy(ctx);
}

/* Test: Create topic */
static void test_spot_topic_create()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    int rc = slk_spot_topic_create(spot, "test:topic");
    TEST_SUCCESS(rc);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Create multiple topics */
static void test_spot_topic_create_multiple()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    int rc;
    rc = slk_spot_topic_create(spot, "topic1");
    TEST_SUCCESS(rc);

    rc = slk_spot_topic_create(spot, "topic2");
    TEST_SUCCESS(rc);

    rc = slk_spot_topic_create(spot, "topic3");
    TEST_SUCCESS(rc);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Subscribe to topic */
static void test_spot_subscribe()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    /* Create topic first */
    int rc = slk_spot_topic_create(spot, "test:topic");
    TEST_SUCCESS(rc);

    /* Subscribe */
    rc = slk_spot_subscribe(spot, "test:topic");
    TEST_SUCCESS(rc);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Subscribe to multiple topics */
static void test_spot_subscribe_multiple()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    int rc;
    rc = slk_spot_topic_create(spot, "topic1");
    TEST_SUCCESS(rc);
    rc = slk_spot_topic_create(spot, "topic2");
    TEST_SUCCESS(rc);
    rc = slk_spot_topic_create(spot, "topic3");
    TEST_SUCCESS(rc);

    rc = slk_spot_subscribe(spot, "topic1");
    TEST_SUCCESS(rc);
    rc = slk_spot_subscribe(spot, "topic2");
    TEST_SUCCESS(rc);
    rc = slk_spot_subscribe(spot, "topic3");
    TEST_SUCCESS(rc);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Unsubscribe from topic */
static void test_spot_unsubscribe()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    int rc;
    rc = slk_spot_topic_create(spot, "test:topic");
    TEST_SUCCESS(rc);

    rc = slk_spot_subscribe(spot, "test:topic");
    TEST_SUCCESS(rc);

    rc = slk_spot_unsubscribe(spot, "test:topic");
    TEST_SUCCESS(rc);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Pattern subscription */
static void test_spot_subscribe_pattern()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    /* Subscribe to pattern */
    int rc = slk_spot_subscribe_pattern(spot, "events:*");
    TEST_SUCCESS(rc);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Basic publish/subscribe */
static void test_spot_basic_pubsub()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    /* Create and subscribe to topic */
    int rc = slk_spot_topic_create(spot, "test:topic");
    TEST_SUCCESS(rc);

    rc = slk_spot_subscribe(spot, "test:topic");
    TEST_SUCCESS(rc);

    /* Publish message */
    const char *msg = "hello world";
    rc = slk_spot_publish(spot, "test:topic", msg, strlen(msg));
    TEST_SUCCESS(rc);

    /* Receive message */
    test_sleep_ms(50);

    char topic[64], data[256];
    size_t topic_len, data_len;
    rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 100);
    TEST_SUCCESS(rc);

    topic[topic_len] = '\0';
    TEST_ASSERT_STR_EQ(topic, "test:topic");
    TEST_ASSERT_EQ(data_len, strlen(msg));
    TEST_ASSERT_MEM_EQ(data, msg, data_len);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Publish to non-existent topic should fail */
static void test_spot_publish_nonexistent()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    /* Try to publish to topic that doesn't exist */
    const char *msg = "hello";
    int rc = slk_spot_publish(spot, "nonexistent", msg, strlen(msg));
    TEST_FAILURE(rc);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Multiple messages */
static void test_spot_multiple_messages()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    int rc = slk_spot_topic_create(spot, "test:topic");
    TEST_SUCCESS(rc);

    rc = slk_spot_subscribe(spot, "test:topic");
    TEST_SUCCESS(rc);

    /* Publish multiple messages */
    for (int i = 0; i < 10; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "message %d", i);
        rc = slk_spot_publish(spot, "test:topic", msg, strlen(msg));
        TEST_SUCCESS(rc);
    }

    test_sleep_ms(100);

    /* Receive all messages */
    for (int i = 0; i < 10; i++) {
        char topic[64], data[256];
        size_t topic_len, data_len;

        rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                          data, sizeof(data), &data_len, 100);
        TEST_SUCCESS(rc);

        char expected[32];
        snprintf(expected, sizeof(expected), "message %d", i);

        topic[topic_len] = '\0';
        data[data_len] = '\0';

        TEST_ASSERT_STR_EQ(topic, "test:topic");
        TEST_ASSERT_STR_EQ(data, expected);
    }

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Destroy topic */
static void test_spot_topic_destroy()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    int rc = slk_spot_topic_create(spot, "test:topic");
    TEST_SUCCESS(rc);

    rc = slk_spot_topic_destroy(spot, "test:topic");
    TEST_SUCCESS(rc);

    /* Publishing to destroyed topic should fail */
    const char *msg = "hello";
    rc = slk_spot_publish(spot, "test:topic", msg, strlen(msg));
    TEST_FAILURE(rc);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink SPOT Basic Tests ===\n\n");

    RUN_TEST(test_spot_create_destroy);
    RUN_TEST(test_spot_topic_create);
    RUN_TEST(test_spot_topic_create_multiple);
    RUN_TEST(test_spot_subscribe);
    RUN_TEST(test_spot_subscribe_multiple);
    RUN_TEST(test_spot_unsubscribe);
    RUN_TEST(test_spot_subscribe_pattern);
    RUN_TEST(test_spot_basic_pubsub);
    RUN_TEST(test_spot_publish_nonexistent);
    RUN_TEST(test_spot_multiple_messages);
    RUN_TEST(test_spot_topic_destroy);

    printf("\n=== All SPOT Basic Tests Passed ===\n");
    return 0;
}

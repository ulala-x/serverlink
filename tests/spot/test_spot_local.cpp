/* ServerLink SPOT Local PUB/SUB Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/* Test: Multiple topics on single SPOT instance */
static void test_spot_multi_topic()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    /* Create multiple topics */
    int rc;
    rc = slk_spot_topic_create(spot, "events:login");
    TEST_SUCCESS(rc);
    rc = slk_spot_topic_create(spot, "events:logout");
    TEST_SUCCESS(rc);
    rc = slk_spot_topic_create(spot, "metrics:cpu");
    TEST_SUCCESS(rc);

    /* Subscribe to all */
    rc = slk_spot_subscribe(spot, "events:login");
    TEST_SUCCESS(rc);
    rc = slk_spot_subscribe(spot, "events:logout");
    TEST_SUCCESS(rc);
    rc = slk_spot_subscribe(spot, "metrics:cpu");
    TEST_SUCCESS(rc);

    /* Publish to each topic */
    rc = slk_spot_publish(spot, "events:login", "user1", 5);
    TEST_SUCCESS(rc);
    rc = slk_spot_publish(spot, "events:logout", "user2", 5);
    TEST_SUCCESS(rc);
    rc = slk_spot_publish(spot, "metrics:cpu", "85%", 3);
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Receive messages (order may vary) */
    char received_topics[3][64] = {0};
    char received_data[3][256] = {0};

    for (int i = 0; i < 3; i++) {
        char topic[64], data[256];
        size_t topic_len, data_len;

        rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                          data, sizeof(data), &data_len, 100);
        TEST_SUCCESS(rc);

        memcpy(received_topics[i], topic, topic_len);
        received_topics[i][topic_len] = '\0';
        memcpy(received_data[i], data, data_len);
        received_data[i][data_len] = '\0';
    }

    /* Verify all messages received (order-independent) */
    int found_login = 0, found_logout = 0, found_cpu = 0;
    for (int i = 0; i < 3; i++) {
        if (strcmp(received_topics[i], "events:login") == 0) {
            TEST_ASSERT_STR_EQ(received_data[i], "user1");
            found_login = 1;
        } else if (strcmp(received_topics[i], "events:logout") == 0) {
            TEST_ASSERT_STR_EQ(received_data[i], "user2");
            found_logout = 1;
        } else if (strcmp(received_topics[i], "metrics:cpu") == 0) {
            TEST_ASSERT_STR_EQ(received_data[i], "85%");
            found_cpu = 1;
        }
    }

    TEST_ASSERT(found_login && found_logout && found_cpu);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Multiple subscribers to same topic via TCP
 *
 * In SPOT design:
 * - Publisher creates topic locally and binds to external endpoint
 * - Subscribers route to publisher's endpoint and subscribe
 */
static void test_spot_multi_subscriber()
{
    slk_ctx_t *ctx = test_context_new();

    /* Publisher creates topic and binds to TCP endpoint */
    slk_spot_t *pub = slk_spot_new(ctx);
    TEST_ASSERT_NOT_NULL(pub);

    const char *pub_endpoint = test_endpoint_tcp();

    int rc = slk_spot_topic_create(pub, "broadcast");
    TEST_SUCCESS(rc);

    rc = slk_spot_bind(pub, pub_endpoint);
    TEST_SUCCESS(rc);

    test_sleep_ms(SETTLE_TIME);

    /* Create subscribers that route to publisher */
    slk_spot_t *sub1 = slk_spot_new(ctx);
    slk_spot_t *sub2 = slk_spot_new(ctx);
    TEST_ASSERT_NOT_NULL(sub1);
    TEST_ASSERT_NOT_NULL(sub2);

    /* Each subscriber routes to publisher's endpoint */
    rc = slk_spot_topic_route(sub1, "broadcast", pub_endpoint);
    TEST_SUCCESS(rc);
    rc = slk_spot_topic_route(sub2, "broadcast", pub_endpoint);
    TEST_SUCCESS(rc);

    /* Subscribers subscribe */
    rc = slk_spot_subscribe(sub1, "broadcast");
    TEST_SUCCESS(rc);
    rc = slk_spot_subscribe(sub2, "broadcast");
    TEST_SUCCESS(rc);

    test_sleep_ms(SETTLE_TIME);

    /* Publish message */
    const char *msg = "message to all";
    rc = slk_spot_publish(pub, "broadcast", msg, strlen(msg));
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Both subscribers should receive */
    char topic1[64], data1[256];
    size_t topic1_len, data1_len;
    rc = slk_spot_recv(sub1, topic1, sizeof(topic1), &topic1_len,
                       data1, sizeof(data1), &data1_len, 100);
    TEST_SUCCESS(rc);

    char topic2[64], data2[256];
    size_t topic2_len, data2_len;
    rc = slk_spot_recv(sub2, topic2, sizeof(topic2), &topic2_len,
                       data2, sizeof(data2), &data2_len, 100);
    TEST_SUCCESS(rc);

    topic1[topic1_len] = '\0';
    data1[data1_len] = '\0';
    topic2[topic2_len] = '\0';
    data2[data2_len] = '\0';

    TEST_ASSERT_STR_EQ(topic1, "broadcast");
    TEST_ASSERT_STR_EQ(data1, msg);
    TEST_ASSERT_STR_EQ(topic2, "broadcast");
    TEST_ASSERT_STR_EQ(data2, msg);

    slk_spot_destroy(&pub);
    slk_spot_destroy(&sub1);
    slk_spot_destroy(&sub2);
    test_context_destroy(ctx);
}

/* Test: Pattern matching subscription */
static void test_spot_pattern_matching()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    /* Create topics with pattern */
    int rc;
    rc = slk_spot_topic_create(spot, "events:login");
    TEST_SUCCESS(rc);
    rc = slk_spot_topic_create(spot, "events:logout");
    TEST_SUCCESS(rc);
    rc = slk_spot_topic_create(spot, "metrics:cpu");
    TEST_SUCCESS(rc);

    /* Subscribe using pattern (only events:*) */
    rc = slk_spot_subscribe_pattern(spot, "events:*");
    TEST_SUCCESS(rc);

    /* Publish to all topics */
    rc = slk_spot_publish(spot, "events:login", "data1", 5);
    TEST_SUCCESS(rc);
    rc = slk_spot_publish(spot, "events:logout", "data2", 5);
    TEST_SUCCESS(rc);
    rc = slk_spot_publish(spot, "metrics:cpu", "data3", 5);
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Should only receive events:* messages */
    int received_count = 0;
    while (received_count < 2) {
        char topic[64], data[256];
        size_t topic_len, data_len;

        rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                          data, sizeof(data), &data_len, 100);
        if (rc != 0) break;

        topic[topic_len] = '\0';

        /* Should be events:login or events:logout */
        TEST_ASSERT(strncmp(topic, "events:", 7) == 0);

        received_count++;
    }

    TEST_ASSERT_EQ(received_count, 2);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Selective unsubscribe */
static void test_spot_selective_unsubscribe()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    /* Create and subscribe to multiple topics */
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

    /* Unsubscribe from topic2 */
    rc = slk_spot_unsubscribe(spot, "topic2");
    TEST_SUCCESS(rc);

    /* Publish to all topics */
    rc = slk_spot_publish(spot, "topic1", "msg1", 4);
    TEST_SUCCESS(rc);
    rc = slk_spot_publish(spot, "topic2", "msg2", 4);
    TEST_SUCCESS(rc);
    rc = slk_spot_publish(spot, "topic3", "msg3", 4);
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Should only receive topic1 and topic3 */
    int received_count = 0;
    while (received_count < 2) {
        char topic[64], data[256];
        size_t topic_len, data_len;

        rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                          data, sizeof(data), &data_len, 100);
        if (rc != 0) break;

        topic[topic_len] = '\0';

        /* Should not be topic2 */
        TEST_ASSERT(strcmp(topic, "topic2") != 0);

        received_count++;
    }

    TEST_ASSERT_EQ(received_count, 2);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Large message handling */
static void test_spot_large_message()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    int rc = slk_spot_topic_create(spot, "large");
    TEST_SUCCESS(rc);

    rc = slk_spot_subscribe(spot, "large");
    TEST_SUCCESS(rc);

    /* Create large message (1MB) */
    size_t large_size = 1024 * 1024;
    char *large_data = (char*)malloc(large_size);
    TEST_ASSERT_NOT_NULL(large_data);

    /* Fill with pattern */
    for (size_t i = 0; i < large_size; i++) {
        large_data[i] = (char)(i % 256);
    }

    /* Publish large message */
    rc = slk_spot_publish(spot, "large", large_data, large_size);
    TEST_SUCCESS(rc);

    test_sleep_ms(200);

    /* Receive large message */
    char topic[64];
    char *recv_data = (char*)malloc(large_size);
    TEST_ASSERT_NOT_NULL(recv_data);

    size_t topic_len, data_len;
    rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                       recv_data, large_size, &data_len, 500);
    TEST_SUCCESS(rc);

    topic[topic_len] = '\0';
    TEST_ASSERT_STR_EQ(topic, "large");
    TEST_ASSERT_EQ(data_len, large_size);
    TEST_ASSERT_MEM_EQ(recv_data, large_data, large_size);

    free(large_data);
    free(recv_data);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

/* Test: Rapid publish/subscribe */
static void test_spot_rapid_pubsub()
{
    slk_ctx_t *ctx = test_context_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    int rc = slk_spot_topic_create(spot, "rapid");
    TEST_SUCCESS(rc);

    rc = slk_spot_subscribe(spot, "rapid");
    TEST_SUCCESS(rc);

    /* Rapidly publish 100 messages */
    const int count = 100;
    for (int i = 0; i < count; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "msg%d", i);
        rc = slk_spot_publish(spot, "rapid", msg, strlen(msg));
        TEST_SUCCESS(rc);
    }

    test_sleep_ms(200);

    /* Receive all messages */
    int received = 0;
    for (int i = 0; i < count; i++) {
        char topic[64], data[256];
        size_t topic_len, data_len;

        rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                          data, sizeof(data), &data_len, 100);
        if (rc == 0) {
            received++;
        }
    }

    /* Should receive all messages */
    TEST_ASSERT_EQ(received, count);

    slk_spot_destroy(&spot);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink SPOT Local Tests ===\n\n");

    RUN_TEST(test_spot_multi_topic);
    RUN_TEST(test_spot_multi_subscriber);
    RUN_TEST(test_spot_pattern_matching);
    RUN_TEST(test_spot_selective_unsubscribe);
    RUN_TEST(test_spot_large_message);
    RUN_TEST(test_spot_rapid_pubsub);

    printf("\n=== All SPOT Local Tests Passed ===\n");
    return 0;
}

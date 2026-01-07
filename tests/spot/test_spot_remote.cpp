/* ServerLink SPOT Remote PUB/SUB Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/* Test: Remote publish via TCP */
static void test_spot_remote_tcp()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create publisher and subscriber */
    slk_spot_t *pub = slk_spot_new(ctx);
    slk_spot_t *sub = slk_spot_new(ctx);

    /* Publisher creates topic and binds */
    int rc = slk_spot_topic_create(pub, "remote:tcp");
    TEST_SUCCESS(rc);

    const char *endpoint = test_endpoint_tcp();
    rc = slk_spot_bind(pub, endpoint);
    TEST_SUCCESS(rc);

    test_sleep_ms(SETTLE_TIME);

    /* Subscriber routes topic to publisher and subscribes */
    rc = slk_spot_topic_route(sub, "remote:tcp", endpoint);
    TEST_SUCCESS(rc);

    rc = slk_spot_subscribe(sub, "remote:tcp");
    TEST_SUCCESS(rc);

    test_sleep_ms(SETTLE_TIME);

    /* Publish message */
    const char *msg = "remote message";
    rc = slk_spot_publish(pub, "remote:tcp", msg, strlen(msg));
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Set receive timeout */
    int timeout_ms = 500;
    rc = slk_spot_setsockopt(sub, SLK_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    TEST_SUCCESS(rc);

    /* Receive message */
    char topic[64], data[256];
    size_t topic_len, data_len;
    rc = slk_spot_recv(sub, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 0);
    TEST_SUCCESS(rc);

    topic[topic_len] = '\0';
    data[data_len] = '\0';

    TEST_ASSERT_STR_EQ(topic, "remote:tcp");
    TEST_ASSERT_STR_EQ(data, msg);

    slk_spot_destroy(&pub);
    slk_spot_destroy(&sub);
    test_context_destroy(ctx);
}

/* Test: Remote publish via inproc */
static void test_spot_remote_inproc()
{
    slk_ctx_t *ctx = test_context_new();

    slk_spot_t *pub = slk_spot_new(ctx);
    slk_spot_t *sub = slk_spot_new(ctx);

    /* Publisher creates topic and binds */
    int rc = slk_spot_topic_create(pub, "remote:inproc");
    TEST_SUCCESS(rc);

    const char *endpoint = "inproc://test-remote";
    rc = slk_spot_bind(pub, endpoint);
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Subscriber routes topic to publisher and subscribes */
    rc = slk_spot_topic_route(sub, "remote:inproc", endpoint);
    TEST_SUCCESS(rc);

    rc = slk_spot_subscribe(sub, "remote:inproc");
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Publish message */
    const char *msg = "inproc message";
    rc = slk_spot_publish(pub, "remote:inproc", msg, strlen(msg));
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Set receive timeout */
    int timeout_ms = 500;
    rc = slk_spot_setsockopt(sub, SLK_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    TEST_SUCCESS(rc);

    /* Receive message */
    char topic[64], data[256];
    size_t topic_len, data_len;
    rc = slk_spot_recv(sub, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 0);
    TEST_SUCCESS(rc);

    topic[topic_len] = '\0';
    data[data_len] = '\0';

    TEST_ASSERT_STR_EQ(topic, "remote:inproc");
    TEST_ASSERT_STR_EQ(data, msg);

    slk_spot_destroy(&pub);
    slk_spot_destroy(&sub);
    test_context_destroy(ctx);
}

/* Test: Bidirectional remote communication */
static void test_spot_bidirectional_remote()
{
    slk_ctx_t *ctx = test_context_new();

    slk_spot_t *node1 = slk_spot_new(ctx);
    slk_spot_t *node2 = slk_spot_new(ctx);

    const char *endpoint1 = test_endpoint_tcp();
    const char *endpoint2 = test_endpoint_tcp();

    /* Both nodes create topics and bind */
    int rc;
    rc = slk_spot_topic_create(node1, "topic1");
    TEST_SUCCESS(rc);
    rc = slk_spot_bind(node1, endpoint1);
    TEST_SUCCESS(rc);

    rc = slk_spot_topic_create(node2, "topic2");
    TEST_SUCCESS(rc);
    rc = slk_spot_bind(node2, endpoint2);
    TEST_SUCCESS(rc);

    test_sleep_ms(SETTLE_TIME);

    /* Both nodes route to each other's topics */
    rc = slk_spot_topic_route(node1, "topic2", endpoint2);
    TEST_SUCCESS(rc);
    rc = slk_spot_topic_route(node2, "topic1", endpoint1);
    TEST_SUCCESS(rc);

    /* Cross-subscribe */
    rc = slk_spot_subscribe(node1, "topic2");
    TEST_SUCCESS(rc);
    rc = slk_spot_subscribe(node2, "topic1");
    TEST_SUCCESS(rc);

    test_sleep_ms(SETTLE_TIME);

    /* Node1 publishes */
    rc = slk_spot_publish(node1, "topic1", "from_node1", 10);
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Set receive timeout for node2 */
    int timeout_ms = 500;
    rc = slk_spot_setsockopt(node2, SLK_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    TEST_SUCCESS(rc);

    /* Node2 receives */
    char topic[64], data[256];
    size_t topic_len, data_len;
    rc = slk_spot_recv(node2, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 0);
    TEST_SUCCESS(rc);

    topic[topic_len] = '\0';
    data[data_len] = '\0';
    TEST_ASSERT_STR_EQ(topic, "topic1");
    TEST_ASSERT_STR_EQ(data, "from_node1");

    /* Node2 publishes */
    rc = slk_spot_publish(node2, "topic2", "from_node2", 10);
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Set receive timeout for node1 */
    rc = slk_spot_setsockopt(node1, SLK_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    TEST_SUCCESS(rc);

    /* Node1 receives */
    rc = slk_spot_recv(node1, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 0);
    TEST_SUCCESS(rc);

    topic[topic_len] = '\0';
    data[data_len] = '\0';
    TEST_ASSERT_STR_EQ(topic, "topic2");
    TEST_ASSERT_STR_EQ(data, "from_node2");

    slk_spot_destroy(&node1);
    slk_spot_destroy(&node2);
    test_context_destroy(ctx);
}

/* Test: Reconnection after disconnect */
static void test_spot_reconnect()
{
    slk_ctx_t *ctx = test_context_new();

    slk_spot_t *pub = slk_spot_new(ctx);
    slk_spot_t *sub = slk_spot_new(ctx);

    const char *endpoint = test_endpoint_tcp();

    /* Initial connection */
    int rc = slk_spot_topic_create(pub, "reconnect");
    TEST_SUCCESS(rc);

    rc = slk_spot_bind(pub, endpoint);
    TEST_SUCCESS(rc);

    test_sleep_ms(SETTLE_TIME);

    rc = slk_spot_topic_route(sub, "reconnect", endpoint);
    TEST_SUCCESS(rc);

    rc = slk_spot_subscribe(sub, "reconnect");
    TEST_SUCCESS(rc);

    test_sleep_ms(SETTLE_TIME);

    /* Send first message */
    rc = slk_spot_publish(pub, "reconnect", "msg1", 4);
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Set receive timeout */
    int timeout_ms = 500;
    rc = slk_spot_setsockopt(sub, SLK_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    TEST_SUCCESS(rc);

    char topic[64], data[256];
    size_t topic_len, data_len;
    rc = slk_spot_recv(sub, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 0);
    TEST_SUCCESS(rc);

    /* Disconnect - topic still registered but reconnect */
    rc = slk_spot_unsubscribe(sub, "reconnect");
    TEST_SUCCESS(rc);

    test_sleep_ms(SETTLE_TIME);

    /* Reconnect - re-subscribe */
    rc = slk_spot_subscribe(sub, "reconnect");
    TEST_SUCCESS(rc);

    test_sleep_ms(SETTLE_TIME);

    /* Send second message */
    rc = slk_spot_publish(pub, "reconnect", "msg2", 4);
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Should receive second message */
    rc = slk_spot_recv(sub, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 0);
    TEST_SUCCESS(rc);

    topic[topic_len] = '\0';
    data[data_len] = '\0';
    TEST_ASSERT_STR_EQ(topic, "reconnect");
    TEST_ASSERT_STR_EQ(data, "msg2");

    slk_spot_destroy(&pub);
    slk_spot_destroy(&sub);
    test_context_destroy(ctx);
}

/* Test: Multiple remote subscribers */
static void test_spot_multiple_remote_subscribers()
{
    slk_ctx_t *ctx = test_context_new();

    slk_spot_t *pub = slk_spot_new(ctx);
    slk_spot_t *sub1 = slk_spot_new(ctx);
    slk_spot_t *sub2 = slk_spot_new(ctx);
    slk_spot_t *sub3 = slk_spot_new(ctx);

    const char *endpoint = test_endpoint_tcp();

    /* Publisher setup */
    int rc = slk_spot_topic_create(pub, "broadcast");
    TEST_SUCCESS(rc);

    rc = slk_spot_bind(pub, endpoint);
    TEST_SUCCESS(rc);

    test_sleep_ms(SETTLE_TIME);

    /* All subscribers route to publisher and subscribe */
    rc = slk_spot_topic_route(sub1, "broadcast", endpoint);
    TEST_SUCCESS(rc);
    rc = slk_spot_subscribe(sub1, "broadcast");
    TEST_SUCCESS(rc);

    rc = slk_spot_topic_route(sub2, "broadcast", endpoint);
    TEST_SUCCESS(rc);
    rc = slk_spot_subscribe(sub2, "broadcast");
    TEST_SUCCESS(rc);

    rc = slk_spot_topic_route(sub3, "broadcast", endpoint);
    TEST_SUCCESS(rc);
    rc = slk_spot_subscribe(sub3, "broadcast");
    TEST_SUCCESS(rc);

    /* Give extra time for multiple remote connections to settle in ASIO */
    test_sleep_ms(SETTLE_TIME * 2);

    /* Publish message */
    const char *msg = "broadcast to all";
    rc = slk_spot_publish(pub, "broadcast", msg, strlen(msg));
    TEST_SUCCESS(rc);

    test_sleep_ms(100);

    /* Set receive timeout for all subscribers */
    int timeout_ms = 500;
    rc = slk_spot_setsockopt(sub1, SLK_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    TEST_SUCCESS(rc);
    rc = slk_spot_setsockopt(sub2, SLK_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    TEST_SUCCESS(rc);
    rc = slk_spot_setsockopt(sub3, SLK_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
    TEST_SUCCESS(rc);

    /* All subscribers should receive */
    char topic[64], data[256];
    size_t topic_len, data_len;

    rc = slk_spot_recv(sub1, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 0);
    TEST_SUCCESS(rc);
    data[data_len] = '\0';
    TEST_ASSERT_STR_EQ(data, msg);

    rc = slk_spot_recv(sub2, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 0);
    TEST_SUCCESS(rc);
    data[data_len] = '\0';
    TEST_ASSERT_STR_EQ(data, msg);

    rc = slk_spot_recv(sub3, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 0);
    TEST_SUCCESS(rc);
    data[data_len] = '\0';
    TEST_ASSERT_STR_EQ(data, msg);

    slk_spot_destroy(&pub);
    slk_spot_destroy(&sub1);
    slk_spot_destroy(&sub2);
    slk_spot_destroy(&sub3);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink SPOT Remote Tests ===\n\n");

    RUN_TEST(test_spot_remote_tcp);
    RUN_TEST(test_spot_remote_inproc);
    RUN_TEST(test_spot_bidirectional_remote);
    RUN_TEST(test_spot_reconnect);
    RUN_TEST(test_spot_multiple_remote_subscribers);

    printf("\n=== All SPOT Remote Tests Passed ===\n");
    return 0;
}

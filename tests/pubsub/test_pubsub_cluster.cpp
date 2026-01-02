/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Cluster Pub/Sub Tests */

#include "../testutil.hpp"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_NE(a, b) assert((a) != (b))
#define ASSERT_TRUE(x) assert(x)
#define ASSERT_FALSE(x) assert(!(x))

// Helper: Create a simple XPUB server for testing
slk_socket_t* create_test_server(slk_ctx_t *ctx, const char *endpoint)
{
    slk_socket_t *server = slk_socket(ctx, SLK_XPUB);
    assert(server);

    int rc = slk_bind(server, endpoint);
    assert(rc >= 0);

    return server;
}

// Test 1: Basic cluster creation and destruction
void test_cluster_create_destroy()
{
    printf("Test: Cluster create/destroy... ");

    slk_ctx_t *ctx = slk_ctx_new();
    ASSERT_NE(ctx, nullptr);

    slk_pubsub_cluster_t *cluster = slk_pubsub_cluster_new(ctx);
    ASSERT_NE(cluster, nullptr);

    slk_pubsub_cluster_destroy(&cluster);
    ASSERT_EQ(cluster, nullptr);

    slk_ctx_destroy(ctx);

    printf("PASSED\n");
}

// Test 2: Add and remove nodes
void test_add_remove_nodes()
{
    printf("Test: Add/remove nodes... ");

    slk_ctx_t *ctx = slk_ctx_new();
    ASSERT_NE(ctx, nullptr);

    // Create test servers with dynamic ports
    const char *endpoint1 = test_endpoint_tcp();
    const char *endpoint2 = test_endpoint_tcp();
    slk_socket_t *server1 = create_test_server(ctx, endpoint1);
    slk_socket_t *server2 = create_test_server(ctx, endpoint2);

    slk_pubsub_cluster_t *cluster = slk_pubsub_cluster_new(ctx);
    ASSERT_NE(cluster, nullptr);

    // Add nodes
    int rc = slk_pubsub_cluster_add_node(cluster, endpoint1);
    ASSERT_EQ(rc, 0);

    rc = slk_pubsub_cluster_add_node(cluster, endpoint2);
    ASSERT_EQ(rc, 0);

    // Check node list
    char **nodes = NULL;
    size_t count = 0;
    rc = slk_pubsub_cluster_nodes(cluster, &nodes, &count);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(count, 2);

    slk_pubsub_cluster_nodes_free(nodes, count);

    // Remove a node
    rc = slk_pubsub_cluster_remove_node(cluster, endpoint1);
    ASSERT_EQ(rc, 0);

    rc = slk_pubsub_cluster_nodes(cluster, &nodes, &count);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(count, 1);

    slk_pubsub_cluster_nodes_free(nodes, count);

    // Try to remove non-existent node
    rc = slk_pubsub_cluster_remove_node(cluster, "tcp://127.0.0.1:15099");
    ASSERT_EQ(rc, -1);

    slk_pubsub_cluster_destroy(&cluster);
    slk_close(server1);
    slk_close(server2);
    slk_ctx_destroy(ctx);

    printf("PASSED\n");
}

// Test 3: Subscribe and publish
void test_subscribe_publish()
{
    printf("Test: Subscribe/publish... ");

    slk_ctx_t *ctx = slk_ctx_new();
    ASSERT_NE(ctx, nullptr);

    // Create test server with dynamic port
    const char *endpoint = test_endpoint_tcp();
    slk_socket_t *server = create_test_server(ctx, endpoint);

    slk_pubsub_cluster_t *cluster = slk_pubsub_cluster_new(ctx);
    ASSERT_NE(cluster, nullptr);

    // Add node
    int rc = slk_pubsub_cluster_add_node(cluster, endpoint);
    ASSERT_EQ(rc, 0);

    // Subscribe to a channel
    rc = slk_pubsub_cluster_subscribe(cluster, "test.channel");
    ASSERT_EQ(rc, 0);

    // Give time for subscription to propagate
    slk_sleep(50); // 50ms

    // Publish a message
    const char *msg = "Hello, cluster!";
    rc = slk_pubsub_cluster_publish(cluster, "test.channel", msg, strlen(msg));
    ASSERT_EQ(rc, 0);

    slk_pubsub_cluster_destroy(&cluster);
    slk_close(server);
    slk_ctx_destroy(ctx);

    printf("PASSED\n");
}

// Test 4: Pattern subscription
void test_pattern_subscription()
{
    printf("Test: Pattern subscription... ");

    slk_ctx_t *ctx = slk_ctx_new();
    ASSERT_NE(ctx, nullptr);

    // Create test servers with dynamic ports
    const char *endpoint1 = test_endpoint_tcp();
    const char *endpoint2 = test_endpoint_tcp();
    slk_socket_t *server1 = create_test_server(ctx, endpoint1);
    slk_socket_t *server2 = create_test_server(ctx, endpoint2);

    slk_pubsub_cluster_t *cluster = slk_pubsub_cluster_new(ctx);
    ASSERT_NE(cluster, nullptr);

    // Add nodes
    int rc = slk_pubsub_cluster_add_node(cluster, endpoint1);
    ASSERT_EQ(rc, 0);

    rc = slk_pubsub_cluster_add_node(cluster, endpoint2);
    ASSERT_EQ(rc, 0);

    // Pattern subscribe (should propagate to all nodes)
    rc = slk_pubsub_cluster_psubscribe(cluster, "news.*");
    ASSERT_EQ(rc, 0);

    // Give time for subscription to propagate
    slk_sleep(50); // 50ms

    // Publish to matching channel
    const char *msg = "Breaking news!";
    rc = slk_pubsub_cluster_publish(cluster, "news.sports", msg, strlen(msg));
    ASSERT_EQ(rc, 0);

    // Unsubscribe from pattern
    rc = slk_pubsub_cluster_punsubscribe(cluster, "news.*");
    ASSERT_EQ(rc, 0);

    slk_pubsub_cluster_destroy(&cluster);
    slk_close(server1);
    slk_close(server2);
    slk_ctx_destroy(ctx);

    printf("PASSED\n");
}

// Test 5: Hash tag support
void test_hash_tag()
{
    printf("Test: Hash tag support... ");

    slk_ctx_t *ctx = slk_ctx_new();
    ASSERT_NE(ctx, nullptr);

    // Create test servers with dynamic ports
    const char *endpoint1 = test_endpoint_tcp();
    const char *endpoint2 = test_endpoint_tcp();
    slk_socket_t *server1 = create_test_server(ctx, endpoint1);
    slk_socket_t *server2 = create_test_server(ctx, endpoint2);

    slk_pubsub_cluster_t *cluster = slk_pubsub_cluster_new(ctx);
    ASSERT_NE(cluster, nullptr);

    // Add nodes
    int rc = slk_pubsub_cluster_add_node(cluster, endpoint1);
    ASSERT_EQ(rc, 0);

    rc = slk_pubsub_cluster_add_node(cluster, endpoint2);
    ASSERT_EQ(rc, 0);

    // Subscribe with hash tag (channels with same tag go to same node)
    rc = slk_pubsub_cluster_subscribe(cluster, "{user:123}messages");
    ASSERT_EQ(rc, 0);

    rc = slk_pubsub_cluster_subscribe(cluster, "{user:123}notifications");
    ASSERT_EQ(rc, 0);

    // Give time for subscription to propagate
    slk_sleep(50); // 50ms

    // Publish to both channels (should route to same node)
    const char *msg1 = "New message";
    rc = slk_pubsub_cluster_publish(cluster, "{user:123}messages", msg1, strlen(msg1));
    ASSERT_EQ(rc, 0);

    const char *msg2 = "New notification";
    rc = slk_pubsub_cluster_publish(cluster, "{user:123}notifications", msg2, strlen(msg2));
    ASSERT_EQ(rc, 0);

    slk_pubsub_cluster_destroy(&cluster);
    slk_close(server1);
    slk_close(server2);
    slk_ctx_destroy(ctx);

    printf("PASSED\n");
}

// Test 6: Multiple subscriptions
void test_multiple_subscriptions()
{
    printf("Test: Multiple subscriptions... ");

    slk_ctx_t *ctx = slk_ctx_new();
    ASSERT_NE(ctx, nullptr);

    const char *endpoint = test_endpoint_tcp();
    slk_socket_t *server = create_test_server(ctx, endpoint);

    slk_pubsub_cluster_t *cluster = slk_pubsub_cluster_new(ctx);
    ASSERT_NE(cluster, nullptr);

    int rc = slk_pubsub_cluster_add_node(cluster, endpoint);
    ASSERT_EQ(rc, 0);

    // Subscribe to multiple channels
    rc = slk_pubsub_cluster_subscribe(cluster, "channel1");
    ASSERT_EQ(rc, 0);

    rc = slk_pubsub_cluster_subscribe(cluster, "channel2");
    ASSERT_EQ(rc, 0);

    rc = slk_pubsub_cluster_subscribe(cluster, "channel3");
    ASSERT_EQ(rc, 0);

    // Unsubscribe from one
    rc = slk_pubsub_cluster_unsubscribe(cluster, "channel2");
    ASSERT_EQ(rc, 0);

    slk_pubsub_cluster_destroy(&cluster);
    slk_close(server);
    slk_ctx_destroy(ctx);

    printf("PASSED\n");
}

int main()
{
    printf("=== Cluster Pub/Sub Tests ===\n\n");

    test_cluster_create_destroy();
    test_add_remove_nodes();
    test_subscribe_publish();
    test_pattern_subscription();
    test_hash_tag();
    test_multiple_subscriptions();

    printf("\n=== All Cluster Pub/Sub Tests Passed ===\n");
    return 0;
}

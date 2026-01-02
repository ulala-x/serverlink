/*
 * Cluster Pub/Sub Example
 *
 * Demonstrates the slk_pubsub_cluster_* API for distributed pub/sub across
 * multiple ServerLink instances. This example shows:
 *   - Creating a cluster and adding nodes
 *   - Publishing to cluster channels with automatic routing
 *   - Subscribing to channels and receiving messages
 *   - Pattern subscriptions across cluster nodes
 *   - Node management (add/remove)
 *
 * NOTE: This example simulates a cluster using multiple threads.
 * In a real deployment, each node would be a separate process/server.
 *
 * Build:
 *   cmake --build build
 *   ./build/examples/pubsub/cluster_example
 */

#include <stdio.h>
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include <serverlink/serverlink.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#define CHECK(expr, msg) do { \
    if ((expr) < 0) { \
        perror(msg); \
        return 1; \
    } \
} while(0)

#define CHECK_PTR(expr, msg) do { \
    if (!(expr)) { \
        perror(msg); \
        return NULL; \
    } \
} while(0)

// Simulated node server
typedef struct {
    int node_id;
    slk_ctx_t *ctx;
    slk_socket_t *pub;
    slk_socket_t *sub;
    const char *endpoint;
    volatile int running;
} node_server_t;

// Node server thread
void *node_server_thread(void *arg) {
    node_server_t *node = (node_server_t *)arg;

    printf("[Node %d] Starting on %s\n", node->node_id, node->endpoint);

    node->ctx = slk_ctx_new();
    CHECK_PTR(node->ctx, "slk_ctx_new");

    // For this example, we'll use a simple PUB socket as the node endpoint
    node->pub = slk_socket(node->ctx, SLK_PUB);
    CHECK_PTR(node->pub, "slk_socket(PUB)");

    if (slk_bind(node->pub, node->endpoint) < 0) {
        perror("slk_bind");
        slk_ctx_destroy(node->ctx);
        return NULL;
    }

    printf("[Node %d] Ready and listening\n", node->node_id);

    // Keep node running
    while (node->running) {
        usleep(100000);  // 100ms
    }

    printf("[Node %d] Shutting down\n", node->node_id);

    slk_close(node->pub);
    slk_ctx_destroy(node->ctx);
    return NULL;
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("=== ServerLink Cluster Pub/Sub Example ===\n\n");

    // Initialize context
    slk_ctx_t *ctx = slk_ctx_new();
    if (!ctx) {
        perror("slk_ctx_new");
        return 1;
    }

    // Create cluster
    printf("Creating cluster...\n");
    slk_pubsub_cluster_t *cluster = slk_pubsub_cluster_new(ctx);
    if (!cluster) {
        perror("slk_pubsub_cluster_new");
        slk_ctx_destroy(ctx);
        return 1;
    }

    // Simulate cluster nodes
    printf("\n=== Simulating Cluster Nodes ===\n\n");

    node_server_t nodes[3] = {
        {0, NULL, NULL, NULL, "tcp://127.0.0.1:6001", 1},
        {1, NULL, NULL, NULL, "tcp://127.0.0.1:6002", 1},
        {2, NULL, NULL, NULL, "tcp://127.0.0.1:6003", 1}
    };

    pthread_t node_threads[3];

    // Start node servers
    for (int i = 0; i < 3; i++) {
        if (pthread_create(&node_threads[i], NULL, node_server_thread, &nodes[i]) != 0) {
            perror("pthread_create");
            slk_pubsub_cluster_destroy(&cluster);
            slk_ctx_destroy(ctx);
            return 1;
        }
    }

    // Allow nodes to start
    sleep(1);

    // Add nodes to cluster
    printf("\n=== Adding Nodes to Cluster ===\n\n");
    for (int i = 0; i < 3; i++) {
        printf("Adding node: %s\n", nodes[i].endpoint);
        CHECK(slk_pubsub_cluster_add_node(cluster, nodes[i].endpoint),
              "slk_pubsub_cluster_add_node");
    }

    // List cluster nodes
    printf("\n=== Cluster Nodes ===\n\n");
    char **node_list;
    size_t node_count;

    if (slk_pubsub_cluster_nodes(cluster, &node_list, &node_count) == 0) {
        printf("Cluster has %zu nodes:\n", node_count);
        for (size_t i = 0; i < node_count; i++) {
            printf("  [%zu] %s\n", i, node_list[i]);
        }
        // Note: In real implementation, would need to free node_list
    }

    printf("\n=== Subscribing to Channels ===\n\n");

    // Subscribe to specific channels
    printf("Subscribing to 'global.events'\n");
    CHECK(slk_pubsub_cluster_subscribe(cluster, "global.events"),
          "slk_pubsub_cluster_subscribe");

    printf("Subscribing to 'news.sports'\n");
    CHECK(slk_pubsub_cluster_subscribe(cluster, "news.sports"),
          "slk_pubsub_cluster_subscribe");

    // Subscribe using pattern
    printf("Subscribing to pattern 'alerts.*'\n");
    CHECK(slk_pubsub_cluster_psubscribe(cluster, "alerts.*"),
          "slk_pubsub_cluster_psubscribe");

    // Allow subscriptions to propagate
    usleep(500000);  // 500ms

    printf("\n=== Publishing Messages ===\n\n");

    // Publish to various channels
    const char *channels[] = {
        "global.events",
        "news.sports",
        "alerts.critical",
        "alerts.warning",
        "alerts.info"
    };

    for (int i = 0; i < 5; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Message %d on channel %s", i, channels[i]);

        printf("Publishing: [%s] %s\n", channels[i], msg);

        int nodes_reached = slk_pubsub_cluster_publish(cluster, channels[i],
                                                       msg, strlen(msg));
        if (nodes_reached >= 0) {
            printf("  -> Reached %d nodes\n", nodes_reached);
        } else {
            perror("  -> slk_pubsub_cluster_publish");
        }
    }

    printf("\n=== Receiving Messages ===\n\n");

    // Receive messages
    int received_count = 0;
    for (int i = 0; i < 10; i++) {
        char channel[256];
        char data[1024];
        size_t channel_len = sizeof(channel);
        size_t data_len = sizeof(data);

        int rc = slk_pubsub_cluster_recv(cluster,
                                          channel, &channel_len,
                                          data, &data_len,
                                          0);  // Blocking

        if (rc == 0) {
            printf("[%d] Channel: %.*s\n", received_count + 1,
                   (int)channel_len, channel);
            printf("    Message: %.*s\n", (int)data_len, data);
            received_count++;
        } else {
            if (slk_errno() == SLK_EAGAIN) {
                printf("Receive timeout - no more messages\n");
                break;
            }
            perror("slk_pubsub_cluster_recv");
            break;
        }
    }

    printf("\nReceived %d messages\n", received_count);

    printf("\n=== Hash Tag Routing ===\n\n");

    // Demonstrate hash tags for routing
    printf("Publishing messages with hash tags:\n");

    const char *user_channels[] = {
        "{user:123}messages",
        "{user:123}notifications",
        "{user:456}messages",
        "{user:456}notifications"
    };

    for (int i = 0; i < 4; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Message for %s", user_channels[i]);

        printf("  %s: %s\n", user_channels[i], msg);

        int nodes_reached = slk_pubsub_cluster_publish(cluster, user_channels[i],
                                                       msg, strlen(msg));
        printf("    (Routed to %d nodes)\n", nodes_reached);
    }

    printf("\nNote: Channels with same hash tag (e.g., {user:123}) are routed to the same node\n");

    printf("\n=== Node Management ===\n\n");

    // Remove a node
    printf("Removing node: %s\n", nodes[2].endpoint);
    CHECK(slk_pubsub_cluster_remove_node(cluster, nodes[2].endpoint),
          "slk_pubsub_cluster_remove_node");

    // List nodes again
    if (slk_pubsub_cluster_nodes(cluster, &node_list, &node_count) == 0) {
        printf("Cluster now has %zu nodes:\n", node_count);
        for (size_t i = 0; i < node_count; i++) {
            printf("  [%zu] %s\n", i, node_list[i]);
        }
    }

    // Publish after node removal
    printf("\nPublishing after node removal:\n");
    int nodes_reached = slk_pubsub_cluster_publish(cluster, "global.events",
                                                   "Post-removal message", 20);
    printf("Message reached %d nodes (expected 2)\n", nodes_reached);

    printf("\n=== Cleanup ===\n");

    // Stop cluster
    slk_pubsub_cluster_destroy(&cluster);

    // Stop node servers
    for (int i = 0; i < 3; i++) {
        nodes[i].running = 0;
    }

    // Wait for node threads
    for (int i = 0; i < 3; i++) {
        pthread_join(node_threads[i], NULL);
    }

    slk_ctx_destroy(ctx);

    printf("Done.\n");
    return 0;
}

/* ServerLink SPOT PUB/SUB - Cluster Subscriber Example
 *
 * This example demonstrates a SPOT node acting as a subscriber in a cluster:
 * - Connecting to remote publishers
 * - Discovering remote topics via cluster sync
 * - Subscribing to remote topics
 * - Receiving messages over TCP
 *
 * Usage:
 *   1. Start publisher first: ./spot_cluster_publisher
 *   2. Start this subscriber: ./spot_cluster_subscriber
 *   3. Subscriber discovers and consumes remote topics
 *
 * This demonstrates SPOT's location transparency - the subscriber uses
 * the same API for local and remote topics.
 */

#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

static void print_timestamp(void)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
}

int main(void)
{
    printf("=== SPOT Cluster Subscriber ===\n\n");

    /* Initialize */
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx != NULL);

    slk_spot_t *spot = slk_spot_new(ctx);
    assert(spot != NULL);

    /* Configure HWM */
    slk_spot_set_hwm(spot, 1000, 1000);

    /* Step 1: Add cluster node
     *
     * Connect to the remote publisher's ROUTER socket.
     * This establishes the cluster relationship.
     */
    printf("Connecting to cluster...\n");
    const char *publisher_endpoint = "tcp://localhost:5555";

    if (slk_spot_cluster_add(spot, publisher_endpoint) < 0) {
        fprintf(stderr, "Failed to add cluster node %s: %s\n",
                publisher_endpoint, slk_strerror(slk_errno()));
        fprintf(stderr, "\nMake sure the publisher is running first!\n");
        goto cleanup;
    }

    print_timestamp();
    printf("✓ Connected to publisher at %s\n\n", publisher_endpoint);

    /* Step 2: Synchronize topics with cluster
     *
     * This sends a QUERY command to the cluster node and receives
     * the list of available topics. Remote topics are automatically
     * registered in the local topic registry.
     */
    printf("Synchronizing topics...\n");

    if (slk_spot_cluster_sync(spot, 5000) < 0) {
        fprintf(stderr, "Cluster sync failed: %s\n", slk_strerror(slk_errno()));
        goto cleanup;
    }

    print_timestamp();
    printf("✓ Cluster synchronization complete\n\n");

    /* Step 3: List discovered topics */
    char **topics;
    size_t topic_count;

    if (slk_spot_list_topics(spot, &topics, &topic_count) == 0) {
        printf("Discovered topics (%zu):\n", topic_count);
        for (size_t i = 0; i < topic_count; i++) {
            int is_local = slk_spot_topic_is_local(spot, topics[i]);
            printf("  %zu. %s %s\n", i + 1, topics[i],
                   is_local ? "(local)" : "(remote)");
        }
        slk_spot_list_topics_free(topics, topic_count);
        printf("\n");
    }

    /* Step 4: Subscribe to interesting topics
     *
     * Subscribe to specific remote topics we want to monitor.
     * The API is identical whether topics are local or remote!
     */
    printf("Subscribing to topics...\n");

    const char *subscribe_topics[] = {
        "stock:prices:AAPL",
        "stock:prices:GOOGL",
        "crypto:prices:BTC",
        NULL
    };

    for (int i = 0; subscribe_topics[i] != NULL; i++) {
        /* Check if topic exists before subscribing */
        int exists = slk_spot_topic_exists(spot, subscribe_topics[i]);
        if (exists != 1) {
            printf("  ⚠ Topic '%s' not found (skipping)\n", subscribe_topics[i]);
            continue;
        }

        if (slk_spot_subscribe(spot, subscribe_topics[i]) < 0) {
            fprintf(stderr, "  ✗ Failed to subscribe to '%s': %s\n",
                    subscribe_topics[i], slk_strerror(slk_errno()));
        } else {
            int is_local = slk_spot_topic_is_local(spot, subscribe_topics[i]);
            printf("  ✓ Subscribed to %s %s\n", subscribe_topics[i],
                   is_local ? "(local)" : "(remote)");
        }
    }

    printf("\n");
    print_timestamp();
    printf("Waiting for messages (will receive up to 30)...\n\n");

    /* Step 5: Receive messages
     *
     * Messages published by the remote node are delivered over TCP.
     * The receive API is the same for local and remote messages.
     */
    char recv_topic[256];
    char recv_data[1024];
    size_t topic_len, data_len;
    int received_count = 0;
    int max_messages = 30;

    while (received_count < max_messages) {
        int rc = slk_spot_recv(spot, recv_topic, sizeof(recv_topic), &topic_len,
                              recv_data, sizeof(recv_data), &data_len,
                              SLK_DONTWAIT);

        if (rc == 0) {
            recv_topic[topic_len] = '\0';
            recv_data[data_len] = '\0';

            print_timestamp();
            printf("[%d] %s\n", received_count + 1, recv_topic);
            printf("     %s\n", recv_data);

            received_count++;
        } else if (slk_errno() == SLK_EAGAIN) {
            /* No message available, wait a bit */
            slk_sleep(100);
        } else {
            fprintf(stderr, "Receive error: %s\n", slk_strerror(slk_errno()));
            break;
        }
    }

    printf("\n");
    print_timestamp();
    printf("Received %d messages from remote publisher\n\n", received_count);

    /* Step 6: Unsubscribe demonstration */
    printf("Unsubscribing from stock:prices:AAPL...\n");

    if (slk_spot_unsubscribe(spot, "stock:prices:AAPL") == 0) {
        print_timestamp();
        printf("✓ Unsubscribed successfully\n");
    } else {
        fprintf(stderr, "Unsubscribe failed: %s\n", slk_strerror(slk_errno()));
    }

    printf("\n");

    /* Step 7: Remove cluster node */
    printf("Disconnecting from cluster...\n");

    if (slk_spot_cluster_remove(spot, publisher_endpoint) == 0) {
        print_timestamp();
        printf("✓ Disconnected from %s\n", publisher_endpoint);
    } else {
        fprintf(stderr, "Failed to remove cluster node: %s\n",
                slk_strerror(slk_errno()));
    }

    printf("\n=== Subscriber shutting down ===\n");

cleanup:
    slk_spot_destroy(&spot);
    slk_ctx_destroy(ctx);

    return 0;
}

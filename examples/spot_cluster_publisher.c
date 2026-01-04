/* ServerLink SPOT PUB/SUB - Cluster Publisher Example
 *
 * This example demonstrates a SPOT node acting as a publisher in a cluster:
 * - Hosting local topics
 * - Accepting connections from remote subscribers
 * - Publishing messages that can be consumed by remote nodes
 * - Server mode with ROUTER socket
 *
 * Usage:
 *   1. Start this publisher: ./spot_cluster_publisher
 *   2. Start subscriber: ./spot_cluster_subscriber
 *   3. Publisher sends periodic messages
 *   4. Subscriber receives them over TCP
 */

#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>  // For rand()
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
    printf("=== SPOT Cluster Publisher ===\n\n");

    /* Initialize */
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx != NULL);

    slk_spot_t *spot = slk_spot_new(ctx);
    assert(spot != NULL);

    /* Configure HWM */
    slk_spot_set_hwm(spot, 1000, 1000);

    /* Step 1: Bind to endpoint for cluster mode
     *
     * This creates a ROUTER socket that accepts connections from other
     * SPOT nodes. Remote nodes can query available topics and subscribe.
     */
    printf("Starting server mode...\n");
    const char *bind_endpoint = "tcp://*:5555";

    if (slk_spot_bind(spot, bind_endpoint) < 0) {
        fprintf(stderr, "Failed to bind to %s: %s\n",
                bind_endpoint, slk_strerror(slk_errno()));
        goto cleanup;
    }

    print_timestamp();
    printf("✓ Server listening on %s\n\n", bind_endpoint);

    /* Step 2: Create local topics
     *
     * These topics will be advertised to remote nodes that connect
     * and query for available topics.
     */
    printf("Creating local topics...\n");

    const char *local_topics[] = {
        "stock:prices:AAPL",
        "stock:prices:GOOGL",
        "stock:prices:MSFT",
        "forex:rates:USD_EUR",
        "forex:rates:USD_GBP",
        "crypto:prices:BTC",
        NULL
    };

    for (int i = 0; local_topics[i] != NULL; i++) {
        if (slk_spot_topic_create(spot, local_topics[i]) < 0) {
            fprintf(stderr, "Failed to create topic '%s': %s\n",
                    local_topics[i], slk_strerror(slk_errno()));
            goto cleanup;
        }
        printf("  ✓ %s\n", local_topics[i]);
    }

    printf("\n");
    print_timestamp();
    printf("Publisher is ready. Waiting for subscribers...\n");
    printf("(Remote subscribers will connect to tcp://localhost:5555)\n\n");

    /* Step 3: Publish messages periodically
     *
     * Simulate real-time market data updates.
     * Remote subscribers will receive these messages over TCP.
     */
    printf("Starting message publishing (Ctrl+C to stop)...\n\n");

    int message_count = 0;
    char message_buf[256];

    /* Simple price simulation */
    double prices[] = {150.25, 2800.50, 380.75, 0.92, 0.78, 45000.0};
    const int num_prices = sizeof(prices) / sizeof(prices[0]);

    for (int round = 0; round < 10; round++) {  /* 10 rounds for demo */
        print_timestamp();
        printf("Publishing round %d...\n", round + 1);

        /* Publish to each topic */
        for (int i = 0; local_topics[i] != NULL; i++) {
            /* Simulate price change */
            prices[i] += ((double)(rand() % 200 - 100)) / 100.0;

            /* Format message */
            snprintf(message_buf, sizeof(message_buf),
                    "{\"price\":%.2f,\"volume\":%d,\"timestamp\":%lld}",
                    prices[i], 1000 + (rand() % 9000),
                    (long long)time(NULL));

            /* Publish */
            if (slk_spot_publish(spot, local_topics[i],
                                message_buf, strlen(message_buf)) == 0) {
                printf("  [%d] %s: %.2f\n", message_count + 1,
                       local_topics[i], prices[i]);
                message_count++;
            } else {
                /* Handle HWM or other errors */
                if (slk_errno() == SLK_EAGAIN) {
                    printf("  [!] HWM reached for %s, message dropped\n",
                           local_topics[i]);
                } else {
                    fprintf(stderr, "  [!] Publish error: %s\n",
                            slk_strerror(slk_errno()));
                }
            }
        }

        printf("\n");
        slk_sleep(1000);  /* 1 second interval */
    }

    print_timestamp();
    printf("Published %d messages total\n\n", message_count);

    /* Step 4: Show statistics */
    char **topics;
    size_t topic_count;

    if (slk_spot_list_topics(spot, &topics, &topic_count) == 0) {
        printf("Final topic list (%zu topics):\n", topic_count);
        for (size_t i = 0; i < topic_count; i++) {
            int is_local = slk_spot_topic_is_local(spot, topics[i]);
            printf("  %zu. %s %s\n", i + 1, topics[i],
                   is_local ? "(local)" : "(remote)");
        }
        slk_spot_list_topics_free(topics, topic_count);
    }

    printf("\n=== Publisher shutting down ===\n");

cleanup:
    slk_spot_destroy(&spot);
    slk_ctx_destroy(ctx);

    return 0;
}

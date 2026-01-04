/* ServerLink SPOT PUB/SUB - Basic Usage Example
 *
 * This example demonstrates the fundamental SPOT PUB/SUB operations:
 * - Creating local topics
 * - Publishing messages
 * - Subscribing to topics
 * - Receiving messages
 *
 * SPOT (Scalable Partitioned Ordered Topics) provides location-transparent
 * pub/sub with topic ID-based routing.
 */

#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    printf("=== ServerLink SPOT Basic Example ===\n\n");

    /* Step 1: Create context and SPOT instance */
    slk_ctx_t *ctx = slk_ctx_new();
    if (!ctx) {
        fprintf(stderr, "Failed to create context: %s\n", slk_strerror(slk_errno()));
        return 1;
    }

    slk_spot_t *spot = slk_spot_new(ctx);
    if (!spot) {
        fprintf(stderr, "Failed to create SPOT instance: %s\n", slk_strerror(slk_errno()));
        slk_ctx_destroy(ctx);
        return 1;
    }

    /* Step 2: Create local topics
     *
     * These topics are hosted on this node. When you create a topic,
     * SPOT internally creates an XPUB socket bound to an inproc endpoint.
     */
    printf("Creating local topics...\n");

    if (slk_spot_topic_create(spot, "news:weather") < 0) {
        fprintf(stderr, "Failed to create topic 'news:weather': %s\n",
                slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Created topic: news:weather\n");

    if (slk_spot_topic_create(spot, "news:sports") < 0) {
        fprintf(stderr, "Failed to create topic 'news:sports': %s\n",
                slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Created topic: news:sports\n");

    if (slk_spot_topic_create(spot, "alerts:traffic") < 0) {
        fprintf(stderr, "Failed to create topic 'alerts:traffic': %s\n",
                slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Created topic: alerts:traffic\n\n");

    /* Step 3: List all topics */
    char **topics;
    size_t topic_count;
    if (slk_spot_list_topics(spot, &topics, &topic_count) == 0) {
        printf("Registered topics (%zu):\n", topic_count);
        for (size_t i = 0; i < topic_count; i++) {
            int is_local = slk_spot_topic_is_local(spot, topics[i]);
            printf("  %zu. %s %s\n", i + 1, topics[i],
                   is_local ? "(local)" : "(remote)");
        }
        slk_spot_list_topics_free(topics, topic_count);
        printf("\n");
    }

    /* Step 4: Subscribe to topics
     *
     * Subscribing connects the internal XSUB socket to the topic's
     * endpoint and sets up the subscription filter.
     */
    printf("Subscribing to topics...\n");

    if (slk_spot_subscribe(spot, "news:weather") < 0) {
        fprintf(stderr, "Failed to subscribe to 'news:weather': %s\n",
                slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Subscribed to: news:weather\n");

    if (slk_spot_subscribe(spot, "alerts:traffic") < 0) {
        fprintf(stderr, "Failed to subscribe to 'alerts:traffic': %s\n",
                slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Subscribed to: alerts:traffic\n\n");

    /* Give inproc connections time to establish */
    slk_sleep(10);

    /* Step 5: Publish messages
     *
     * Messages are sent to the topic's XPUB socket.
     * Only subscribers to that topic will receive them.
     */
    printf("Publishing messages...\n");

    const char *weather_msg = "Sunny, 25°C";
    if (slk_spot_publish(spot, "news:weather", weather_msg, strlen(weather_msg)) < 0) {
        fprintf(stderr, "Failed to publish to 'news:weather': %s\n",
                slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Published to news:weather: %s\n", weather_msg);

    const char *sports_msg = "Team A wins 3-2";
    if (slk_spot_publish(spot, "news:sports", sports_msg, strlen(sports_msg)) < 0) {
        fprintf(stderr, "Failed to publish to 'news:sports': %s\n",
                slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Published to news:sports: %s (not subscribed)\n", sports_msg);

    const char *traffic_msg = "Highway A1 congestion";
    if (slk_spot_publish(spot, "alerts:traffic", traffic_msg, strlen(traffic_msg)) < 0) {
        fprintf(stderr, "Failed to publish to 'alerts:traffic': %s\n",
                slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Published to alerts:traffic: %s\n\n", traffic_msg);

    /* Step 6: Receive messages
     *
     * Only messages from subscribed topics will be received.
     * Expected: news:weather and alerts:traffic (2 messages)
     */
    printf("Receiving messages (expecting 2)...\n");

    char recv_topic[256];
    char recv_data[1024];
    size_t topic_len, data_len;
    int msg_count = 0;

    /* Non-blocking receive loop */
    while (msg_count < 2) {
        int rc = slk_spot_recv(spot, recv_topic, sizeof(recv_topic), &topic_len,
                              recv_data, sizeof(recv_data), &data_len,
                              SLK_DONTWAIT);

        if (rc == 0) {
            recv_topic[topic_len] = '\0';
            recv_data[data_len] = '\0';
            printf("  [%d] Topic: %s\n", msg_count + 1, recv_topic);
            printf("      Data:  %s\n", recv_data);
            msg_count++;
        } else {
            /* No more messages available yet */
            if (slk_errno() == SLK_EAGAIN) {
                slk_sleep(10);  /* Wait a bit for messages to propagate */
            } else {
                fprintf(stderr, "Receive error: %s\n", slk_strerror(slk_errno()));
                break;
            }
        }
    }

    printf("\nReceived %d messages (news:sports was filtered out)\n", msg_count);

    /* Step 7: Unsubscribe demonstration */
    printf("\nUnsubscribing from news:weather...\n");
    if (slk_spot_unsubscribe(spot, "news:weather") < 0) {
        fprintf(stderr, "Failed to unsubscribe: %s\n", slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Unsubscribed from news:weather\n");

    /* Publish again - this message should not be received */
    const char *weather_msg2 = "Cloudy, 20°C";
    slk_spot_publish(spot, "news:weather", weather_msg2, strlen(weather_msg2));
    slk_sleep(10);

    int rc = slk_spot_recv(spot, recv_topic, sizeof(recv_topic), &topic_len,
                          recv_data, sizeof(recv_data), &data_len,
                          SLK_DONTWAIT);

    if (rc < 0 && slk_errno() == SLK_EAGAIN) {
        printf("  ✓ No message received (unsubscribe successful)\n");
    }

    printf("\n=== Example completed successfully ===\n");

cleanup:
    /* Step 8: Cleanup */
    slk_spot_destroy(&spot);
    slk_ctx_destroy(ctx);

    return 0;
}

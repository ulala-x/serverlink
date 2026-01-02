/*
 * Sharded Pub/Sub Example
 *
 * Demonstrates the slk_sharded_pubsub_* API for horizontal scalability.
 * Shows how channels are distributed across multiple shards and how hash tags
 * can be used to co-locate related channels.
 *
 * Build:
 *   cmake --build build
 *   ./build/examples/pubsub/sharded_example
 */

#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include <serverlink/serverlink.h>
#include <stdio.h>
#include <stdlib.h>
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

#define SHARD_COUNT 16
#define MESSAGE_COUNT 1000

typedef struct {
    slk_sharded_pubsub_t *shard_ctx;
    int thread_id;
    int message_count;
} publisher_args_t;

typedef struct {
    slk_sharded_pubsub_t *shard_ctx;
    slk_ctx_t *ctx;
    const char *channel_pattern;
    int expected_count;
} subscriber_args_t;

// Publisher thread
void *publisher_thread(void *arg) {
    publisher_args_t *args = (publisher_args_t *)arg;

    printf("[Publisher %d] Starting\n", args->thread_id);

    for (int i = 0; i < args->message_count; i++) {
        char channel[64];
        char message[256];

        // Publish to different channels
        snprintf(channel, sizeof(channel), "events.thread%d", args->thread_id);
        snprintf(message, sizeof(message), "Message %d from thread %d", i, args->thread_id);

        int rc = slk_spublish(args->shard_ctx, channel, message, strlen(message));
        if (rc < 0) {
            perror("slk_spublish");
            break;
        }

        // Occasional status update
        if (i % 100 == 0) {
            printf("[Publisher %d] Sent %d messages\n", args->thread_id, i);
        }
    }

    printf("[Publisher %d] Completed %d messages\n", args->thread_id, args->message_count);
    return NULL;
}

// Subscriber thread
void *subscriber_thread(void *arg) {
    subscriber_args_t *args = (subscriber_args_t *)arg;

    slk_socket_t *sub = slk_socket(args->ctx, SLK_SUB);
    if (!sub) {
        perror("slk_socket(SUB)");
        return NULL;
    }

    // Subscribe to channels matching pattern
    printf("[Subscriber %s] Subscribing\n", args->channel_pattern);

    // For sharded pub/sub, we need to subscribe to specific channels
    // (pattern subscriptions are not supported in sharded mode)
    // Extract thread number from pattern
    int thread_num;
    if (sscanf(args->channel_pattern, "events.thread%d", &thread_num) == 1) {
        char channel[64];
        snprintf(channel, sizeof(channel), "events.thread%d", thread_num);

        if (slk_ssubscribe(args->shard_ctx, sub, channel) < 0) {
            perror("slk_ssubscribe");
            slk_close(sub);
            return NULL;
        }
    }

    // Receive messages with timeout using loop
    int count = 0;
    int no_msg_count = 0;
    char message[1024];

    while (no_msg_count < 50) {  // Exit after 50 consecutive empty receives
        int rc = slk_recv(sub, message, sizeof(message), SLK_DONTWAIT);
        if (rc < 0) {
            if (slk_errno() == SLK_EAGAIN) {
                usleep(100000);  // 100ms
                no_msg_count++;
                continue;
            }
            perror("slk_recv");
            break;
        }
        no_msg_count = 0;

        count++;

        if (count % 100 == 0) {
            printf("[Subscriber %s] Received %d messages\n", args->channel_pattern, count);
        }
    }

    printf("[Subscriber %s] Total received: %d (expected: %d)\n",
           args->channel_pattern, count, args->expected_count);

    slk_close(sub);
    return NULL;
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("=== ServerLink Sharded Pub/Sub Example ===\n\n");

    // Initialize context
    slk_ctx_t *ctx = slk_ctx_new();
    if (!ctx) {
        perror("slk_ctx_new");
        return 1;
    }

    // Create sharded pub/sub context with 16 shards
    printf("Creating sharded pub/sub with %d shards...\n", SHARD_COUNT);
    slk_sharded_pubsub_t *shard_ctx = slk_sharded_pubsub_new(ctx, SHARD_COUNT);
    if (!shard_ctx) {
        perror("slk_sharded_pubsub_new");
        slk_ctx_destroy(ctx);
        return 1;
    }

    // Set HWM (high water mark) for flow control
    printf("Setting HWM to 10,000 messages per shard\n");
    CHECK(slk_sharded_pubsub_set_hwm(shard_ctx, 10000),
          "slk_sharded_pubsub_set_hwm");

    printf("\n=== Basic Pub/Sub Test ===\n\n");

    // Create a simple subscriber
    slk_socket_t *sub1 = slk_socket(ctx, SLK_SUB);
    if (!sub1) {
        perror("slk_socket(SUB)");
        slk_sharded_pubsub_destroy(&shard_ctx);
        slk_ctx_destroy(ctx);
        return 1;
    }

    CHECK(slk_ssubscribe(shard_ctx, sub1, "test.channel"),
          "slk_ssubscribe");

    usleep(100000);  // 100ms for subscription to propagate

    // Publish some messages
    for (int i = 0; i < 10; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Test message %d", i);

        int subscribers = slk_spublish(shard_ctx, "test.channel", msg, strlen(msg));
        printf("Published: %s (reached %d subscribers)\n", msg, subscribers);
    }

    // Receive messages
    printf("\nReceiving messages:\n");
    char buf[256];
    for (int i = 0; i < 10; i++) {
        int rc = slk_recv(sub1, buf, sizeof(buf), 0);
        if (rc > 0) {
            buf[rc] = '\0';
            printf("  [%d] %s\n", i, buf);
        }
    }

    slk_close(sub1);

    printf("\n=== Hash Tag Demonstration ===\n\n");

    // Demonstrate hash tags - channels with same tag go to same shard
    const char *room1_channels[] = {
        "{room:1}chat",
        "{room:1}events",
        "{room:1}members"
    };

    const char *room2_channels[] = {
        "{room:2}chat",
        "{room:2}events",
        "{room:2}members"
    };

    printf("Publishing to Room 1 channels (same shard due to {room:1} tag):\n");
    for (int i = 0; i < 3; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Room 1 message on %s", room1_channels[i]);
        slk_spublish(shard_ctx, room1_channels[i], msg, strlen(msg));
        printf("  %s: %s\n", room1_channels[i], msg);
    }

    printf("\nPublishing to Room 2 channels (same shard due to {room:2} tag):\n");
    for (int i = 0; i < 3; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Room 2 message on %s", room2_channels[i]);
        slk_spublish(shard_ctx, room2_channels[i], msg, strlen(msg));
        printf("  %s: %s\n", room2_channels[i], msg);
    }

    printf("\n=== Multi-threaded Performance Test ===\n\n");

    int num_publishers = 4;
    int messages_per_publisher = MESSAGE_COUNT / num_publishers;

    pthread_t pub_threads[4];
    publisher_args_t pub_args[4];

    pthread_t sub_threads[4];
    subscriber_args_t sub_args[4];

    // Start subscribers first
    for (int i = 0; i < num_publishers; i++) {
        sub_args[i].shard_ctx = shard_ctx;
        sub_args[i].ctx = ctx;

        char *pattern = malloc(32);
        snprintf(pattern, 32, "events.thread%d", i);
        sub_args[i].channel_pattern = pattern;
        sub_args[i].expected_count = messages_per_publisher;

        if (pthread_create(&sub_threads[i], NULL, subscriber_thread, &sub_args[i]) != 0) {
            perror("pthread_create subscriber");
            slk_sharded_pubsub_destroy(&shard_ctx);
            slk_ctx_destroy(ctx);
            return 1;
        }
    }

    // Allow subscribers to set up
    usleep(500000);  // 500ms

    // Start publishers
    printf("Starting %d publisher threads...\n", num_publishers);
    for (int i = 0; i < num_publishers; i++) {
        pub_args[i].shard_ctx = shard_ctx;
        pub_args[i].thread_id = i;
        pub_args[i].message_count = messages_per_publisher;

        if (pthread_create(&pub_threads[i], NULL, publisher_thread, &pub_args[i]) != 0) {
            perror("pthread_create publisher");
            slk_sharded_pubsub_destroy(&shard_ctx);
            slk_ctx_destroy(ctx);
            return 1;
        }
    }

    // Wait for publishers
    for (int i = 0; i < num_publishers; i++) {
        pthread_join(pub_threads[i], NULL);
    }

    printf("\nAll publishers completed\n");

    // Wait for subscribers
    for (int i = 0; i < num_publishers; i++) {
        pthread_join(sub_threads[i], NULL);
        free((void *)sub_args[i].channel_pattern);
    }

    printf("\nAll subscribers completed\n");

    printf("\n=== Cleanup ===\n");

    CHECK(slk_sharded_pubsub_destroy(&shard_ctx),
          "slk_sharded_pubsub_destroy");

    slk_ctx_destroy(ctx);

    printf("Done.\n");
    return 0;
}

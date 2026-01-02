/*
 * Broker Pub/Sub Example
 *
 * Demonstrates the slk_pubsub_broker_* API for centralized message routing.
 * Shows how to create a broker, run it in background, and collect statistics.
 *
 * Build:
 *   cmake --build build
 *   ./build/examples/pubsub/broker_example
 */

#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#define CHECK(expr, msg) do { \
    if ((expr) < 0) { \
        perror(msg); \
        return NULL; \
    } \
} while(0)

#define CHECK_INT(expr, msg) do { \
    if ((expr) < 0) { \
        perror(msg); \
        return 1; \
    } \
} while(0)

// Shared broker for signal handling
static slk_pubsub_broker_t *g_broker = NULL;

// Publisher thread function
void *publisher_thread(void *arg __attribute__((unused))) {
    slk_ctx_t *ctx = slk_ctx_new();
    if (!ctx) {
        perror("slk_ctx_new");
        return NULL;
    }

    slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
    if (!pub) {
        perror("slk_socket(PUB)");
        slk_ctx_destroy(ctx);
        return NULL;
    }

    // Connect to broker frontend
    CHECK(slk_connect(pub, "tcp://127.0.0.1:5555"), "slk_connect");

    // Allow connection to establish
    usleep(100000);  // 100ms

    printf("[Publisher] Connected to broker\n");

    // Publish messages to different channels
    const char *channels[] = {
        "news.sports",
        "news.weather",
        "events.login",
        "events.logout",
        "alerts.critical"
    };

    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < 5; i++) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Message %d on %s", round * 5 + i, channels[i]);

            CHECK(slk_send(pub, channels[i], strlen(channels[i]), SLK_SNDMORE),
                  "slk_send channel");
            CHECK(slk_send(pub, msg, strlen(msg), 0),
                  "slk_send message");

            printf("[Publisher] Sent: [%s] %s\n", channels[i], msg);
        }
        usleep(200000);  // 200ms between rounds
    }

    printf("[Publisher] Finished sending messages\n");

    slk_close(pub);
    slk_ctx_destroy(ctx);
    return NULL;
}

// Subscriber thread function
void *subscriber_thread(void *arg) {
    const char *name = (const char *)arg;
    slk_ctx_t *ctx = slk_ctx_new();
    if (!ctx) {
        perror("slk_ctx_new");
        return NULL;
    }

    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    if (!sub) {
        perror("slk_socket(SUB)");
        slk_ctx_destroy(ctx);
        return NULL;
    }

    // Connect to broker backend
    CHECK(slk_connect(sub, "tcp://127.0.0.1:5556"), "slk_connect");

    // Subscribe to channels based on subscriber name
    if (strcmp(name, "Sub1") == 0) {
        // Subscribe to all news
        CHECK(slk_setsockopt(sub, SLK_SUBSCRIBE, "news.", 5),
              "SLK_SUBSCRIBE news.");
        printf("[%s] Subscribed to news.*\n", name);
    } else if (strcmp(name, "Sub2") == 0) {
        // Subscribe to all events
        CHECK(slk_setsockopt(sub, SLK_SUBSCRIBE, "events.", 7),
              "SLK_SUBSCRIBE events.");
        printf("[%s] Subscribed to events.*\n", name);
    } else {
        // Subscribe to everything
        CHECK(slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0),
              "SLK_SUBSCRIBE all");
        printf("[%s] Subscribed to all channels\n", name);
    }

    // Allow subscription to propagate
    usleep(100000);  // 100ms

    printf("[%s] Ready to receive messages\n", name);

    // Receive messages with timeout using loop
    int count = 0;
    int no_msg_count = 0;
    while (no_msg_count < 10) {  // Exit after 10 consecutive empty receives
        char channel[256];
        char message[1024];

        int rc = slk_recv(sub, channel, sizeof(channel), SLK_DONTWAIT);
        if (rc < 0) {
            if (slk_errno() == SLK_EAGAIN) {
                usleep(100000);  // 100ms
                no_msg_count++;
                continue;
            }
            perror("slk_recv channel");
            break;
        }
        no_msg_count = 0;
        channel[rc] = '\0';

        rc = slk_recv(sub, message, sizeof(message), 0);
        if (rc < 0) {
            perror("slk_recv message");
            break;
        }
        message[rc] = '\0';

        count++;
        printf("[%s] Received: [%s] %s\n", name, channel, message);
    }

    printf("[%s] Total messages received: %d\n", name, count);

    slk_close(sub);
    slk_ctx_destroy(ctx);
    return NULL;
}

// Monitor thread function
void *monitor_thread(void *arg) {
    slk_pubsub_broker_t *broker = (slk_pubsub_broker_t *)arg;

    printf("[Monitor] Starting statistics monitoring\n");

    for (int i = 0; i < 10; i++) {
        sleep(1);

        size_t msg_count = 0;
        if (slk_pubsub_broker_stats(broker, &msg_count) == 0) {
            printf("[Monitor] Messages relayed: %zu\n", msg_count);
        }
    }

    printf("[Monitor] Stopping monitoring\n");
    return NULL;
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("=== ServerLink Broker Pub/Sub Example ===\n\n");

    // Initialize context
    slk_ctx_t *ctx = slk_ctx_new();
    if (!ctx) {
        perror("slk_ctx_new");
        return 1;
    }

    // Create broker
    printf("Creating broker...\n");
    printf("  Frontend (publishers): tcp://*:5555\n");
    printf("  Backend (subscribers): tcp://*:5556\n");

    slk_pubsub_broker_t *broker = slk_pubsub_broker_new(ctx,
        "tcp://*:5555",  // Frontend for publishers
        "tcp://*:5556"   // Backend for subscribers
    );

    if (!broker) {
        perror("slk_pubsub_broker_new");
        slk_ctx_destroy(ctx);
        return 1;
    }

    g_broker = broker;

    // Start broker in background thread
    printf("Starting broker in background...\n");
    CHECK_INT(slk_pubsub_broker_start(broker), "slk_pubsub_broker_start");

    // Allow broker to start
    usleep(500000);  // 500ms

    printf("\n=== Starting Publisher and Subscribers ===\n\n");

    // Create threads
    pthread_t pub_thread, sub1_thread, sub2_thread, sub3_thread, mon_thread;

    // Start monitor thread
    if (pthread_create(&mon_thread, NULL, monitor_thread, broker) != 0) {
        perror("pthread_create monitor");
        slk_pubsub_broker_stop(broker);
        slk_pubsub_broker_destroy(&broker);
        slk_ctx_destroy(ctx);
        return 1;
    }

    // Start subscriber threads
    if (pthread_create(&sub1_thread, NULL, subscriber_thread, "Sub1") != 0) {
        perror("pthread_create sub1");
        slk_pubsub_broker_stop(broker);
        slk_pubsub_broker_destroy(&broker);
        slk_ctx_destroy(ctx);
        return 1;
    }

    if (pthread_create(&sub2_thread, NULL, subscriber_thread, "Sub2") != 0) {
        perror("pthread_create sub2");
        slk_pubsub_broker_stop(broker);
        slk_pubsub_broker_destroy(&broker);
        slk_ctx_destroy(ctx);
        return 1;
    }

    if (pthread_create(&sub3_thread, NULL, subscriber_thread, "Sub3") != 0) {
        perror("pthread_create sub3");
        slk_pubsub_broker_stop(broker);
        slk_pubsub_broker_destroy(&broker);
        slk_ctx_destroy(ctx);
        return 1;
    }

    // Give subscribers time to connect and subscribe
    usleep(500000);  // 500ms

    // Start publisher thread
    if (pthread_create(&pub_thread, NULL, publisher_thread, NULL) != 0) {
        perror("pthread_create publisher");
        slk_pubsub_broker_stop(broker);
        slk_pubsub_broker_destroy(&broker);
        slk_ctx_destroy(ctx);
        return 1;
    }

    // Wait for threads to complete
    pthread_join(pub_thread, NULL);
    printf("\n[Main] Publisher thread completed\n");

    pthread_join(sub1_thread, NULL);
    printf("[Main] Subscriber 1 thread completed\n");

    pthread_join(sub2_thread, NULL);
    printf("[Main] Subscriber 2 thread completed\n");

    pthread_join(sub3_thread, NULL);
    printf("[Main] Subscriber 3 thread completed\n");

    // Stop broker
    printf("\n=== Stopping Broker ===\n");
    CHECK_INT(slk_pubsub_broker_stop(broker), "slk_pubsub_broker_stop");

    pthread_join(mon_thread, NULL);
    printf("[Main] Monitor thread completed\n");

    // Final statistics
    size_t final_count = 0;
    if (slk_pubsub_broker_stats(broker, &final_count) == 0) {
        printf("\nFinal statistics:\n");
        printf("  Total messages relayed: %zu\n", final_count);
    }

    // Cleanup
    printf("\n=== Cleanup ===\n");
    CHECK_INT(slk_pubsub_broker_destroy(&broker), "slk_pubsub_broker_destroy");
    slk_ctx_destroy(ctx);

    printf("Done.\n");
    return 0;
}

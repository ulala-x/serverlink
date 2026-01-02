/*
 * Pattern Subscription Example
 *
 * Demonstrates SLK_PSUBSCRIBE and SLK_PUNSUBSCRIBE usage with glob patterns.
 * Shows how to subscribe to multiple channels using wildcard patterns.
 *
 * Build:
 *   cmake --build build
 *   ./build/examples/pubsub/psubscribe_example
 */

#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define CHECK(expr, msg) do { \
    if ((expr) < 0) { \
        perror(msg); \
        return 1; \
    } \
} while(0)

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("=== ServerLink Pattern Subscription Example ===\n\n");

    // Initialize ServerLink context
    slk_ctx_t *ctx = slk_ctx_new();
    if (!ctx) {
        perror("slk_ctx_new");
        return 1;
    }

    // Create publisher socket
    slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
    if (!pub) {
        perror("slk_socket(PUB)");
        slk_ctx_destroy(ctx);
        return 1;
    }
    CHECK(slk_bind(pub, "inproc://events"), "slk_bind");

    // Create subscriber socket
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    if (!sub) {
        perror("slk_socket(SUB)");
        slk_close(pub);
        slk_ctx_destroy(ctx);
        return 1;
    }
    CHECK(slk_connect(sub, "inproc://events"), "slk_connect");

    // Pattern 1: Subscribe to all news channels
    printf("1. Subscribing to pattern: news.*\n");
    CHECK(slk_setsockopt(sub, SLK_PSUBSCRIBE, "news.*", 6),
          "SLK_PSUBSCRIBE news.*");

    // Pattern 2: Subscribe to user channels with single character suffix
    printf("2. Subscribing to pattern: user.?\n");
    CHECK(slk_setsockopt(sub, SLK_PSUBSCRIBE, "user.?", 6),
          "SLK_PSUBSCRIBE user.?");

    // Pattern 3: Subscribe to alerts with priority levels
    printf("3. Subscribing to pattern: alert.[0-9]\n");
    CHECK(slk_setsockopt(sub, SLK_PSUBSCRIBE, "alert.[0-9]", 11),
          "SLK_PSUBSCRIBE alert.[0-9]");

    // Allow time for subscription to propagate
    usleep(100000);  // 100ms

    printf("\n=== Publishing Test Messages ===\n\n");

    // Test messages for news.* pattern
    const char *news_channels[] = {
        "news.sports",
        "news.weather",
        "news.politics"
    };
    for (int i = 0; i < 3; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Breaking: %s update", news_channels[i]);
        printf("Publishing to %s: %s\n", news_channels[i], msg);

        CHECK(slk_send(pub, news_channels[i], strlen(news_channels[i]), SLK_SNDMORE),
              "slk_send channel");
        CHECK(slk_send(pub, msg, strlen(msg), 0),
              "slk_send message");
    }

    // Test messages for user.? pattern (should match)
    const char *user_channels_match[] = {
        "user.1",
        "user.a",
        "user.Z"
    };
    for (int i = 0; i < 3; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg), "User activity on %s", user_channels_match[i]);
        printf("Publishing to %s: %s\n", user_channels_match[i], msg);

        CHECK(slk_send(pub, user_channels_match[i], strlen(user_channels_match[i]), SLK_SNDMORE),
              "slk_send channel");
        CHECK(slk_send(pub, msg, strlen(msg), 0),
              "slk_send message");
    }

    // Test messages for user.? pattern (should NOT match - too long)
    const char *user_channels_nomatch[] = {
        "user.123",
        "user.admin"
    };
    for (int i = 0; i < 2; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg), "User activity on %s (should NOT match user.?)",
                 user_channels_nomatch[i]);
        printf("Publishing to %s: %s\n", user_channels_nomatch[i], msg);

        CHECK(slk_send(pub, user_channels_nomatch[i], strlen(user_channels_nomatch[i]), SLK_SNDMORE),
              "slk_send channel");
        CHECK(slk_send(pub, msg, strlen(msg), 0),
              "slk_send message");
    }

    // Test messages for alert.[0-9] pattern
    for (int level = 0; level <= 9; level++) {
        char channel[32];
        char msg[256];
        snprintf(channel, sizeof(channel), "alert.%d", level);
        snprintf(msg, sizeof(msg), "Alert level %d triggered", level);
        printf("Publishing to %s: %s\n", channel, msg);

        CHECK(slk_send(pub, channel, strlen(channel), SLK_SNDMORE),
              "slk_send channel");
        CHECK(slk_send(pub, msg, strlen(msg), 0),
              "slk_send message");
    }

    printf("\n=== Receiving Messages ===\n\n");

    // Receive all messages (3 news + 3 user.? + 10 alert.[0-9] = 16 total)
    int expected_count = 16;
    int received_count = 0;

    for (int i = 0; i < expected_count; i++) {
        char channel[256];
        char message[1024];

        int rc = slk_recv(sub, channel, sizeof(channel), 0);
        if (rc < 0) {
            perror("slk_recv channel");
            break;
        }
        channel[rc] = '\0';

        rc = slk_recv(sub, message, sizeof(message), 0);
        if (rc < 0) {
            perror("slk_recv message");
            break;
        }
        message[rc] = '\0';

        printf("[%2d] Channel: %-20s Message: %s\n",
               i + 1, channel, message);
        received_count++;
    }

    printf("\nReceived %d out of %d expected messages\n",
           received_count, expected_count);

    // Demonstrate unsubscribing from a pattern
    printf("\n=== Unsubscribing from news.* ===\n");
    CHECK(slk_setsockopt(sub, SLK_PUNSUBSCRIBE, "news.*", 6),
          "SLK_PUNSUBSCRIBE news.*");

    usleep(100000);  // 100ms

    // Publish more news (should not be received)
    printf("\nPublishing to news.tech (should not be received):\n");
    CHECK(slk_send(pub, "news.tech", 9, SLK_SNDMORE),
          "slk_send channel");
    CHECK(slk_send(pub, "New tech release", 16, 0),
          "slk_send message");

    // Try to receive with non-blocking flag (should get EAGAIN)
    char channel[256];
    int rc = slk_recv(sub, channel, sizeof(channel), SLK_DONTWAIT);
    if (rc < 0 && slk_errno() == SLK_EAGAIN) {
        printf("No message received (expected after unsubscribe)\n");
    } else if (rc < 0) {
        perror("slk_recv");
    } else {
        printf("WARNING: Received unexpected message after unsubscribe\n");
    }

    // Cleanup
    printf("\n=== Cleanup ===\n");
    slk_close(sub);
    slk_close(pub);
    slk_ctx_destroy(ctx);

    printf("Done.\n");
    return 0;
}

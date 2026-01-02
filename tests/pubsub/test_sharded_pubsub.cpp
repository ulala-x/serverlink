/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Test Sharded Pub/Sub */

#include "serverlink/serverlink.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Helper: sleep for milliseconds
static void msleep(int ms)
{
    slk_sleep(ms);
}

// Test 1: Basic sharded pub/sub
static void test_basic_sharded_pubsub()
{
    printf("Running test_basic_sharded_pubsub...\n");

    // Create context
    printf("  Creating context...\n");
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);
    printf("  Context created\n");

    // Create sharded pub/sub with 4 shards
    printf("  Creating sharded pub/sub...\n");
    slk_sharded_pubsub_t *shard_ctx = slk_sharded_pubsub_new(ctx, 4);
    printf("  shard_ctx = %p\n", (void*)shard_ctx);
    assert(shard_ctx);
    printf("  Sharded pub/sub created\n");

    // Create subscriber
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    assert(sub);

    // Subscribe to a channel
    int rc = slk_ssubscribe(shard_ctx, sub, "news");
    assert(rc == 0);

    // Allow time for subscription to propagate
    msleep(100);

    // Publish message
    const char *msg = "Hello World";
    rc = slk_spublish(shard_ctx, "news", msg, strlen(msg));
    assert(rc == (int)strlen(msg));

    // Receive message
    char buf[256];
    int nbytes = slk_recv(sub, buf, sizeof(buf), 0);
    assert(nbytes > 0);
    assert(memcmp(buf, "news", 4) == 0);

    // Receive data frame
    nbytes = slk_recv(sub, buf, sizeof(buf), 0);
    assert(nbytes == (int)strlen(msg));
    assert(memcmp(buf, msg, strlen(msg)) == 0);

    // Cleanup
    slk_close(sub);
    slk_sharded_pubsub_destroy(&shard_ctx);
    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

// Test 2: Hash tag support
static void test_hash_tags()
{
    printf("Running test_hash_tags...\n");

    // Create context
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    // Create sharded pub/sub with 8 shards
    slk_sharded_pubsub_t *shard_ctx = slk_sharded_pubsub_new(ctx, 8);
    assert(shard_ctx);

    // Create subscriber
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    assert(sub);

    // Subscribe to channels with same hash tag
    int rc = slk_ssubscribe(shard_ctx, sub, "{room:1}chat");
    assert(rc == 0);
    rc = slk_ssubscribe(shard_ctx, sub, "{room:1}members");
    assert(rc == 0);

    msleep(100);

    // Publish to both channels
    const char *msg1 = "Hello from chat";
    rc = slk_spublish(shard_ctx, "{room:1}chat", msg1, strlen(msg1));
    assert(rc == (int)strlen(msg1));

    const char *msg2 = "User joined";
    rc = slk_spublish(shard_ctx, "{room:1}members", msg2, strlen(msg2));
    assert(rc == (int)strlen(msg2));

    // Receive first message
    char buf[256];
    int nbytes = slk_recv(sub, buf, sizeof(buf), 0);  // channel
    assert(nbytes > 0);
    nbytes = slk_recv(sub, buf, sizeof(buf), 0);  // data
    assert(nbytes > 0);

    // Receive second message
    nbytes = slk_recv(sub, buf, sizeof(buf), 0);  // channel
    assert(nbytes > 0);
    nbytes = slk_recv(sub, buf, sizeof(buf), 0);  // data
    assert(nbytes > 0);

    // Cleanup
    slk_close(sub);
    slk_sharded_pubsub_destroy(&shard_ctx);
    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

// Test 3: Multiple subscribers
static void test_multiple_subscribers()
{
    printf("Running test_multiple_subscribers...\n");

    // Create context
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    // Create sharded pub/sub
    slk_sharded_pubsub_t *shard_ctx = slk_sharded_pubsub_new(ctx, 4);
    assert(shard_ctx);

    // Create 3 subscribers
    slk_socket_t *sub1 = slk_socket(ctx, SLK_SUB);
    slk_socket_t *sub2 = slk_socket(ctx, SLK_SUB);
    slk_socket_t *sub3 = slk_socket(ctx, SLK_SUB);
    assert(sub1 && sub2 && sub3);

    // All subscribe to same channel
    int rc = slk_ssubscribe(shard_ctx, sub1, "broadcast");
    assert(rc == 0);
    rc = slk_ssubscribe(shard_ctx, sub2, "broadcast");
    assert(rc == 0);
    rc = slk_ssubscribe(shard_ctx, sub3, "broadcast");
    assert(rc == 0);

    msleep(100);

    // Publish one message
    const char *msg = "Hello All";
    rc = slk_spublish(shard_ctx, "broadcast", msg, strlen(msg));
    assert(rc == (int)strlen(msg));

    // All subscribers should receive it
    char buf[256];

    // Sub1 receives
    int nbytes = slk_recv(sub1, buf, sizeof(buf), 0);  // channel
    assert(nbytes > 0);
    nbytes = slk_recv(sub1, buf, sizeof(buf), 0);  // data
    assert(nbytes == (int)strlen(msg));

    // Sub2 receives
    nbytes = slk_recv(sub2, buf, sizeof(buf), 0);  // channel
    assert(nbytes > 0);
    nbytes = slk_recv(sub2, buf, sizeof(buf), 0);  // data
    assert(nbytes == (int)strlen(msg));

    // Sub3 receives
    nbytes = slk_recv(sub3, buf, sizeof(buf), 0);  // channel
    assert(nbytes > 0);
    nbytes = slk_recv(sub3, buf, sizeof(buf), 0);  // data
    assert(nbytes == (int)strlen(msg));

    // Cleanup
    slk_close(sub1);
    slk_close(sub2);
    slk_close(sub3);
    slk_sharded_pubsub_destroy(&shard_ctx);
    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

// Test 4: Channel distribution across shards
static void test_shard_distribution()
{
    printf("Running test_shard_distribution...\n");

    // Create context
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    // Create sharded pub/sub with 16 shards
    slk_sharded_pubsub_t *shard_ctx = slk_sharded_pubsub_new(ctx, 16);
    assert(shard_ctx);

    // Create subscriber
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    assert(sub);

    // Subscribe to multiple channels
    const char *channels[] = {
        "channel1", "channel2", "channel3", "channel4",
        "channel5", "channel6", "channel7", "channel8"
    };
    const int num_channels = sizeof(channels) / sizeof(channels[0]);

    for (int i = 0; i < num_channels; i++) {
        int rc = slk_ssubscribe(shard_ctx, sub, channels[i]);
        assert(rc == 0);
    }

    msleep(100);

    // Publish to all channels
    for (int i = 0; i < num_channels; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Message %d", i);
        int rc = slk_spublish(shard_ctx, channels[i], msg, strlen(msg));
        assert(rc == (int)strlen(msg));
    }

    // Receive all messages (order may vary due to sharding)
    char buf[256];
    int received = 0;
    for (int i = 0; i < num_channels; i++) {
        int nbytes = slk_recv(sub, buf, sizeof(buf), 0);  // channel
        if (nbytes > 0) {
            nbytes = slk_recv(sub, buf, sizeof(buf), 0);  // data
            if (nbytes > 0) {
                received++;
            }
        }
    }

    // Should receive all messages
    assert(received == num_channels);

    // Cleanup
    slk_close(sub);
    slk_sharded_pubsub_destroy(&shard_ctx);
    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

// Test 5: Unsubscribe
static void test_unsubscribe()
{
    printf("Running test_unsubscribe...\n");

    // Create context
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    // Create sharded pub/sub
    slk_sharded_pubsub_t *shard_ctx = slk_sharded_pubsub_new(ctx, 4);
    assert(shard_ctx);

    // Create subscriber
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    assert(sub);

    // Subscribe to channel
    int rc = slk_ssubscribe(shard_ctx, sub, "test");
    assert(rc == 0);

    msleep(100);

    // Publish and receive
    const char *msg1 = "Message 1";
    rc = slk_spublish(shard_ctx, "test", msg1, strlen(msg1));
    assert(rc == (int)strlen(msg1));

    char buf[256];
    int nbytes = slk_recv(sub, buf, sizeof(buf), 0);  // channel
    assert(nbytes > 0);
    nbytes = slk_recv(sub, buf, sizeof(buf), 0);  // data
    assert(nbytes > 0);

    // Unsubscribe
    rc = slk_sunsubscribe(shard_ctx, sub, "test");
    assert(rc == 0);

    msleep(100);

    // Publish again
    const char *msg2 = "Message 2";
    rc = slk_spublish(shard_ctx, "test", msg2, strlen(msg2));
    assert(rc == (int)strlen(msg2));

    // Should NOT receive (non-blocking check)
    nbytes = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    assert(nbytes == -1);  // No message

    // Cleanup
    slk_close(sub);
    slk_sharded_pubsub_destroy(&shard_ctx);
    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

// Test 6: HWM setting
static void test_hwm()
{
    printf("Running test_hwm...\n");

    // Create context
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    // Create sharded pub/sub
    slk_sharded_pubsub_t *shard_ctx = slk_sharded_pubsub_new(ctx, 2);
    assert(shard_ctx);

    // Set low HWM
    int rc = slk_sharded_pubsub_set_hwm(shard_ctx, 10);
    assert(rc == 0);

    // Create subscriber (but don't connect yet - to fill queue)
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    assert(sub);

    // Subscribe
    rc = slk_ssubscribe(shard_ctx, sub, "test");
    assert(rc == 0);

    msleep(50);

    // Try to publish many messages
    // Some may be dropped or blocked depending on HWM behavior
    for (int i = 0; i < 20; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Message %d", i);
        // Use non-blocking to avoid hanging
        slk_spublish(shard_ctx, "test", msg, strlen(msg));
    }

    // Just verify we can set HWM without error
    // Actual HWM behavior testing would require more complex setup

    // Cleanup
    slk_close(sub);
    slk_sharded_pubsub_destroy(&shard_ctx);
    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

// Test 7: Different shard counts
static void test_shard_counts()
{
    printf("Running test_shard_counts...\n");

    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    // Test various shard counts
    int shard_counts[] = {1, 2, 4, 8, 16, 32};
    int num_counts = sizeof(shard_counts) / sizeof(shard_counts[0]);

    for (int i = 0; i < num_counts; i++) {
        slk_sharded_pubsub_t *shard_ctx = slk_sharded_pubsub_new(ctx, shard_counts[i]);
        assert(shard_ctx);

        // Basic publish/subscribe
        slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
        assert(sub);

        int rc = slk_ssubscribe(shard_ctx, sub, "test");
        assert(rc == 0);

        msleep(50);

        const char *msg = "test";
        rc = slk_spublish(shard_ctx, "test", msg, strlen(msg));
        assert(rc == (int)strlen(msg));

        char buf[256];
        int nbytes = slk_recv(sub, buf, sizeof(buf), 0);
        assert(nbytes > 0);
        nbytes = slk_recv(sub, buf, sizeof(buf), 0);
        assert(nbytes > 0);

        slk_close(sub);
        slk_sharded_pubsub_destroy(&shard_ctx);
    }

    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

// Test 8: Error handling
static void test_error_handling()
{
    printf("Running test_error_handling...\n");

    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    // Invalid shard count (0)
    slk_sharded_pubsub_t *shard_ctx = slk_sharded_pubsub_new(ctx, 0);
    assert(shard_ctx == NULL);

    // Invalid shard count (too large)
    shard_ctx = slk_sharded_pubsub_new(ctx, 2000);
    assert(shard_ctx == NULL);

    // Valid shard count
    shard_ctx = slk_sharded_pubsub_new(ctx, 4);
    assert(shard_ctx);

    // NULL channel
    int rc = slk_spublish(shard_ctx, NULL, "data", 4);
    assert(rc == -1);

    // NULL socket
    rc = slk_ssubscribe(shard_ctx, NULL, "test");
    assert(rc == -1);

    // Empty channel
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    rc = slk_ssubscribe(shard_ctx, sub, "");
    assert(rc == -1);

    // Invalid HWM
    rc = slk_sharded_pubsub_set_hwm(shard_ctx, -1);
    assert(rc == -1);

    slk_close(sub);
    slk_sharded_pubsub_destroy(&shard_ctx);
    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

int main()
{
    fprintf(stderr, "STARTING TEST PROGRAM\n");
    fflush(stderr);

    printf("ServerLink Sharded Pub/Sub Tests\n");
    printf("=================================\n\n");
    fflush(stdout);

    fprintf(stderr, "About to call test_basic_sharded_pubsub\n");
    fflush(stderr);

    test_basic_sharded_pubsub();
    test_hash_tags();
    test_multiple_subscribers();
    test_shard_distribution();
    test_unsubscribe();
    test_hwm();
    test_shard_counts();
    test_error_handling();

    printf("\n=================================\n");
    printf("All tests passed!\n");
    return 0;
}

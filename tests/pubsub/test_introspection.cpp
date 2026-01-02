/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Pub/Sub Introspection API Integration Test */

#include <serverlink/serverlink.h>
#include "../testutil.hpp"
#include <string.h>
#include <string>
#include <algorithm>
#include <vector>

// Helper: drain subscription messages from XPUB socket
static void drain_subscriptions(slk_socket_t *xpub, int expected_count)
{
    char sub_msg[256];
    for (int i = 0; i < expected_count; i++) {
        // Wait for message with timeout
        int rc = -1;
        for (int retry = 0; retry < 100 && rc < 0; retry++) {
            rc = slk_recv(xpub, sub_msg, sizeof(sub_msg), SLK_DONTWAIT);
            if (rc < 0) slk_sleep(10);
        }
        assert(rc > 0);
    }
}

// Test basic channel listing
static void test_channels_basic()
{
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    slk_socket_t *pub = slk_socket(ctx, SLK_XPUB);
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    assert(pub && sub);

    assert(slk_bind(pub, "inproc://test") == 0);
    assert(slk_connect(sub, "inproc://test") == 0);

    // Subscribe to channels
    assert(slk_setsockopt(sub, SLK_SUBSCRIBE, "news", 4) == 0);
    assert(slk_setsockopt(sub, SLK_SUBSCRIBE, "sports", 6) == 0);
    assert(slk_setsockopt(sub, SLK_SUBSCRIBE, "weather", 7) == 0);

    // Read subscription notifications to trigger registry hooks
    drain_subscriptions(pub, 3);

    // Query all channels
    char **channels = NULL;
    size_t count = 0;
    assert(slk_pubsub_channels(ctx, "", &channels, &count) == 0);
    assert(count == 3);

    // Verify channels (sorted)
    std::vector<std::string> chan_vec;
    for (size_t i = 0; i < count; ++i) {
        chan_vec.push_back(channels[i]);
    }
    std::sort(chan_vec.begin(), chan_vec.end());
    assert(chan_vec[0] == "news");
    assert(chan_vec[1] == "sports");
    assert(chan_vec[2] == "weather");

    slk_pubsub_channels_free(channels, count);

    // Cleanup
    slk_close(sub);
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

// Test channel pattern matching
static void test_channels_pattern()
{
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    slk_socket_t *pub = slk_socket(ctx, SLK_XPUB);
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    assert(pub && sub);

    assert(slk_bind(pub, "inproc://test") == 0);
    assert(slk_connect(sub, "inproc://test") == 0);

    // Subscribe to various channels
    assert(slk_setsockopt(sub, SLK_SUBSCRIBE, "news.tech", 9) == 0);
    assert(slk_setsockopt(sub, SLK_SUBSCRIBE, "news.sports", 11) == 0);
    assert(slk_setsockopt(sub, SLK_SUBSCRIBE, "weather.local", 13) == 0);

    // Read subscription notifications
    drain_subscriptions(pub, 3);

    // Query with pattern
    char **channels = NULL;
    size_t count = 0;
    assert(slk_pubsub_channels(ctx, "news.*", &channels, &count) == 0);
    assert(count == 2);

    std::vector<std::string> chan_vec;
    for (size_t i = 0; i < count; ++i) {
        chan_vec.push_back(channels[i]);
    }
    std::sort(chan_vec.begin(), chan_vec.end());
    assert(chan_vec[0] == "news.sports");
    assert(chan_vec[1] == "news.tech");

    slk_pubsub_channels_free(channels, count);

    // Cleanup
    slk_close(sub);
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

// Test subscriber count
static void test_numsub()
{
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    slk_socket_t *pub = slk_socket(ctx, SLK_XPUB);

    // Enable verbose mode to receive ALL subscription notifications
    int verbose = 1;
    assert(slk_setsockopt(pub, SLK_XPUB_VERBOSE, &verbose, sizeof(verbose)) == 0);

    slk_socket_t *sub1 = slk_socket(ctx, SLK_SUB);
    slk_socket_t *sub2 = slk_socket(ctx, SLK_SUB);
    slk_socket_t *sub3 = slk_socket(ctx, SLK_SUB);
    assert(pub && sub1 && sub2 && sub3);

    assert(slk_bind(pub, "inproc://test") == 0);
    assert(slk_connect(sub1, "inproc://test") == 0);
    assert(slk_connect(sub2, "inproc://test") == 0);
    assert(slk_connect(sub3, "inproc://test") == 0);

    // Multiple subscribers to same channel
    assert(slk_setsockopt(sub1, SLK_SUBSCRIBE, "news", 4) == 0);
    assert(slk_setsockopt(sub2, SLK_SUBSCRIBE, "news", 4) == 0);
    assert(slk_setsockopt(sub3, SLK_SUBSCRIBE, "sports", 6) == 0);

    // Read all 3 subscription messages (each pipe sends its subscription)
    // Even though only 2 unique channels, we get 3 messages
    drain_subscriptions(pub, 3);

    // Query subscriber counts
    const char *channels[] = {"news", "sports", "nonexistent"};
    size_t numsub[3] = {0};
    assert(slk_pubsub_numsub(ctx, channels, 3, numsub) == 0);

    assert(numsub[0] == 2);  // news has 2 subscribers
    assert(numsub[1] == 1);  // sports has 1 subscriber
    assert(numsub[2] == 0);  // nonexistent has 0 subscribers

    // Cleanup
    slk_close(sub3);
    slk_close(sub2);
    slk_close(sub1);
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

// Test unsubscribe updates registry
static void test_unsubscribe()
{
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    slk_socket_t *pub = slk_socket(ctx, SLK_XPUB);
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    assert(pub && sub);

    assert(slk_bind(pub, "inproc://test") == 0);
    assert(slk_connect(sub, "inproc://test") == 0);

    // Subscribe
    assert(slk_setsockopt(sub, SLK_SUBSCRIBE, "channel1", 8) == 0);
    drain_subscriptions(pub, 1);

    // Verify subscription
    char **channels = NULL;
    size_t count = 0;
    assert(slk_pubsub_channels(ctx, "", &channels, &count) == 0);
    assert(count == 1);
    slk_pubsub_channels_free(channels, count);

    // Unsubscribe
    assert(slk_setsockopt(sub, SLK_UNSUBSCRIBE, "channel1", 8) == 0);
    drain_subscriptions(pub, 1);  // Read unsubscribe notification

    // Verify channel removed
    channels = NULL;
    count = 0;
    assert(slk_pubsub_channels(ctx, "", &channels, &count) == 0);
    assert(count == 0);
    slk_pubsub_channels_free(channels, count);

    // Cleanup
    slk_close(sub);
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

// Test numpat (pattern subscriptions)
static void test_numpat()
{
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    // Initially no pattern subscriptions
    int numpat = slk_pubsub_numpat(ctx);
    assert(numpat == 0);

    // Note: This test will be more meaningful once PSUBSCRIBE is implemented
    // For now, just verify the API works

    slk_ctx_destroy(ctx);
}

// Test empty results
static void test_empty()
{
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    // Query with no subscriptions
    char **channels = NULL;
    size_t count = 0;
    assert(slk_pubsub_channels(ctx, "", &channels, &count) == 0);
    assert(count == 0);
    assert(channels == NULL || count == 0);  // Handle both possible implementations
    slk_pubsub_channels_free(channels, count);

    slk_ctx_destroy(ctx);
}

int main()
{
    printf("Testing Pub/Sub Introspection API...\n");

    RUN_TEST(test_channels_basic);
    RUN_TEST(test_channels_pattern);
    RUN_TEST(test_numsub);
    RUN_TEST(test_unsubscribe);
    RUN_TEST(test_numpat);
    RUN_TEST(test_empty);

    printf("All Pub/Sub introspection tests passed!\n");
    return 0;
}

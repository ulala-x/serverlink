/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Pattern subscription integration test */

#include "../testutil.hpp"
#include <string.h>
#include <unistd.h>

// Test basic pattern subscription with PUB/SUB
static void test_psubscribe_basic()
{
    slk_ctx_t *ctx = slk_ctx_new();
    TEST_ASSERT_NOT_NULL(ctx);

    // Create PUB and SUB sockets
    slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    TEST_ASSERT_NOT_NULL(pub);
    TEST_ASSERT_NOT_NULL(sub);

    // Bind and connect
    TEST_SUCCESS(slk_bind(pub, "inproc://test_psubscribe"));
    TEST_SUCCESS(slk_connect(sub, "inproc://test_psubscribe"));

    // Subscribe to pattern "news.*"
    TEST_SUCCESS(slk_setsockopt(sub, SLK_PSUBSCRIBE, "news.*", 6));

    // Give time for subscription to propagate
    usleep(50000); // 50ms

    // Publish messages
    TEST_ASSERT(slk_send(pub, "news.sports", 11, 0) == 11);
    TEST_ASSERT(slk_send(pub, "news.tech", 9, 0) == 9);
    TEST_ASSERT(slk_send(pub, "weather.today", 13, 0) == 13); // Should not match

    // Receive matching messages
    char buf[64];
    int rc;

    rc = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    TEST_ASSERT(rc > 0);
    TEST_ASSERT_MEM_EQ(buf, "news.sports", 11);

    rc = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    TEST_ASSERT(rc > 0);
    TEST_ASSERT_MEM_EQ(buf, "news.tech", 9);

    // Non-matching message should not be received
    rc = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);

    slk_close(sub);
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

// Test pattern unsubscribe
static void test_punsubscribe()
{
    slk_ctx_t *ctx = slk_ctx_new();
    TEST_ASSERT_NOT_NULL(ctx);

    slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    TEST_ASSERT_NOT_NULL(pub);
    TEST_ASSERT_NOT_NULL(sub);

    TEST_SUCCESS(slk_bind(pub, "inproc://test_punsubscribe"));
    TEST_SUCCESS(slk_connect(sub, "inproc://test_punsubscribe"));

    // Subscribe then unsubscribe
    TEST_SUCCESS(slk_setsockopt(sub, SLK_PSUBSCRIBE, "event.*", 7));
    usleep(50000);
    TEST_SUCCESS(slk_setsockopt(sub, SLK_PUNSUBSCRIBE, "event.*", 7));
    usleep(50000);

    // Publish message
    TEST_ASSERT(slk_send(pub, "event.login", 11, 0) == 11);

    // Should not receive since we unsubscribed
    char buf[64];
    int rc = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);

    slk_close(sub);
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

// Test multiple patterns
static void test_multiple_patterns()
{
    slk_ctx_t *ctx = slk_ctx_new();
    TEST_ASSERT_NOT_NULL(ctx);

    slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    TEST_ASSERT_NOT_NULL(pub);
    TEST_ASSERT_NOT_NULL(sub);

    TEST_SUCCESS(slk_bind(pub, "inproc://test_multi_pattern"));
    TEST_SUCCESS(slk_connect(sub, "inproc://test_multi_pattern"));

    // Subscribe to multiple patterns
    TEST_SUCCESS(slk_setsockopt(sub, SLK_PSUBSCRIBE, "user.?", 6));
    TEST_SUCCESS(slk_setsockopt(sub, SLK_PSUBSCRIBE, "event.*", 7));
    usleep(50000);

    // Publish messages
    TEST_ASSERT(slk_send(pub, "user.1", 6, 0) == 6);
    TEST_ASSERT(slk_send(pub, "event.logout", 12, 0) == 12);
    TEST_ASSERT(slk_send(pub, "system.alert", 12, 0) == 12); // Should not match

    // Receive matching messages
    char buf[64];
    int rc;
    int count = 0;

    rc = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    if (rc > 0) count++;

    rc = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    if (rc > 0) count++;

    rc = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    if (rc > 0) count++;

    // Should have received exactly 2 messages
    TEST_ASSERT_EQ(count, 2);

    slk_close(sub);
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

// Test pattern with character class
static void test_pattern_char_class()
{
    slk_ctx_t *ctx = slk_ctx_new();
    TEST_ASSERT_NOT_NULL(ctx);

    slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    TEST_ASSERT_NOT_NULL(pub);
    TEST_ASSERT_NOT_NULL(sub);

    TEST_SUCCESS(slk_bind(pub, "inproc://test_char_class"));
    TEST_SUCCESS(slk_connect(sub, "inproc://test_char_class"));

    // Subscribe to pattern with character class
    TEST_SUCCESS(slk_setsockopt(sub, SLK_PSUBSCRIBE, "id.[0-9]", 8));
    usleep(50000);

    // Publish messages
    TEST_ASSERT(slk_send(pub, "id.5", 4, 0) == 4);
    TEST_ASSERT(slk_send(pub, "id.a", 4, 0) == 4); // Should not match

    // Receive matching message
    char buf[64];
    int rc;

    rc = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    TEST_ASSERT(rc == 4);
    TEST_ASSERT_MEM_EQ(buf, "id.5", 4);

    // Non-matching message should not be received
    rc = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);

    slk_close(sub);
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

// Test combining prefix and pattern subscriptions
static void test_mixed_subscriptions()
{
    slk_ctx_t *ctx = slk_ctx_new();
    TEST_ASSERT_NOT_NULL(ctx);

    slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    TEST_ASSERT_NOT_NULL(pub);
    TEST_ASSERT_NOT_NULL(sub);

    TEST_SUCCESS(slk_bind(pub, "inproc://test_mixed"));
    TEST_SUCCESS(slk_connect(sub, "inproc://test_mixed"));

    // Mix prefix subscription and pattern subscription
    TEST_SUCCESS(slk_setsockopt(sub, SLK_SUBSCRIBE, "data.", 5)); // Prefix
    TEST_SUCCESS(slk_setsockopt(sub, SLK_PSUBSCRIBE, "event.*", 7)); // Pattern
    usleep(50000);

    // Publish messages
    TEST_ASSERT(slk_send(pub, "data.123", 8, 0) == 8); // Matches prefix
    TEST_ASSERT(slk_send(pub, "event.click", 11, 0) == 11); // Matches pattern

    // Both should be received
    char buf[64];
    int rc;
    int count = 0;

    rc = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    if (rc > 0) count++;

    rc = slk_recv(sub, buf, sizeof(buf), SLK_DONTWAIT);
    if (rc > 0) count++;

    TEST_ASSERT_EQ(count, 2);

    slk_close(sub);
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

int main()
{
    printf("Running pattern subscription tests...\n");

    test_psubscribe_basic();
    test_punsubscribe();
    test_multiple_patterns();
    test_pattern_char_class();
    test_mixed_subscriptions();

    printf("All pattern subscription tests passed!\n");
    return 0;
}

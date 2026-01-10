/* ServerLink PUB-SUB Socket Unit Tests */
#include "../testutil.hpp"

static void test_pubsub_inproc()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *pub = test_socket_new(ctx, SLK_PUB);
    int rc = slk_bind(pub, "inproc://pubsub_test");
    TEST_ASSERT_EQ(rc, 0);

    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub, "inproc://pubsub_test");
    TEST_ASSERT_EQ(rc, 0);
    slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);

    test_sleep_ms(100);

    slk_send(pub, "Hello", 5, 0);
    char buf[256];
    rc = slk_recv(sub, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 5);

    test_socket_close(sub);
    test_socket_close(pub);
    test_context_destroy(ctx);
}

static void test_pubsub_tcp()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *pub = test_socket_new(ctx, SLK_XPUB); // Use XPUB for sync
    int rc = slk_bind(pub, "tcp://127.0.0.1:*");
    TEST_ASSERT_EQ(rc, 0);
    
    char endpoint[256]; size_t size = sizeof(endpoint);
    rc = slk_getsockopt(pub, SLK_LAST_ENDPOINT, endpoint, &size);
    TEST_ASSERT_EQ(rc, 0);

    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub, endpoint);
    TEST_ASSERT_EQ(rc, 0);
    slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);

    // Sync: Wait for XPUB to receive subscription
    char sync_buf[32];
    int retries = 50;
    while (retries-- > 0) {
        if (slk_recv(pub, sync_buf, sizeof(sync_buf), SLK_DONTWAIT) > 0) break;
        test_sleep_ms(10);
    }

    slk_send(pub, "TCP", 3, 0);
    rc = slk_recv(sub, sync_buf, sizeof(sync_buf), 0);
    TEST_ASSERT_EQ(rc, 3);

    test_socket_close(sub);
    test_socket_close(pub);
    test_context_destroy(ctx);
}

#ifdef __linux__
static void test_pubsub_ipc()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *pub = test_socket_new(ctx, SLK_XPUB);
    int rc = slk_bind(pub, "ipc://pubsub_test.ipc");
    TEST_ASSERT_EQ(rc, 0);

    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub, "ipc://pubsub_test.ipc");
    TEST_ASSERT_EQ(rc, 0);
    slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);

    // Sync
    char sync_buf[32];
    int retries = 50;
    while (retries-- > 0) {
        if (slk_recv(pub, sync_buf, sizeof(sync_buf), SLK_DONTWAIT) > 0) break;
        test_sleep_ms(10);
    }

    slk_send(pub, "IPC", 3, 0);
    char buf[256];
    rc = slk_recv(sub, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 3);

    test_socket_close(sub);
    test_socket_close(pub);
    test_context_destroy(ctx);
}
#endif

int main()
{
    printf("=== ServerLink PUB-SUB Unit Tests ===\n");
    RUN_TEST(test_pubsub_inproc);
    RUN_TEST(test_pubsub_tcp);
#ifdef __linux__
    RUN_TEST(test_pubsub_ipc);
#endif
    printf("=== All PUB-SUB Tests Passed ===\n");
    return 0;
}

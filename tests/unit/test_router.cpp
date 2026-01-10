/* ServerLink ROUTER Socket Unit Tests (R-R Pattern) */
#include "../testutil.hpp"

static void test_router_inproc()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sb = test_socket_new(ctx, SLK_ROUTER);
    slk_setsockopt(sb, SLK_ROUTING_ID, "SRV", 3);
    int rc = slk_bind(sb, "inproc://router_test");
    TEST_ASSERT_EQ(rc, 0);

    slk_socket_t *sc = test_socket_new(ctx, SLK_ROUTER);
    slk_setsockopt(sc, SLK_ROUTING_ID, "CLI", 3);
    rc = slk_connect(sc, "inproc://router_test");
    TEST_ASSERT_EQ(rc, 0);

    test_sleep_ms(100);

    // Client sends to Server
    slk_send(sc, "SRV", 3, SLK_SNDMORE);
    slk_send(sc, "Hello", 5, 0);

    // Server receives: [ID][Data]
    char buf[256];
    rc = slk_recv(sb, buf, sizeof(buf), 0); // ID
    TEST_ASSERT_EQ(rc, 3);
    rc = slk_recv(sb, buf, sizeof(buf), 0); // Data
    TEST_ASSERT_EQ(rc, 5);

    test_socket_close(sc);
    test_socket_close(sb);
    test_context_destroy(ctx);
}

static void test_router_tcp()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sb = test_socket_new(ctx, SLK_ROUTER);
    slk_setsockopt(sb, SLK_ROUTING_ID, "SRV", 3);
    int rc = slk_bind(sb, "tcp://127.0.0.1:*");
    TEST_ASSERT_EQ(rc, 0);
    
    char endpoint[256]; size_t size = sizeof(endpoint);
    rc = slk_getsockopt(sb, SLK_LAST_ENDPOINT, endpoint, &size);
    TEST_ASSERT_EQ(rc, 0);

    slk_socket_t *sc = test_socket_new(ctx, SLK_ROUTER);
    slk_setsockopt(sc, SLK_ROUTING_ID, "CLI", 3);
    rc = slk_connect(sc, endpoint);
    TEST_ASSERT_EQ(rc, 0);

    test_sleep_ms(200);

    slk_send(sc, "SRV", 3, SLK_SNDMORE);
    slk_send(sc, "TCP", 3, 0);

    char buf[256];
    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 3);
    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 3);

    test_socket_close(sc);
    test_socket_close(sb);
    test_context_destroy(ctx);
}

#ifdef __linux__
static void test_router_ipc()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sb = test_socket_new(ctx, SLK_ROUTER);
    slk_setsockopt(sb, SLK_ROUTING_ID, "SRV", 3);
    int rc = slk_bind(sb, "ipc://router_test.ipc");
    TEST_ASSERT_EQ(rc, 0);

    slk_socket_t *sc = test_socket_new(ctx, SLK_ROUTER);
    slk_setsockopt(sc, SLK_ROUTING_ID, "CLI", 3);
    rc = slk_connect(sc, "ipc://router_test.ipc");
    TEST_ASSERT_EQ(rc, 0);

    test_sleep_ms(100);

    slk_send(sc, "SRV", 3, SLK_SNDMORE);
    slk_send(sc, "IPC", 3, 0);

    char buf[256];
    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 3);
    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 3);

    test_socket_close(sc);
    test_socket_close(sb);
    test_context_destroy(ctx);
}
#endif

int main()
{
    printf("=== ServerLink ROUTER Socket Unit Tests ===\n");
    RUN_TEST(test_router_inproc);
    RUN_TEST(test_router_tcp);
#ifdef __linux__
    RUN_TEST(test_router_ipc);
#endif
    printf("=== All ROUTER Tests Passed ===\n");
    return 0;
}

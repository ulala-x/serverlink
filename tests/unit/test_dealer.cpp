/* ServerLink DEALER Socket Unit Tests (Threaded, Shared Context) */
#include "../testutil.hpp"
#include <thread>

static void server_task(slk_ctx_t *ctx, const char *addr) {
    slk_socket_t *server = test_socket_new(ctx, SLK_DEALER);
    slk_bind(server, addr);

    char buf[256];
    int rc = slk_recv(server, buf, sizeof(buf), 0);
    if (rc > 0) {
        buf[rc] = '\0';
        if (strcmp(buf, "Q") == 0) {
            slk_send(server, "A", 1, 0);
        }
    }
    test_socket_close(server);
}

static void test_dealer_inproc()
{
    printf("[test_dealer_inproc] Starting...\n");
    slk_ctx_t *ctx = test_context_new();
    std::thread t(server_task, ctx, "inproc://dealer_test");
    test_sleep_ms(100);

    slk_socket_t *client = test_socket_new(ctx, SLK_DEALER);
    slk_connect(client, "inproc://dealer_test");

    slk_send(client, "Q", 1, 0);
    char buf[256];
    int rc = slk_recv(client, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 1);
    TEST_ASSERT_EQ(buf[0], 'A');

    test_socket_close(client);
    t.join();
    test_context_destroy(ctx);
    printf("[test_dealer_inproc] Passed\n");
}

static void test_dealer_tcp()
{
    printf("[test_dealer_tcp] Starting...\n");
    slk_ctx_t *ctx = test_context_new();
    
    srand(time(NULL));
    int port = 41000 + (rand() % 10000);
    char addr[64];
    sprintf(addr, "tcp://127.0.0.1:%d", port);

    std::thread t(server_task, ctx, addr);
    test_sleep_ms(200);

    slk_socket_t *client = test_socket_new(ctx, SLK_DEALER);
    slk_connect(client, addr);

    slk_send(client, "Q", 1, 0);
    char buf[256];
    int rc = slk_recv(client, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 1);
    TEST_ASSERT_EQ(buf[0], 'A');

    test_socket_close(client);
    t.join();
    test_context_destroy(ctx);
    printf("[test_dealer_tcp] Passed\n");
}

int main()
{
    printf("=== ServerLink DEALER Socket Unit Tests ===\n");
    test_dealer_inproc();
    test_dealer_tcp();
    printf("=== All DEALER Tests Passed ===\n");
    return 0;
}

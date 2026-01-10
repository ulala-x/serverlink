/* ServerLink PAIR Socket Unit Tests (Threaded, Shared Context) */
#include "../testutil.hpp"
#include <thread>

// Worker thread for server (receives shared context) 
static void server_task(slk_ctx_t *ctx, const char *addr) {
    // Create socket from SHARED context
    slk_socket_t *server = test_socket_new(ctx, SLK_PAIR);
    int rc = slk_bind(server, addr);
    if (rc != 0) {
        test_socket_close(server);
        return;
    }

    char buf[256];
    rc = slk_recv(server, buf, sizeof(buf), 0);
    if (rc > 0) {
        buf[rc] = '\0';
        if (strcmp(buf, "Hello") == 0) {
            slk_send(server, "World", 5, 0);
        }
    }
    test_socket_close(server);
}

static void test_pair_inproc()
{
    printf("[test_pair_inproc] Starting...\n");
    slk_ctx_t *ctx = test_context_new(); // Shared Context
    
    std::thread t(server_task, ctx, "inproc://pair_test");
    test_sleep_ms(100);

    slk_socket_t *client = test_socket_new(ctx, SLK_PAIR);
    int rc = slk_connect(client, "inproc://pair_test");
    TEST_ASSERT_EQ(rc, 0);

    slk_send(client, "Hello", 5, 0);
    
    char buf[256];
    rc = slk_recv(client, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 5);
    buf[rc] = '\0';
    TEST_ASSERT_STR_EQ(buf, "World");

    test_socket_close(client);
    t.join();
    test_context_destroy(ctx); // Destroy after all threads are done
    printf("[test_pair_inproc] Passed\n");
}

static void test_pair_tcp()
{
    printf("[test_pair_tcp] Starting...\n");
    slk_ctx_t *ctx = test_context_new(); // Shared Context works for TCP too

    srand(time(NULL));
    int port = 40000 + (rand() % 10000);
    char addr[64];
    sprintf(addr, "tcp://127.0.0.1:%d", port);

    std::thread t(server_task, ctx, addr);
    test_sleep_ms(200);

    slk_socket_t *client = test_socket_new(ctx, SLK_PAIR);
    int rc = slk_connect(client, addr);
    TEST_ASSERT_EQ(rc, 0);

    slk_send(client, "Hello", 5, 0);
    
    char buf[256];
    rc = slk_recv(client, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 5);
    buf[rc] = '\0';
    TEST_ASSERT_STR_EQ(buf, "World");

    test_socket_close(client);
    t.join();
    test_context_destroy(ctx);
    printf("[test_pair_tcp] Passed\n");
}

int main()
{
    printf("=== ServerLink PAIR Socket Unit Tests ===\n");
    test_pair_inproc();
    test_pair_tcp();
    printf("=== All PAIR Tests Passed ===\n");
    return 0;
}
/* ServerLink DEALER-ROUTER Socket Unit Tests (Threaded, Shared Context) */
#include "../testutil.hpp"
#include <thread>

static void server_task(slk_ctx_t *ctx, const char *addr) {
    slk_socket_t *router = test_socket_new(ctx, SLK_ROUTER);
    slk_setsockopt(router, SLK_ROUTING_ID, "SRV", 3);
    slk_bind(router, addr);

    char id[256]; char buf[256];
    int id_len = slk_recv(router, id, sizeof(id), 0); // ID
    int msg_len = slk_recv(router, buf, sizeof(buf), 0); // Data

    if (id_len > 0 && msg_len > 0) {
        slk_send(router, id, id_len, SLK_SNDMORE);
        slk_send(router, "World", 5, 0);
    }

    test_socket_close(router);
}

static void test_dr_inproc()
{
    printf("[test_dr_inproc] Starting...\n");
    slk_ctx_t *ctx = test_context_new();
    std::thread t(server_task, ctx, "inproc://dr_test");
    test_sleep_ms(100);

    slk_socket_t *dealer = test_socket_new(ctx, SLK_DEALER);
    slk_setsockopt(dealer, SLK_ROUTING_ID, "CLI", 3);
    slk_connect(dealer, "inproc://dr_test");

    slk_send(dealer, "Hello", 5, 0);
    
    char buf[256];
    int rc = slk_recv(dealer, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 5);
    buf[rc] = '\0';
    TEST_ASSERT_STR_EQ(buf, "World");

    test_socket_close(dealer);
    t.join();
    test_context_destroy(ctx);
    printf("[test_dr_inproc] Passed\n");
}

static void test_dr_tcp()
{
    printf("[test_dr_tcp] Starting...\n");
    slk_ctx_t *ctx = test_context_new();
    
    srand(time(NULL));
    int port = 42000 + (rand() % 10000);
    char addr[64];
    sprintf(addr, "tcp://127.0.0.1:%d", port);

    std::thread t(server_task, ctx, addr);
    test_sleep_ms(200);

    slk_socket_t *dealer = test_socket_new(ctx, SLK_DEALER);
    slk_setsockopt(dealer, SLK_ROUTING_ID, "CLI", 3);
    slk_connect(dealer, addr);

    slk_send(dealer, "Hello", 5, 0);
    
    char buf[256];
    int rc = slk_recv(dealer, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 5);
    buf[rc] = '\0';
    TEST_ASSERT_STR_EQ(buf, "World");

    test_socket_close(dealer);
    t.join();
    test_context_destroy(ctx);
    printf("[test_dr_tcp] Passed\n");
}

int main()
{
    printf("=== ServerLink DEALER-ROUTER Unit Tests ===\n");
    test_dr_inproc();
    test_dr_tcp();
    printf("=== All DEALER-ROUTER Tests Passed ===\n");
    return 0;
}

/* SPDX-License-Identifier: MPL-2.0 */

#include "../testutil.hpp"

// Test Case 1: Dealer load balancing
// 1 DEALER -> 3 ROUTERS
void test_dealer_load_balancing_standard() {
    printf("  Running test_dealer_load_balancing_standard\n");
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *dealer = test_socket_new(ctx, SLK_DEALER);
    slk_socket_t *routers[3];
    const char *endpoints[3] = {"inproc://router1", "inproc://router2", "inproc://router3"};

    for (int i = 0; i < 3; i++) {
        routers[i] = test_socket_new(ctx, SLK_ROUTER);
        test_socket_bind(routers[i], endpoints[i]);
        test_socket_connect(dealer, endpoints[i]);
    }

    test_sleep_ms(100);

    // Send 6 messages. Each router should get exactly 2 messages.
    for (int i = 0; i < 6; i++) {
        char buf[10];
        sprintf(buf, "msg%d", i);
        test_send_string(dealer, buf, 0);
    }

    test_sleep_ms(100);

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            char id[256], data[256];
            int rc = slk_recv(routers[i], id, sizeof(id), 0);
            TEST_ASSERT(rc > 0);
            rc = slk_recv(routers[i], data, sizeof(data), 0);
            TEST_ASSERT(rc > 0);
        }
        // No more messages should be in this router
        slk_pollitem_t item = {routers[i], 0, SLK_POLLIN, 0};
        int rc = slk_poll(&item, 1, 10);
        TEST_ASSERT_EQ(rc, 0);
    }

    slk_close(dealer);
    for (int i = 0; i < 3; i++) slk_close(routers[i]);
    test_context_destroy(ctx);
}

// Test Case 2: Dealer fair queueing
// 3 ROUTERS -> 1 DEALER
void test_dealer_fair_queueing_standard() {
    printf("  Running test_dealer_fair_queueing_standard\n");
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *dealer = test_socket_new(ctx, SLK_DEALER);
    test_set_routing_id(dealer, "DEALER");

    slk_socket_t *routers[3];
    const char *endpoints[3] = {"inproc://fq1", "inproc://fq2", "inproc://fq3"};

    for (int i = 0; i < 3; i++) {
        routers[i] = test_socket_new(ctx, SLK_ROUTER);
        test_socket_bind(routers[i], endpoints[i]);
        test_socket_connect(dealer, endpoints[i]);
    }

    test_sleep_ms(100);

    // Each router sends 2 messages to the dealer
    for (int i = 0; i < 3; i++) {
        s_send_seq_2(routers[i], "DEALER", "A");
        s_send_seq_2(routers[i], "DEALER", "B");
    }

    test_sleep_ms(100);

    // Dealer should receive 6 messages in total
    int count = 0;
    while (test_poll_readable(dealer, 100)) {
        char buf[256];
        if (slk_recv(dealer, buf, sizeof(buf), 0) >= 0) {
            count++;
        }
    }
    TEST_ASSERT_EQ(count, 6);

    slk_close(dealer);
    for (int i = 0; i < 3; i++) slk_close(routers[i]);
    test_context_destroy(ctx);
}

int main() {
    RUN_TEST(test_dealer_load_balancing_standard);
    RUN_TEST(test_dealer_fair_queueing_standard);
    return 0;
}

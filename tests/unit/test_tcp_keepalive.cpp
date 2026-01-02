/* ServerLink TCP Keepalive Option Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/*
 * TCP Keepalive Option Tests
 *
 * Tests the TCP keepalive socket options:
 * - SLK_TCP_KEEPALIVE: Enable/disable TCP keepalive (0/1/-1)
 * - SLK_TCP_KEEPALIVE_IDLE: TCP keepalive idle time (seconds)
 * - SLK_TCP_KEEPALIVE_INTVL: TCP keepalive interval (seconds)
 * - SLK_TCP_KEEPALIVE_CNT: TCP keepalive probe count
 *
 * These options are applied to TCP sockets during connection establishment.
 */

/* Test 1: SLK_TCP_KEEPALIVE enable/disable */
static void test_tcp_keepalive_option()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Default value should be -1 (system default) */
    int keepalive = -999;
    size_t optlen = sizeof(keepalive);
    int rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE, &keepalive, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(optlen, sizeof(int));
    TEST_ASSERT_EQ(keepalive, -1);

    /* Enable TCP keepalive */
    keepalive = 1;
    rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE, &keepalive, sizeof(keepalive));
    TEST_SUCCESS(rc);

    /* Verify enabled */
    keepalive = -999;
    optlen = sizeof(keepalive);
    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE, &keepalive, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(keepalive, 1);

    /* Disable TCP keepalive */
    keepalive = 0;
    rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE, &keepalive, sizeof(keepalive));
    TEST_SUCCESS(rc);

    keepalive = -999;
    optlen = sizeof(keepalive);
    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE, &keepalive, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(keepalive, 0);

    /* Set back to system default */
    keepalive = -1;
    rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE, &keepalive, sizeof(keepalive));
    TEST_SUCCESS(rc);

    keepalive = -999;
    optlen = sizeof(keepalive);
    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE, &keepalive, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(keepalive, -1);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 2: SLK_TCP_KEEPALIVE_IDLE option */
static void test_tcp_keepalive_idle()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Default value should be -1 (system default) */
    int idle = -999;
    size_t optlen = sizeof(idle);
    int rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE_IDLE, &idle, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(optlen, sizeof(int));
    TEST_ASSERT_EQ(idle, -1);

    /* Set idle time to 60 seconds */
    idle = 60;
    rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE_IDLE, &idle, sizeof(idle));
    TEST_SUCCESS(rc);

    idle = -999;
    optlen = sizeof(idle);
    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE_IDLE, &idle, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(idle, 60);

    /* Set to 300 seconds (5 minutes) */
    idle = 300;
    rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE_IDLE, &idle, sizeof(idle));
    TEST_SUCCESS(rc);

    idle = -999;
    optlen = sizeof(idle);
    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE_IDLE, &idle, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(idle, 300);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 3: SLK_TCP_KEEPALIVE_INTVL option */
static void test_tcp_keepalive_intvl()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Default value should be -1 (system default) */
    int intvl = -999;
    size_t optlen = sizeof(intvl);
    int rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE_INTVL, &intvl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(optlen, sizeof(int));
    TEST_ASSERT_EQ(intvl, -1);

    /* Set interval to 10 seconds */
    intvl = 10;
    rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE_INTVL, &intvl, sizeof(intvl));
    TEST_SUCCESS(rc);

    intvl = -999;
    optlen = sizeof(intvl);
    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE_INTVL, &intvl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(intvl, 10);

    /* Set to 30 seconds */
    intvl = 30;
    rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE_INTVL, &intvl, sizeof(intvl));
    TEST_SUCCESS(rc);

    intvl = -999;
    optlen = sizeof(intvl);
    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE_INTVL, &intvl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(intvl, 30);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 4: SLK_TCP_KEEPALIVE_CNT option */
static void test_tcp_keepalive_cnt()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Default value should be -1 (system default) */
    int cnt = -999;
    size_t optlen = sizeof(cnt);
    int rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE_CNT, &cnt, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(optlen, sizeof(int));
    TEST_ASSERT_EQ(cnt, -1);

    /* Set count to 5 probes */
    cnt = 5;
    rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE_CNT, &cnt, sizeof(cnt));
    TEST_SUCCESS(rc);

    cnt = -999;
    optlen = sizeof(cnt);
    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE_CNT, &cnt, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(cnt, 5);

    /* Set to 10 probes */
    cnt = 10;
    rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE_CNT, &cnt, sizeof(cnt));
    TEST_SUCCESS(rc);

    cnt = -999;
    optlen = sizeof(cnt);
    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE_CNT, &cnt, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(cnt, 10);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 5: All TCP keepalive options together */
static void test_tcp_keepalive_all_options()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Configure full TCP keepalive settings */
    int keepalive = 1;
    int idle = 120;
    int intvl = 15;
    int cnt = 8;

    int rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE, &keepalive, sizeof(keepalive));
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE_IDLE, &idle, sizeof(idle));
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE_INTVL, &intvl, sizeof(intvl));
    TEST_SUCCESS(rc);
    rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE_CNT, &cnt, sizeof(cnt));
    TEST_SUCCESS(rc);

    /* Verify all options */
    keepalive = -999;
    idle = -999;
    intvl = -999;
    cnt = -999;
    size_t optlen = sizeof(int);

    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE, &keepalive, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(keepalive, 1);

    optlen = sizeof(int);
    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE_IDLE, &idle, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(idle, 120);

    optlen = sizeof(int);
    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE_INTVL, &intvl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(intvl, 15);

    optlen = sizeof(int);
    rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE_CNT, &cnt, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(cnt, 8);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 6: TCP keepalive with different socket types */
static void test_tcp_keepalive_different_sockets()
{
    slk_ctx_t *ctx = test_context_new();
    int socket_types[] = {SLK_ROUTER, SLK_PUB, SLK_SUB, SLK_PAIR};
    const char *socket_names[] = {"ROUTER", "PUB", "SUB", "PAIR"};
    int num_types = sizeof(socket_types) / sizeof(socket_types[0]);

    for (int i = 0; i < num_types; i++) {
        slk_socket_t *sock = test_socket_new(ctx, socket_types[i]);

        /* Set TCP keepalive */
        int keepalive = 1;
        int rc = slk_setsockopt(sock, SLK_TCP_KEEPALIVE, &keepalive, sizeof(keepalive));
        if (rc != 0) {
            printf("  NOTE: %s socket may not support TCP keepalive\n",
                   socket_names[i]);
            test_socket_close(sock);
            continue;
        }

        /* Verify */
        keepalive = -999;
        size_t optlen = sizeof(keepalive);
        rc = slk_getsockopt(sock, SLK_TCP_KEEPALIVE, &keepalive, &optlen);
        TEST_SUCCESS(rc);
        TEST_ASSERT_EQ(keepalive, 1);

        test_socket_close(sock);
    }

    test_context_destroy(ctx);
}

/* Test 7: TCP keepalive options applied to connection */
static void test_tcp_keepalive_on_connection()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    /* Server with TCP keepalive enabled */
    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);

    int keepalive = 1;
    int rc = slk_setsockopt(server, SLK_TCP_KEEPALIVE, &keepalive, sizeof(keepalive));
    TEST_SUCCESS(rc);

    int idle = 30;
    rc = slk_setsockopt(server, SLK_TCP_KEEPALIVE_IDLE, &idle, sizeof(idle));
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(server, SLK_ROUTING_ID, "server", 6);
    TEST_SUCCESS(rc);

    test_socket_bind(server, endpoint);

    /* Client with different TCP keepalive settings */
    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);

    keepalive = 1;
    rc = slk_setsockopt(client, SLK_TCP_KEEPALIVE, &keepalive, sizeof(keepalive));
    TEST_SUCCESS(rc);

    idle = 60;
    rc = slk_setsockopt(client, SLK_TCP_KEEPALIVE_IDLE, &idle, sizeof(idle));
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(client, SLK_ROUTING_ID, "client", 6);
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(client, SLK_CONNECT_ROUTING_ID, "server", 6);
    TEST_SUCCESS(rc);

    test_socket_connect(client, endpoint);

    /* Allow connection to establish */
    test_sleep_ms(100);

    /* Verify options are still correct after connection */
    keepalive = -999;
    size_t optlen = sizeof(keepalive);
    rc = slk_getsockopt(client, SLK_TCP_KEEPALIVE, &keepalive, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(keepalive, 1);

    idle = -999;
    optlen = sizeof(idle);
    rc = slk_getsockopt(client, SLK_TCP_KEEPALIVE_IDLE, &idle, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(idle, 60);

    /* Test that connection works */
    rc = slk_send(client, "server", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(client, "test", 4, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    char buf[256];
    rc = slk_recv(server, buf, sizeof(buf), 0);  /* routing ID */
    TEST_ASSERT(rc > 0);
    rc = slk_recv(server, buf, sizeof(buf), 0);  /* payload */
    TEST_ASSERT_EQ(rc, 4);
    TEST_ASSERT_MEM_EQ(buf, "test", 4);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Main test runner */
int main()
{
    printf("=== ServerLink TCP Keepalive Tests ===\n\n");

    RUN_TEST(test_tcp_keepalive_option);
    RUN_TEST(test_tcp_keepalive_idle);
    RUN_TEST(test_tcp_keepalive_intvl);
    RUN_TEST(test_tcp_keepalive_cnt);
    RUN_TEST(test_tcp_keepalive_all_options);
    RUN_TEST(test_tcp_keepalive_different_sockets);
    RUN_TEST(test_tcp_keepalive_on_connection);

    printf("\n=== TCP Keepalive Tests Completed ===\n");
    printf("NOTE: TCP keepalive options are applied at the OS level.\n");
    printf("      Actual keepalive behavior depends on OS support.\n");
    return 0;
}

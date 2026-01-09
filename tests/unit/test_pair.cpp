/* ServerLink PAIR Socket Unit Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/* Test: Basic PAIR socket creation */
static void test_pair_create()
{
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *pair = test_socket_new(ctx, SLK_PAIR);
    TEST_ASSERT_NOT_NULL(pair);

    test_socket_close(pair);
    test_context_destroy(ctx);
}

/* Test: PAIR socket inproc communication */
static void test_pair_inproc()
{
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *server = test_socket_new(ctx, SLK_PAIR);
    test_socket_bind(server, "inproc://pair_test");

    slk_socket_t *client = test_socket_new(ctx, SLK_PAIR);
    test_socket_connect(client, "inproc://pair_test");

    // Give sockets time to connect
    test_sleep_ms(100);

    // Test send/recv
    const char *msg = "Hello PAIR";
    int rc = slk_send(server, msg, strlen(msg), 0);
    TEST_ASSERT_EQ(rc, (int)strlen(msg));

    char buf[256];
    rc = slk_recv(client, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, (int)strlen(msg));
    buf[rc] = '\0';
    TEST_ASSERT_STR_EQ(buf, msg);

    // Test bidirectional communication
    const char *reply = "Reply from client";
    rc = slk_send(client, reply, strlen(reply), 0);
    TEST_ASSERT_EQ(rc, (int)strlen(reply));

    rc = slk_recv(server, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, (int)strlen(reply));
    buf[rc] = '\0';
    TEST_ASSERT_STR_EQ(buf, reply);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: PAIR socket TCP communication */
static void test_pair_tcp()
{
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *server = test_socket_new(ctx, SLK_PAIR);
    // Use ephemeral port
    test_socket_bind(server, "tcp://127.0.0.1:*");

    char endpoint[256];
    size_t size = sizeof(endpoint);
    TEST_SUCCESS(slk_getsockopt(server, SLK_LAST_ENDPOINT, endpoint, &size));

    slk_socket_t *client = test_socket_new(ctx, SLK_PAIR);
    test_socket_connect(client, endpoint);

    test_sleep_ms(200);

    const char *msg = "TCP message";
    int rc = slk_send(client, msg, strlen(msg), 0);
    TEST_ASSERT_EQ(rc, (int)strlen(msg));

    char buf[256];
    rc = slk_recv(server, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, (int)strlen(msg));
    buf[rc] = '\0';
    TEST_ASSERT_STR_EQ(buf, msg);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Test: PAIR socket IPC communication */
static void test_pair_ipc()
{
    if (!slk_has("ipc")) {
        printf("  Skipping IPC test (not supported)\n");
        return;
    }

    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *server = test_socket_new(ctx, SLK_PAIR);
    const char* endpoint = test_endpoint_ipc();
    test_socket_bind(server, endpoint);

    slk_socket_t *client = test_socket_new(ctx, SLK_PAIR);
    test_socket_connect(client, endpoint);

    test_sleep_ms(200);

    const char *msg = "IPC message";
    int rc = slk_send(server, msg, strlen(msg), 0);
    TEST_ASSERT_EQ(rc, (int)strlen(msg));

    char buf[256];
    rc = slk_recv(client, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, (int)strlen(msg));
    buf[rc] = '\0';
    TEST_ASSERT_STR_EQ(buf, msg);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink PAIR Socket Protocol Tests ===\n\n");

    RUN_TEST(test_pair_create);
    RUN_TEST(test_pair_inproc);
    RUN_TEST(test_pair_tcp);
    RUN_TEST(test_pair_ipc);

    printf("\n=== All PAIR Protocol Tests Passed ===\n");
    return 0;
}
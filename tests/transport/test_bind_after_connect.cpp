/* ServerLink Bind After Connect Tests - Ported from libzmq */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/*
 * Test: Connect before bind with TCP
 *
 * Note: Original libzmq test used DEALER sockets. ServerLink only supports
 * ROUTER, so we adapt the test accordingly.
 */
static void test_bind_after_connect_tcp()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    /* Create connect socket first */
    slk_socket_t *sc = test_socket_new(ctx, SLK_ROUTER);
    int rc = slk_setsockopt(sc, SLK_ROUTING_ID, "client", 6);
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(sc, SLK_CONNECT_ROUTING_ID, "server", 6);
    TEST_SUCCESS(rc);

    test_socket_connect(sc, endpoint);

    test_sleep_ms(100);

    /* Now bind */
    slk_socket_t *sb = test_socket_new(ctx, SLK_ROUTER);
    rc = slk_setsockopt(sb, SLK_ROUTING_ID, "server", 6);
    TEST_SUCCESS(rc);

    test_socket_bind(sb, endpoint);

    test_sleep_ms(200);

    /* ROUTER-to-ROUTER handshake: client to server */
    rc = slk_send(sc, "server", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(sc, "HELLO", 5, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* Server receives handshake */
    char buf[256];
    rc = slk_recv(sb, buf, sizeof(buf), 0);  /* routing ID */
    TEST_ASSERT(rc > 0);
    int client_rid_len = rc;
    char client_rid[256];
    memcpy(client_rid, buf, client_rid_len);

    rc = slk_recv(sb, buf, sizeof(buf), 0);  /* "HELLO" */
    TEST_ASSERT(rc > 0);

    /* Server responds */
    rc = slk_send(sb, client_rid, client_rid_len, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(sb, "READY", 5, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* Client receives response */
    rc = slk_recv(sc, buf, sizeof(buf), 0);  /* routing ID "server" */
    TEST_ASSERT(rc > 0);
    rc = slk_recv(sc, buf, sizeof(buf), 0);  /* "READY" */
    TEST_ASSERT(rc > 0);

    /* Now send actual test data */
    rc = slk_send(sc, "server", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(sc, "foobar", 6, 0);
    TEST_ASSERT(rc >= 0);

    rc = slk_send(sc, "server", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(sc, "baz", 3, 0);
    TEST_ASSERT(rc >= 0);

    rc = slk_send(sc, "server", 6, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);
    rc = slk_send(sc, "buzz", 4, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(100);

    /* Receive the messages */
    /* Each message is preceded by routing ID */
    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT(rc > 0);  /* routing ID */

    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 6);
    TEST_ASSERT_MEM_EQ(buf, "foobar", 6);

    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT(rc > 0);  /* routing ID */

    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 3);
    TEST_ASSERT_MEM_EQ(buf, "baz", 3);

    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT(rc > 0);  /* routing ID */

    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 4);
    TEST_ASSERT_MEM_EQ(buf, "buzz", 4);

    test_socket_close(sc);
    test_socket_close(sb);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink Bind After Connect Tests ===\n\n");

    printf("Note: Testing TCP endpoint with connect-before-bind pattern\n\n");

    RUN_TEST(test_bind_after_connect_tcp);

    printf("\n=== All Bind After Connect Tests Passed ===\n");
    return 0;
}

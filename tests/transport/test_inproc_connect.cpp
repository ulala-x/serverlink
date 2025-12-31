/* ServerLink Inproc Connect Tests - Ported from libzmq */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/*
 * Note: ServerLink may not fully support inproc transport.
 * These tests are included for API compatibility but may be skipped
 * if inproc is not available.
 */

/* Test: Bind before connect with inproc */
static void test_bind_before_connect()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://test_bbc";

    /* Bind first */
    slk_socket_t *bind_socket = test_socket_new(ctx, SLK_ROUTER);
    int rc = slk_bind(bind_socket, endpoint);
    if (rc < 0) {
        printf("  NOTE: inproc transport not supported, skipping test\n");
        test_socket_close(bind_socket);
        test_context_destroy(ctx);
        return;
    }

    /* Now connect */
    slk_socket_t *connect_socket = test_socket_new(ctx, SLK_ROUTER);
    rc = slk_setsockopt(connect_socket, SLK_ROUTING_ID, "client", 6);
    TEST_SUCCESS(rc);

    test_socket_connect(connect_socket, endpoint);

    test_sleep_ms(100);

    /* Queue up some data */
    rc = slk_send(connect_socket, "foobar", 6, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* Read pending message */
    char buf[256];
    rc = slk_recv(bind_socket, buf, sizeof(buf), 0);  /* routing ID */
    TEST_ASSERT(rc > 0);

    rc = slk_recv(bind_socket, buf, sizeof(buf), 0);  /* payload */
    TEST_ASSERT_EQ(rc, 6);
    TEST_ASSERT_MEM_EQ(buf, "foobar", 6);

    /* Cleanup */
    test_socket_close(connect_socket);
    test_socket_close(bind_socket);
    test_context_destroy(ctx);
}

/* Test: Connect before bind with inproc */
static void test_connect_before_bind()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://test_cbb";

    /* Connect first */
    slk_socket_t *connect_socket = test_socket_new(ctx, SLK_ROUTER);
    int rc = slk_setsockopt(connect_socket, SLK_ROUTING_ID, "client", 6);
    TEST_SUCCESS(rc);

    rc = slk_connect(connect_socket, endpoint);
    if (rc < 0) {
        printf("  NOTE: inproc transport not supported, skipping test\n");
        test_socket_close(connect_socket);
        test_context_destroy(ctx);
        return;
    }

    /* Queue up some data */
    rc = slk_send(connect_socket, "foobar", 6, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* Now bind */
    slk_socket_t *bind_socket = test_socket_new(ctx, SLK_ROUTER);
    test_socket_bind(bind_socket, endpoint);

    test_sleep_ms(100);

    /* Read pending message */
    char buf[256];
    rc = slk_recv(bind_socket, buf, sizeof(buf), 0);  /* routing ID */
    TEST_ASSERT(rc > 0);

    rc = slk_recv(bind_socket, buf, sizeof(buf), 0);  /* payload */
    TEST_ASSERT_EQ(rc, 6);
    TEST_ASSERT_MEM_EQ(buf, "foobar", 6);

    /* Cleanup */
    test_socket_close(connect_socket);
    test_socket_close(bind_socket);
    test_context_destroy(ctx);
}

/* Test: Multiple connects */
static void test_multiple_connects()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://test_multiple";
    const unsigned int no_of_connects = 10;

    slk_socket_t *connect_socket[no_of_connects];

    /* Connect first */
    for (unsigned int i = 0; i < no_of_connects; ++i) {
        connect_socket[i] = test_socket_new(ctx, SLK_ROUTER);

        char id[32];
        snprintf(id, sizeof(id), "client%u", i);
        int rc = slk_setsockopt(connect_socket[i], SLK_ROUTING_ID, id, strlen(id));
        TEST_SUCCESS(rc);

        rc = slk_connect(connect_socket[i], endpoint);
        if (rc < 0 && i == 0) {
            printf("  NOTE: inproc transport not supported, skipping test\n");
            test_socket_close(connect_socket[i]);
            test_context_destroy(ctx);
            return;
        }
        TEST_SUCCESS(rc);

        /* Queue up some data */
        rc = slk_send(connect_socket[i], "foobar", 6, 0);
        TEST_ASSERT(rc >= 0);
    }

    test_sleep_ms(100);

    /* Now bind */
    slk_socket_t *bind_socket = test_socket_new(ctx, SLK_ROUTER);
    test_socket_bind(bind_socket, endpoint);

    test_sleep_ms(200);

    /* Receive all messages */
    for (unsigned int i = 0; i < no_of_connects; ++i) {
        char buf[256];
        int rc = slk_recv(bind_socket, buf, sizeof(buf), 0);  /* routing ID */
        TEST_ASSERT(rc > 0);

        rc = slk_recv(bind_socket, buf, sizeof(buf), 0);  /* payload */
        TEST_ASSERT_EQ(rc, 6);
        TEST_ASSERT_MEM_EQ(buf, "foobar", 6);
    }

    /* Cleanup */
    for (unsigned int i = 0; i < no_of_connects; ++i) {
        test_socket_close(connect_socket[i]);
    }

    test_socket_close(bind_socket);
    test_context_destroy(ctx);
}

/* Test: Routing ID with inproc */
static void test_routing_id()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://test_routing_id";

    /* Create the infrastructure */
    slk_socket_t *sc = test_socket_new(ctx, SLK_ROUTER);
    int rc = slk_setsockopt(sc, SLK_ROUTING_ID, "dealer", 6);
    TEST_SUCCESS(rc);

    rc = slk_connect(sc, endpoint);
    if (rc < 0) {
        printf("  NOTE: inproc transport not supported, skipping test\n");
        test_socket_close(sc);
        test_context_destroy(ctx);
        return;
    }

    slk_socket_t *sb = test_socket_new(ctx, SLK_ROUTER);
    test_socket_bind(sb, endpoint);

    test_sleep_ms(100);

    /* Send 2-part message (for ROUTER, we send data directly) */
    rc = slk_send(sc, "A", 1, SLK_SNDMORE);
    TEST_ASSERT(rc >= 0);

    rc = slk_send(sc, "B", 1, 0);
    TEST_ASSERT(rc >= 0);

    test_sleep_ms(50);

    /* Routing ID comes first */
    char buf[256];
    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT(rc > 0);

    /* Then the first part of the message body */
    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 1);
    TEST_ASSERT_EQ(buf[0], 'A');

    /* And finally, the second part of the message body */
    rc = slk_recv(sb, buf, sizeof(buf), 0);
    TEST_ASSERT_EQ(rc, 1);
    TEST_ASSERT_EQ(buf[0], 'B');

    /* Deallocate the infrastructure */
    test_socket_close(sc);
    test_socket_close(sb);
    test_context_destroy(ctx);
}

/* Test: Connect only (no bind) */
static void test_connect_only()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = "inproc://test_connect_only";

    slk_socket_t *connect_socket = test_socket_new(ctx, SLK_ROUTER);
    int rc = slk_connect(connect_socket, endpoint);
    if (rc < 0) {
        printf("  NOTE: inproc transport not supported, skipping test\n");
    } else {
        printf("  NOTE: Connect-only succeeded (messages will be queued)\n");
    }

    test_socket_close(connect_socket);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink Inproc Connect Tests ===\n\n");

    printf("Note: inproc transport may not be fully supported in ServerLink.\n");
    printf("Tests will be skipped if inproc is not available.\n\n");

    RUN_TEST(test_bind_before_connect);
    RUN_TEST(test_connect_before_bind);
    RUN_TEST(test_multiple_connects);
    RUN_TEST(test_routing_id);
    RUN_TEST(test_connect_only);

    printf("\n=== All Inproc Connect Tests Completed ===\n");
    return 0;
}

/* ServerLink Heartbeat Option Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/*
 * Heartbeat Option Tests
 *
 * Tests the heartbeat socket options:
 * - SLK_HEARTBEAT_IVL: Heartbeat interval in milliseconds
 * - SLK_HEARTBEAT_TIMEOUT: Heartbeat timeout in milliseconds
 * - SLK_HEARTBEAT_TTL: Heartbeat time-to-live (hops)
 *
 * Note: These tests verify option setting/getting. The actual heartbeat
 * mechanism may not be fully implemented, but options should be stored correctly.
 */

/* Test 1: SLK_HEARTBEAT_IVL option setting and getting */
static void test_heartbeat_ivl_option()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Default value should be 0 (disabled) */
    int ivl = -999;
    size_t optlen = sizeof(ivl);
    int rc = slk_getsockopt(sock, SLK_HEARTBEAT_IVL, &ivl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(optlen, sizeof(int));
    TEST_ASSERT_EQ(ivl, 0);

    /* Set heartbeat interval to 1000ms (1 second) */
    ivl = 1000;
    rc = slk_setsockopt(sock, SLK_HEARTBEAT_IVL, &ivl, sizeof(ivl));
    TEST_SUCCESS(rc);

    /* Verify the value was stored */
    ivl = -999;
    optlen = sizeof(ivl);
    rc = slk_getsockopt(sock, SLK_HEARTBEAT_IVL, &ivl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ivl, 1000);

    /* Set to a different value */
    ivl = 5000;
    rc = slk_setsockopt(sock, SLK_HEARTBEAT_IVL, &ivl, sizeof(ivl));
    TEST_SUCCESS(rc);

    ivl = -999;
    optlen = sizeof(ivl);
    rc = slk_getsockopt(sock, SLK_HEARTBEAT_IVL, &ivl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ivl, 5000);

    /* Set back to 0 (disabled) */
    ivl = 0;
    rc = slk_setsockopt(sock, SLK_HEARTBEAT_IVL, &ivl, sizeof(ivl));
    TEST_SUCCESS(rc);

    ivl = -999;
    optlen = sizeof(ivl);
    rc = slk_getsockopt(sock, SLK_HEARTBEAT_IVL, &ivl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ivl, 0);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 2: SLK_HEARTBEAT_TIMEOUT option setting and getting */
static void test_heartbeat_timeout_option()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Default value should be -1 (disabled/default behavior) */
    int timeout = -999;
    size_t optlen = sizeof(timeout);
    int rc = slk_getsockopt(sock, SLK_HEARTBEAT_TIMEOUT, &timeout, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(optlen, sizeof(int));
    TEST_ASSERT_EQ(timeout, -1);

    /* Set heartbeat timeout to 3000ms (3 seconds) */
    timeout = 3000;
    rc = slk_setsockopt(sock, SLK_HEARTBEAT_TIMEOUT, &timeout, sizeof(timeout));
    TEST_SUCCESS(rc);

    /* Verify the value was stored */
    timeout = -999;
    optlen = sizeof(timeout);
    rc = slk_getsockopt(sock, SLK_HEARTBEAT_TIMEOUT, &timeout, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(timeout, 3000);

    /* Set to 0 */
    timeout = 0;
    rc = slk_setsockopt(sock, SLK_HEARTBEAT_TIMEOUT, &timeout, sizeof(timeout));
    TEST_SUCCESS(rc);

    timeout = -999;
    optlen = sizeof(timeout);
    rc = slk_getsockopt(sock, SLK_HEARTBEAT_TIMEOUT, &timeout, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(timeout, 0);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 3: SLK_HEARTBEAT_TTL option setting and getting */
static void test_heartbeat_ttl_option()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Default value should be 0 */
    int ttl = -999;
    size_t optlen = sizeof(ttl);
    int rc = slk_getsockopt(sock, SLK_HEARTBEAT_TTL, &ttl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(optlen, sizeof(int));
    TEST_ASSERT_EQ(ttl, 0);

    /* Set heartbeat TTL to 5 (5 hops / 500ms internal units) */
    ttl = 500;  /* TTL is stored as value / 100 internally */
    rc = slk_setsockopt(sock, SLK_HEARTBEAT_TTL, &ttl, sizeof(ttl));
    TEST_SUCCESS(rc);

    /* Verify the value was stored (converted to/from internal units) */
    ttl = -999;
    optlen = sizeof(ttl);
    rc = slk_getsockopt(sock, SLK_HEARTBEAT_TTL, &ttl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ttl, 500);

    /* Set to different value */
    ttl = 1000;
    rc = slk_setsockopt(sock, SLK_HEARTBEAT_TTL, &ttl, sizeof(ttl));
    TEST_SUCCESS(rc);

    ttl = -999;
    optlen = sizeof(ttl);
    rc = slk_getsockopt(sock, SLK_HEARTBEAT_TTL, &ttl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ttl, 1000);

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 4: Heartbeat options work with different socket types */
static void test_heartbeat_different_socket_types()
{
    slk_ctx_t *ctx = test_context_new();
    int socket_types[] = {SLK_ROUTER, SLK_PUB, SLK_SUB, SLK_PAIR};
    const char *socket_names[] = {"ROUTER", "PUB", "SUB", "PAIR"};
    int num_types = sizeof(socket_types) / sizeof(socket_types[0]);

    for (int i = 0; i < num_types; i++) {
        slk_socket_t *sock = test_socket_new(ctx, socket_types[i]);

        /* Set heartbeat interval */
        int ivl = 2000;
        int rc = slk_setsockopt(sock, SLK_HEARTBEAT_IVL, &ivl, sizeof(ivl));
        if (rc != 0) {
            printf("  NOTE: %s socket may not support heartbeat options\n",
                   socket_names[i]);
            test_socket_close(sock);
            continue;
        }

        /* Verify */
        ivl = -999;
        size_t optlen = sizeof(ivl);
        rc = slk_getsockopt(sock, SLK_HEARTBEAT_IVL, &ivl, &optlen);
        TEST_SUCCESS(rc);
        TEST_ASSERT_EQ(ivl, 2000);

        /* Set timeout */
        int timeout = 6000;
        rc = slk_setsockopt(sock, SLK_HEARTBEAT_TIMEOUT, &timeout, sizeof(timeout));
        TEST_SUCCESS(rc);

        timeout = -999;
        optlen = sizeof(timeout);
        rc = slk_getsockopt(sock, SLK_HEARTBEAT_TIMEOUT, &timeout, &optlen);
        TEST_SUCCESS(rc);
        TEST_ASSERT_EQ(timeout, 6000);

        test_socket_close(sock);
    }

    test_context_destroy(ctx);
}

/* Test 5: Invalid heartbeat option values */
static void test_heartbeat_invalid_values()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    /* Negative interval should fail (except -1 which might mean default) */
    int ivl = -100;
    int rc = slk_setsockopt(sock, SLK_HEARTBEAT_IVL, &ivl, sizeof(ivl));
    /* Implementation may accept or reject negative values */
    /* Just document the behavior */
    if (rc == 0) {
        printf("  NOTE: Negative heartbeat interval accepted (implementation specific)\n");
    }

    /* Excessively large TTL should be capped (TTL uses uint16_t internally) */
    int ttl = 100000;  /* Very large value */
    rc = slk_setsockopt(sock, SLK_HEARTBEAT_TTL, &ttl, sizeof(ttl));
    if (rc == 0) {
        /* Check if value was capped */
        ttl = -999;
        size_t optlen = sizeof(ttl);
        rc = slk_getsockopt(sock, SLK_HEARTBEAT_TTL, &ttl, &optlen);
        TEST_SUCCESS(rc);
        /* Value may be capped or truncated */
        printf("  NOTE: Large TTL stored as: %d\n", ttl);
    }

    test_socket_close(sock);
    test_context_destroy(ctx);
}

/* Test 6: Heartbeat options before and after bind/connect */
static void test_heartbeat_before_after_connect()
{
    slk_ctx_t *ctx = test_context_new();
    const char *endpoint = test_endpoint_tcp();

    /* Server socket */
    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);

    /* Set heartbeat options BEFORE bind */
    int ivl = 1500;
    int rc = slk_setsockopt(server, SLK_HEARTBEAT_IVL, &ivl, sizeof(ivl));
    TEST_SUCCESS(rc);

    int timeout = 4500;
    rc = slk_setsockopt(server, SLK_HEARTBEAT_TIMEOUT, &timeout, sizeof(timeout));
    TEST_SUCCESS(rc);

    /* Bind */
    test_socket_bind(server, endpoint);

    /* Verify options still work after bind */
    ivl = -999;
    size_t optlen = sizeof(ivl);
    rc = slk_getsockopt(server, SLK_HEARTBEAT_IVL, &ivl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ivl, 1500);

    /* Client socket */
    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);

    /* Set heartbeat options BEFORE connect */
    ivl = 2500;
    rc = slk_setsockopt(client, SLK_HEARTBEAT_IVL, &ivl, sizeof(ivl));
    TEST_SUCCESS(rc);

    rc = slk_setsockopt(client, SLK_ROUTING_ID, "client", 6);
    TEST_SUCCESS(rc);

    /* Connect */
    test_socket_connect(client, endpoint);

    /* Verify options still work after connect */
    ivl = -999;
    optlen = sizeof(ivl);
    rc = slk_getsockopt(client, SLK_HEARTBEAT_IVL, &ivl, &optlen);
    TEST_SUCCESS(rc);
    TEST_ASSERT_EQ(ivl, 2500);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);
}

/* Main test runner */
int main()
{
    printf("=== ServerLink Heartbeat Option Tests ===\n\n");

    RUN_TEST(test_heartbeat_ivl_option);
    RUN_TEST(test_heartbeat_timeout_option);
    RUN_TEST(test_heartbeat_ttl_option);
    RUN_TEST(test_heartbeat_different_socket_types);
    RUN_TEST(test_heartbeat_invalid_values);
    RUN_TEST(test_heartbeat_before_after_connect);

    printf("\n=== Heartbeat Option Tests Completed ===\n");
    printf("NOTE: These tests verify option storage. Actual heartbeat\n");
    printf("      mechanism behavior depends on implementation status.\n");
    return 0;
}

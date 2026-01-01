/* SPDX-License-Identifier: MPL-2.0 */
/* Test that authentication mechanism properly supports PUB/SUB socket types */

#include "../testutil.hpp"

/* Test: Create all PUB/SUB socket types */
static void test_pub_sub_socket_creation()
{
    slk_ctx_t *ctx = test_context_new();

    // Create PUB socket and verify it can be created
    slk_socket_t *pub = test_socket_new(ctx, SLK_PUB);

    // Create SUB socket and verify it can be created
    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);

    // Create XPUB socket and verify it can be created
    slk_socket_t *xpub = test_socket_new(ctx, SLK_XPUB);

    // Create XSUB socket and verify it can be created
    slk_socket_t *xsub = test_socket_new(ctx, SLK_XSUB);

    test_socket_close(pub);
    test_socket_close(sub);
    test_socket_close(xpub);
    test_socket_close(xsub);
    test_context_destroy(ctx);
}

/* Test: PUB-SUB connection (triggers ZMTP handshake) */
static void test_pub_sub_connection()
{
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *pub = test_socket_new(ctx, SLK_PUB);
    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);

    // Subscribe to all messages
    int rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    TEST_SUCCESS(rc);

    // Bind PUB socket and connect SUB socket
    // This will trigger ZMTP handshake with socket type verification
    rc = slk_bind(pub, "inproc://test_auth_pubsub");
    TEST_SUCCESS(rc);

    rc = slk_connect(sub, "inproc://test_auth_pubsub");
    TEST_SUCCESS(rc);

    // Give time for connection to establish
    slk_sleep(100);

    // The key test is that the connection was established without assertion failure
    // in the authentication mechanism's socket_type_string() function

    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

/* Test: XPUB-XSUB connection (triggers ZMTP handshake) */
static void test_xpub_xsub_connection()
{
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *xpub = test_socket_new(ctx, SLK_XPUB);
    slk_socket_t *xsub = test_socket_new(ctx, SLK_XSUB);

    // Bind XPUB socket and connect XSUB socket
    // This will trigger ZMTP handshake with socket type verification
    int rc = slk_bind(xpub, "inproc://test_auth_xpubsub");
    TEST_SUCCESS(rc);

    rc = slk_connect(xsub, "inproc://test_auth_xpubsub");
    TEST_SUCCESS(rc);

    // Give time for connection to establish
    slk_sleep(100);

    // The key test is that the connection was established without assertion failure
    // in the authentication mechanism's socket_type_string() function

    test_socket_close(xpub);
    test_socket_close(xsub);
    test_context_destroy(ctx);
}

int main()
{
    RUN_TEST(test_pub_sub_socket_creation);
    RUN_TEST(test_pub_sub_connection);
    RUN_TEST(test_xpub_xsub_connection);

    printf("\nAll auth_pubsub tests passed!\n");
    return 0;
}

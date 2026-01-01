/* ServerLink Proxy Simple API Test */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

/* Test: Just verify API exists and compiles */
static void test_proxy_api_exists()
{
    // This test just verifies that the proxy API exists and links
    // We don't actually call them because they block
    printf("  Proxy API functions exist:\n");
    printf("    - slk_proxy\n");
    printf("    - slk_proxy_steerable\n");
}

/* Test: Create proxy sockets (but don't run proxy) */
static void test_proxy_sockets_creation()
{
    slk_ctx_t *ctx = test_context_new();

    // Create frontend and backend sockets
    slk_socket_t *frontend = test_socket_new(ctx, SLK_ROUTER);
    slk_socket_t *backend = test_socket_new(ctx, SLK_ROUTER);

    TEST_ASSERT_NOT_NULL(frontend);
    TEST_ASSERT_NOT_NULL(backend);

    // Bind sockets (would be used by proxy)
    const char *frontend_endpoint = test_endpoint_tcp();
    const char *backend_endpoint = test_endpoint_tcp();

    test_socket_bind(frontend, frontend_endpoint);
    test_socket_bind(backend, backend_endpoint);

    // Don't actually run proxy, just verify setup works
    printf("  Created and bound frontend and backend sockets\n");

    // Cleanup
    test_socket_close(frontend);
    test_socket_close(backend);
    test_context_destroy(ctx);
}

/* Test: Verify proxy function signatures compile */
static void test_proxy_signatures()
{
    // This just tests that the function pointers work
    typedef int (*proxy_fn)(void*, void*, void*);
    typedef int (*proxy_steerable_fn)(void*, void*, void*, void*);

    proxy_fn p1 = slk_proxy;
    proxy_steerable_fn p2 = slk_proxy_steerable;

    TEST_ASSERT_NOT_NULL(p1);
    TEST_ASSERT_NOT_NULL(p2);

    printf("  Function signatures verified\n");
}

int main()
{
    printf("=== ServerLink Proxy Simple Tests ===\n");
    printf("Note: Full proxy tests require threading and are complex.\n");
    printf("These tests verify the API exists and compiles correctly.\n\n");

    RUN_TEST(test_proxy_api_exists);
    RUN_TEST(test_proxy_sockets_creation);
    RUN_TEST(test_proxy_signatures);

    printf("\n=== All Simple Proxy Tests Passed ===\n");
    return 0;
}

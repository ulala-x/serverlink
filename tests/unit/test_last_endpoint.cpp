/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Test for SLK_LAST_ENDPOINT socket option */

#include <serverlink/serverlink.h>
#include "../testutil.hpp"
#include <string.h>
#include <assert.h>

// Test bind retrieves last endpoint
static void test_bind_last_endpoint()
{
    printf("Testing bind last endpoint retrieval...\n");

    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    // Bind to an endpoint
    const char *endpoint = test_endpoint_tcp();
    int rc = slk_bind(sock, endpoint);
    assert(rc == 0);

    // Get last endpoint
    char last_ep[256];
    size_t len = sizeof(last_ep);
    rc = slk_getsockopt(sock, SLK_LAST_ENDPOINT, last_ep, &len);
    assert(rc == 0);

    printf("  Bound to: %s\n", endpoint);
    printf("  Last endpoint: %s\n", last_ep);

    // Verify the last endpoint starts with tcp://127.0.0.1:
    assert(strncmp(last_ep, "tcp://127.0.0.1:", 16) == 0);

    test_socket_close(sock);
    test_context_destroy(ctx);

    printf("  PASSED\n");
}

// Test connect retrieves last endpoint
static void test_connect_last_endpoint()
{
    printf("Testing connect last endpoint retrieval...\n");

    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *server = test_socket_new(ctx, SLK_ROUTER);
    slk_socket_t *client = test_socket_new(ctx, SLK_ROUTER);

    // Server binds
    const char *endpoint = test_endpoint_tcp();
    int rc = slk_bind(server, endpoint);
    assert(rc == 0);

    // Client connects
    rc = slk_connect(client, endpoint);
    assert(rc == 0);

    // Get client's last endpoint
    char client_ep[256];
    size_t len = sizeof(client_ep);
    rc = slk_getsockopt(client, SLK_LAST_ENDPOINT, client_ep, &len);
    assert(rc == 0);

    printf("  Connected to: %s\n", endpoint);
    printf("  Last endpoint: %s\n", client_ep);

    // Verify the last endpoint contains the connect address (extract port from endpoint)
    const char *port_start = strrchr(endpoint, ':');
    assert(port_start != NULL);
    assert(strstr(client_ep, port_start) != NULL);

    test_socket_close(client);
    test_socket_close(server);
    test_context_destroy(ctx);

    printf("  PASSED\n");
}

// Test bind with wildcard port
static void test_bind_wildcard_port()
{
    printf("Testing bind with wildcard port...\n");

    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    // Bind to wildcard port (OS assigns port)
    const char *endpoint = "tcp://127.0.0.1:*";
    int rc = slk_bind(sock, endpoint);
    assert(rc == 0);

    // Get last endpoint - should contain the assigned port
    char last_ep[256];
    size_t len = sizeof(last_ep);
    rc = slk_getsockopt(sock, SLK_LAST_ENDPOINT, last_ep, &len);
    assert(rc == 0);

    printf("  Bound to: %s\n", endpoint);
    printf("  Last endpoint: %s\n", last_ep);

    // Verify the last endpoint has a specific port (not *)
    assert(strstr(last_ep, ":*") == NULL);
    assert(strncmp(last_ep, "tcp://127.0.0.1:", 16) == 0);

    test_socket_close(sock);
    test_context_destroy(ctx);

    printf("  PASSED\n");
}

// Test inproc endpoint
static void test_inproc_last_endpoint()
{
    printf("Testing inproc last endpoint retrieval...\n");

    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    // Bind to inproc endpoint
    const char *endpoint = "inproc://test-endpoint";
    int rc = slk_bind(sock, endpoint);
    assert(rc == 0);

    // Get last endpoint
    char last_ep[256];
    size_t len = sizeof(last_ep);
    rc = slk_getsockopt(sock, SLK_LAST_ENDPOINT, last_ep, &len);
    assert(rc == 0);

    printf("  Bound to: %s\n", endpoint);
    printf("  Last endpoint: %s\n", last_ep);

    // Verify exact match for inproc
    assert(strcmp(last_ep, endpoint) == 0);

    test_socket_close(sock);
    test_context_destroy(ctx);

    printf("  PASSED\n");
}

// Test buffer too small error
static void test_buffer_too_small()
{
    printf("Testing buffer too small error handling...\n");

    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *sock = test_socket_new(ctx, SLK_ROUTER);

    // Bind to an endpoint
    const char *endpoint = test_endpoint_tcp();
    int rc = slk_bind(sock, endpoint);
    assert(rc == 0);

    // Try to get last endpoint with too small buffer
    char small_buf[5];
    size_t len = sizeof(small_buf);
    rc = slk_getsockopt(sock, SLK_LAST_ENDPOINT, small_buf, &len);

    // Should fail with EINVAL
    assert(rc == -1);
    assert(slk_errno() == SLK_EINVAL);

    printf("  Correctly rejected small buffer\n");

    test_socket_close(sock);
    test_context_destroy(ctx);

    printf("  PASSED\n");
}

int main()
{
    printf("=== Testing SLK_LAST_ENDPOINT Socket Option ===\n\n");

    test_bind_last_endpoint();
    test_connect_last_endpoint();
    test_bind_wildcard_port();
    test_inproc_last_endpoint();
    test_buffer_too_small();

    printf("\n=== All SLK_LAST_ENDPOINT Tests Passed ===\n");
    return 0;
}

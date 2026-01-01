/* ServerLink - Modern Poller API Test */
/* Ported from libzmq test_poller.cpp */

#include "../testutil.hpp"
#include <cassert>
#include <cstring>
#include <cerrno>

// Test creating and destroying poller
void test_create_destroy()
{
    printf("Running test_create_destroy...\n");

    void *poller = slk_poller_new();
    assert(poller != nullptr);

    int rc = slk_poller_destroy(&poller);
    assert(rc == 0);
    assert(poller == nullptr);

    printf("  PASSED\n");
}

// Test null poller destroy
void test_null_poller_destroy()
{
    printf("Running test_null_poller_destroy...\n");

    void *null_poller = nullptr;
    int rc = slk_poller_destroy(&null_poller);
    assert(rc == -1);
    assert(errno == EFAULT);

    rc = slk_poller_destroy(nullptr);
    assert(rc == -1);
    assert(errno == EFAULT);

    printf("  PASSED\n");
}

// Test poller size
void test_poller_size()
{
    printf("Running test_poller_size...\n");

    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    slk_socket_t *socket = slk_socket(ctx, SLK_ROUTER);
    assert(socket);

    void *poller = slk_poller_new();
    assert(poller);

    int size = slk_poller_size(poller);
    assert(size == 0);

    int rc = slk_poller_add(poller, socket, nullptr, SLK_POLLIN);
    assert(rc == 0);

    size = slk_poller_size(poller);
    assert(size == 1);

    rc = slk_poller_remove(poller, socket);
    assert(rc == 0);

    size = slk_poller_size(poller);
    assert(size == 0);

    slk_poller_destroy(&poller);
    slk_close(socket);
    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

// Test adding socket twice fails
void test_add_twice_fails()
{
    printf("Running test_add_twice_fails...\n");

    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    slk_socket_t *socket = slk_socket(ctx, SLK_ROUTER);
    assert(socket);

    void *poller = slk_poller_new();
    assert(poller);

    int rc = slk_poller_add(poller, socket, nullptr, SLK_POLLIN);
    assert(rc == 0);

    // Attempt to add the same socket twice should fail
    rc = slk_poller_add(poller, socket, nullptr, SLK_POLLIN);
    assert(rc == -1);
    assert(errno == EINVAL);

    slk_poller_destroy(&poller);
    slk_close(socket);
    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

// Test removing unregistered socket fails
void test_remove_unregistered_fails()
{
    printf("Running test_remove_unregistered_fails...\n");

    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    slk_socket_t *socket = slk_socket(ctx, SLK_ROUTER);
    assert(socket);

    void *poller = slk_poller_new();
    assert(poller);

    // Attempt to remove socket that is not present
    int rc = slk_poller_remove(poller, socket);
    assert(rc == -1);
    assert(errno == EINVAL);

    slk_poller_destroy(&poller);
    slk_close(socket);
    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

// Test modifying unregistered socket fails
void test_modify_unregistered_fails()
{
    printf("Running test_modify_unregistered_fails...\n");

    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    slk_socket_t *socket = slk_socket(ctx, SLK_ROUTER);
    assert(socket);

    void *poller = slk_poller_new();
    assert(poller);

    // Attempt to modify socket that is not present
    int rc = slk_poller_modify(poller, socket, SLK_POLLIN);
    assert(rc == -1);
    assert(errno == EINVAL);

    slk_poller_destroy(&poller);
    slk_close(socket);
    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

// Test basic polling with sockets
void test_poll_basic()
{
    printf("Running test_poll_basic...\n");

    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx);

    // Create PUB-SUB sockets for simpler testing
    slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
    assert(pub);

    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    assert(sub);

    // Subscribe to all messages
    int rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    assert(rc == 0);

    // Bind publisher and connect subscriber
    rc = slk_bind(pub, "tcp://127.0.0.1:0");
    assert(rc == 0);

    char endpoint[256];
    size_t endpoint_len = sizeof(endpoint);
    rc = slk_getsockopt(pub, SLK_LAST_ENDPOINT, endpoint, &endpoint_len);
    assert(rc == 0);

    rc = slk_connect(sub, endpoint);
    assert(rc == 0);

    // Give time for connection and subscription
    slk_sleep(200);

    // Set up poller for subscriber
    void *poller = slk_poller_new();
    assert(poller);

    rc = slk_poller_add(poller, sub, sub, SLK_POLLIN);
    assert(rc == 0);

    // Send a message
    const char *msg = "Hello";
    rc = slk_send(pub, msg, strlen(msg), 0);
    assert(rc >= 0);

    // Wait for event
    slk_poller_event_t event;
    rc = slk_poller_wait(poller, &event, 1000);
    assert(rc == 0);
    assert(event.socket == sub);
    assert(event.user_data == sub);
    assert(event.events & SLK_POLLIN);

    // Receive the message
    char buf[256];
    rc = slk_recv(sub, buf, sizeof(buf), 0);
    assert(rc > 0);
    assert(memcmp(buf, msg, strlen(msg)) == 0);

    // Polling again should timeout
    rc = slk_poller_wait(poller, &event, 0);
    assert(rc == -1);
    assert(errno == EAGAIN);

    slk_poller_destroy(&poller);
    slk_close(pub);
    slk_close(sub);
    slk_ctx_destroy(ctx);

    printf("  PASSED\n");
}

// Test waiting on empty poller with timeout
void test_wait_empty_with_timeout()
{
    printf("Running test_wait_empty_with_timeout...\n");

    void *poller = slk_poller_new();
    assert(poller);

    slk_poller_event_t event;
    // Waiting on poller with no registered sockets should report error
    int rc = slk_poller_wait(poller, &event, 0);
    assert(rc == -1);
    assert(errno == EAGAIN);

    slk_poller_destroy(&poller);

    printf("  PASSED\n");
}

// Test waiting on empty poller without timeout fails
void test_wait_empty_without_timeout()
{
    printf("Running test_wait_empty_without_timeout...\n");

    void *poller = slk_poller_new();
    assert(poller);

    slk_poller_event_t event;
    // This would never be able to return since no socket was registered
    int rc = slk_poller_wait(poller, &event, -1);
    assert(rc == -1);
    assert(errno == EFAULT);

    slk_poller_destroy(&poller);

    printf("  PASSED\n");
}

int main()
{
    printf("\n===== ServerLink Poller API Tests =====\n\n");

    test_create_destroy();
    test_null_poller_destroy();
    test_poller_size();
    test_add_twice_fails();
    test_remove_unregistered_fails();
    test_modify_unregistered_fails();
    test_poll_basic();
    test_wait_empty_with_timeout();
    test_wait_empty_without_timeout();

    printf("\n===== All Poller Tests Passed! =====\n\n");

    return 0;
}

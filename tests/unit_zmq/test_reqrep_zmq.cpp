#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

static void test_dr_inproc()
{
    printf("[ZMQ] test_dr_inproc starting...\n");
    void *ctx = zmq_ctx_new();
    void *router = zmq_socket(ctx, ZMQ_ROUTER);
    zmq_setsockopt(router, ZMQ_ROUTING_ID, "SRV", 3);
    int rc = zmq_bind(router, "inproc://dr_test");
    assert(rc == 0);

    void *dealer = zmq_socket(ctx, ZMQ_DEALER);
    zmq_setsockopt(dealer, ZMQ_ROUTING_ID, "CLI", 3);
    rc = zmq_connect(dealer, "inproc://dr_test");
    assert(rc == 0);

    usleep(100000);

    // Dealer sends to Router
    zmq_send(dealer, "Hello", 5, 0);

    // Router receives: [ID][Data]
    char buf[256];
    rc = zmq_recv(router, buf, sizeof(buf), 0);
    assert(rc == 3); // "CLI"
    if(memcmp(buf, "CLI", 3) != 0) printf("Expected CLI, got %.*s\n", rc, buf);

    rc = zmq_recv(router, buf, sizeof(buf), 0);
    assert(rc == 5); // "Hello"

    zmq_close(dealer);
    zmq_close(router);
    zmq_ctx_term(ctx);
    printf("[ZMQ] test_dr_inproc passed\n");
}

static void test_dr_tcp()
{
    printf("[ZMQ] test_dr_tcp starting...\n");
    void *ctx = zmq_ctx_new();
    void *router = zmq_socket(ctx, ZMQ_ROUTER);
    zmq_setsockopt(router, ZMQ_ROUTING_ID, "SRV", 3);
    int rc = zmq_bind(router, "tcp://127.0.0.1:*");
    assert(rc == 0);
    
    char endpoint[256]; size_t size = sizeof(endpoint);
    zmq_getsockopt(router, ZMQ_LAST_ENDPOINT, endpoint, &size);
    printf("      Bound to: %s\n", endpoint);

    void *dealer = zmq_socket(ctx, ZMQ_DEALER);
    zmq_setsockopt(dealer, ZMQ_ROUTING_ID, "CLI", 3);
    rc = zmq_connect(dealer, endpoint);
    assert(rc == 0);

    usleep(200000);

    zmq_send(dealer, "TCP", 3, 0);

    char buf[256];
    rc = zmq_recv(router, buf, sizeof(buf), 0); // ID
    assert(rc == 3);
    rc = zmq_recv(router, buf, sizeof(buf), 0); // Data
    assert(rc == 3);

    zmq_close(dealer);
    zmq_close(router);
    zmq_ctx_term(ctx);
    printf("[ZMQ] test_dr_tcp passed\n");
}

int main() {
    test_dr_inproc();
    test_dr_tcp();
    return 0;
}

#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

static void test_dealer_inproc()
{
    printf("[ZMQ] test_dealer_inproc starting...\n");
    void *ctx = zmq_ctx_new();
    void *sb = zmq_socket(ctx, ZMQ_DEALER);
    int rc = zmq_bind(sb, "inproc://dealer_test");
    assert(rc == 0);

    void *sc = zmq_socket(ctx, ZMQ_DEALER);
    rc = zmq_connect(sc, "inproc://dealer_test");
    assert(rc == 0);

    usleep(100000);

    zmq_send(sc, "ABC", 3, 0);
    char buf[256];
    rc = zmq_recv(sb, buf, sizeof(buf), 0);
    assert(rc == 3);

    zmq_close(sc);
    zmq_close(sb);
    zmq_ctx_term(ctx);
    printf("[ZMQ] test_dealer_inproc passed\n");
}

static void test_dealer_tcp()
{
    printf("[ZMQ] test_dealer_tcp starting...\n");
    void *ctx = zmq_ctx_new();
    void *sb = zmq_socket(ctx, ZMQ_DEALER);
    int rc = zmq_bind(sb, "tcp://127.0.0.1:*");
    assert(rc == 0);
    
    char endpoint[256]; size_t size = sizeof(endpoint);
    zmq_getsockopt(sb, ZMQ_LAST_ENDPOINT, endpoint, &size);
    printf("      Bound to: %s\n", endpoint);

    void *sc = zmq_socket(ctx, ZMQ_DEALER);
    rc = zmq_connect(sc, endpoint);
    assert(rc == 0);

    usleep(200000);

    zmq_send(sc, "TCP", 3, 0);
    char buf[256];
    rc = zmq_recv(sb, buf, sizeof(buf), 0);
    assert(rc == 3);

    zmq_close(sc);
    zmq_close(sb);
    zmq_ctx_term(ctx);
    printf("[ZMQ] test_dealer_tcp passed\n");
}

int main() {
    test_dealer_inproc();
    test_dealer_tcp();
    return 0;
}

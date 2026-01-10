#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define TEST_ASSERT_EQ(a, b) assert((a) == (b))
#define TEST_ASSERT_STR_EQ(a, b) assert(strcmp((a), (b)) == 0)

static void test_pair_inproc()
{
    printf("[ZMQ] test_pair_inproc starting...\n");
    void *ctx = zmq_ctx_new();
    void *server = zmq_socket(ctx, ZMQ_PAIR);
    int rc = zmq_bind(server, "inproc://pair_test");
    assert(rc == 0);

    void *client = zmq_socket(ctx, ZMQ_PAIR);
    rc = zmq_connect(client, "inproc://pair_test");
    assert(rc == 0);

    // Sleep to allow connection (inproc is fast but still)
    usleep(100000);

    const char *msg = "Hello inproc";
    zmq_send(server, msg, strlen(msg), 0);
    
    char buf[256];
    rc = zmq_recv(client, buf, sizeof(buf), 0);
    assert(rc == (int)strlen(msg));
    buf[rc] = '\0';
    TEST_ASSERT_STR_EQ(buf, msg);

    zmq_close(client);
    zmq_close(server);
    zmq_ctx_term(ctx);
    printf("[ZMQ] test_pair_inproc passed\n");
}

static void test_pair_tcp()
{
    printf("[ZMQ] test_pair_tcp starting...\n");
    void *ctx = zmq_ctx_new();
    void *server = zmq_socket(ctx, ZMQ_PAIR);
    // ZMQ allows binding to port 0 for ephemeral port
    int rc = zmq_bind(server, "tcp://127.0.0.1:*"); 
    assert(rc == 0);
    
    char endpoint[256]; size_t size = sizeof(endpoint);
    zmq_getsockopt(server, ZMQ_LAST_ENDPOINT, endpoint, &size);
    printf("      Bound to: %s\n", endpoint);

    void *client = zmq_socket(ctx, ZMQ_PAIR);
    rc = zmq_connect(client, endpoint);
    assert(rc == 0);

    usleep(200000);

    const char *msg = "Hello TCP";
    zmq_send(client, msg, strlen(msg), 0);
    
    char buf[256];
    rc = zmq_recv(server, buf, sizeof(buf), 0);
    assert(rc == (int)strlen(msg));
    buf[rc] = '\0';
    TEST_ASSERT_STR_EQ(buf, msg);

    zmq_close(client);
    zmq_close(server);
    zmq_ctx_term(ctx);
    printf("[ZMQ] test_pair_tcp passed\n");
}

int main() {
    test_pair_inproc();
    test_pair_tcp();
    return 0;
}

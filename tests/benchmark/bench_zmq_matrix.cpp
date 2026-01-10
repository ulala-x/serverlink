#include <zmq.h>
#include <thread>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstring>

int main(int argc, char **argv) {
    if (argc < 5) return 1;
    const char *transport = argv[1];
    int msg_size = atoi(argv[2]);
    int msg_count = atoi(argv[3]);
    int pattern = atoi(argv[4]);
    char endpoint[256];
    sprintf(endpoint, "%s://127.0.0.1:777%d", transport, pattern);

    void *ctx = zmq_ctx_new();
    void *server, *client;

    if (pattern == 0) {
        server = zmq_socket(ctx, ZMQ_ROUTER); client = zmq_socket(ctx, ZMQ_ROUTER);
        zmq_setsockopt(server, ZMQ_ROUTING_ID, "SRV", 3);
        zmq_setsockopt(client, ZMQ_ROUTING_ID, "CLI", 3);
    } else if (pattern == 1) {
        server = zmq_socket(ctx, ZMQ_ROUTER); client = zmq_socket(ctx, ZMQ_DEALER);
        zmq_setsockopt(server, ZMQ_ROUTING_ID, "SRV", 3);
    } else {
        server = zmq_socket(ctx, ZMQ_DEALER); client = zmq_socket(ctx, ZMQ_DEALER);
    }

    zmq_bind(server, endpoint);
    zmq_connect(client, endpoint);

    std::vector<char> buf(msg_size + 256);
    std::thread receiver([&]() {
        for (int i = 0; i < msg_count; i++) {
            if (pattern == 0 || pattern == 1) zmq_recv(server, buf.data(), buf.size(), 0);
            zmq_recv(server, buf.data(), buf.size(), 0);
        }
    });

    std::vector<char> data(msg_size, 'A');
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < msg_count; i++) {
        if (pattern == 0) zmq_send(client, "SRV", 3, ZMQ_SNDMORE);
        zmq_send(client, data.data(), data.size(), 0);
    }
    receiver.join();
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    printf("%.0f", msg_count / elapsed);

    zmq_close(client); zmq_close(server);
    zmq_ctx_term(ctx);
    return 0;
}

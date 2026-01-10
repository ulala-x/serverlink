#include <zmq.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

int main(int argc, char **argv) {
    if (argc < 5) return 1;
    const char *endpoint = argv[1];
    int msg_size = atoi(argv[2]);
    int msg_count = atoi(argv[3]);
    bool is_latency = (atoi(argv[4]) == 1);

    void *ctx = zmq_ctx_new();
    void *server = zmq_socket(ctx, ZMQ_ROUTER);
    void *client = zmq_socket(ctx, ZMQ_ROUTER);

    zmq_setsockopt(server, ZMQ_ROUTING_ID, "SRV", 3);
    zmq_setsockopt(client, ZMQ_ROUTING_ID, "CLI", 3);

    zmq_bind(server, endpoint);
    zmq_connect(client, endpoint);

    std::vector<char> data(msg_size, 'A');
    char buf[262144 + 1024];

    std::thread receiver([&]() {
        zmq_recv(server, buf, sizeof(buf), 0); // CLI
        zmq_recv(server, buf, sizeof(buf), 0); // READY
        zmq_send(server, "CLI", 3, ZMQ_SNDMORE);
        zmq_send(server, "GO", 2, 0);

        for (int i = 0; i < msg_count; i++) {
            zmq_recv(server, buf, sizeof(buf), 0); // ID
            zmq_recv(server, buf, sizeof(buf), 0); // Data
            if (is_latency) {
                zmq_send(server, "CLI", 3, ZMQ_SNDMORE);
                zmq_send(server, buf, msg_size, 0);
            }
        }
    });

    zmq_send(client, "SRV", 3, ZMQ_SNDMORE);
    zmq_send(client, "READY", 5, 0);
    zmq_recv(client, buf, sizeof(buf), 0); // SRV
    zmq_recv(client, buf, sizeof(buf), 0); // GO

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < msg_count; i++) {
        zmq_send(client, "SRV", 3, ZMQ_SNDMORE);
        zmq_send(client, data.data(), msg_size, 0);
        if (is_latency) {
            zmq_recv(client, buf, sizeof(buf), 0);
            zmq_recv(client, buf, sizeof(buf), 0);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    receiver.join();

    double total_us = std::chrono::duration<double, std::micro>(end - start).count();
    if (is_latency) printf("%.2f", total_us / msg_count);
    else printf("%.0f", msg_count / (total_us / 1000000.0));

    zmq_close(client); zmq_close(server);
    zmq_ctx_term(ctx);
    return 0;
}

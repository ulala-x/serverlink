#include <zmq.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv) {
    if (argc < 4) return 1;
    const char *endpoint = argv[1];
    int msg_size = atoi(argv[2]);
    int msg_count = atoi(argv[3]);
    bool is_latency = (atoi(argv[4]) == 1);
    int type = (strstr(argv[0], "dealer") ? ZMQ_DEALER : ZMQ_PAIR);

    void *ctx = zmq_ctx_new();
    void *sb = zmq_socket(ctx, type);
    void *sc = zmq_socket(ctx, type);

    zmq_bind(sb, endpoint);
    zmq_connect(sc, endpoint);

    std::vector<char> data(msg_size, 'A');
    std::vector<char> buf(msg_size + 1024);

    // [Handshake]
    zmq_send(sc, "READY", 5, 0);
    zmq_recv(sb, buf.data(), buf.size(), 0);
    zmq_send(sb, "GO", 2, 0);
    zmq_recv(sc, buf.data(), buf.size(), 0);

    if (is_latency) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < msg_count; i++) {
            zmq_send(sc, data.data(), msg_size, 0);
            zmq_recv(sb, buf.data(), buf.size(), 0);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double total_us = std::chrono::duration<double, std::micro>(end - start).count();
        printf("%.2f", total_us / msg_count);
    } else {
        std::thread receiver([&]() {
            for (int i = 0; i < msg_count; i++) {
                zmq_recv(sb, buf.data(), buf.size(), 0);
            }
        });
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < msg_count; i++) {
            zmq_send(sc, data.data(), msg_size, 0);
        }
        receiver.join();
        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        printf("%.0f", msg_count / elapsed);
    }

    zmq_close(sc); zmq_close(sb);
    zmq_ctx_term(ctx);
    return 0;
}

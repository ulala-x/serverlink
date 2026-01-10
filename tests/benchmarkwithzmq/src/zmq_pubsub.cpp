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
    void *pub = zmq_socket(ctx, ZMQ_XPUB);
    void *sub = zmq_socket(ctx, ZMQ_SUB);

    zmq_bind(pub, endpoint);
    zmq_connect(sub, endpoint);
    zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "", 0);

    // Sync sub
    char sync[32];
    while (zmq_recv(pub, sync, 32, 0) <= 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::vector<char> data(msg_size, 'A');
    char buf[1024];

    std::thread receiver([&]() {
        for (int i = 0; i < msg_count; i++) {
            zmq_recv(sub, buf, 1024, 0);
        }
    });

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < msg_count; i++) {
        zmq_send(pub, data.data(), msg_size, 0);
    }
    receiver.join();
    auto end = std::chrono::high_resolution_clock::now();

    double total = std::chrono::duration<double, std::micro>(end - start).count();
    printf("%.0f", msg_count / (total / 1000000.0));

    zmq_close(sub); zmq_close(pub);
    zmq_ctx_term(ctx);
    return 0;
}
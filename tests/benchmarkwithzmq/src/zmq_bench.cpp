#include <zmq.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv) {
    if (argc < 6) return 1;
    const char *endpoint = argv[1];
    int msg_size = atoi(argv[2]);
    int msg_count = atoi(argv[3]);
    bool is_latency = (atoi(argv[4]) == 1);
    int pattern = atoi(argv[5]); // 0:PAIR, 1:D-D, 2:D-R, 3:R-R, 4:P-S

    void *ctx = zmq_ctx_new();
    void *server, *client;

    if (pattern == 0) { server = zmq_socket(ctx, ZMQ_PAIR); client = zmq_socket(ctx, ZMQ_PAIR); }
    else if (pattern == 1) { server = zmq_socket(ctx, ZMQ_DEALER); client = zmq_socket(ctx, ZMQ_DEALER); }
    else if (pattern == 2) { server = zmq_socket(ctx, ZMQ_ROUTER); client = zmq_socket(ctx, ZMQ_DEALER); }
    else if (pattern == 3) { server = zmq_socket(ctx, ZMQ_ROUTER); client = zmq_socket(ctx, ZMQ_ROUTER); }
    else { server = zmq_socket(ctx, ZMQ_XPUB); client = zmq_socket(ctx, ZMQ_SUB); zmq_setsockopt(client, ZMQ_SUBSCRIBE, "", 0); }

    if (pattern == 3) { zmq_setsockopt(server, ZMQ_ROUTING_ID, "SRV", 3); zmq_setsockopt(client, ZMQ_ROUTING_ID, "CLI", 3); }
    else if (pattern == 2) { zmq_setsockopt(server, ZMQ_ROUTING_ID, "SRV", 3); }

    zmq_bind(server, endpoint);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    zmq_connect(client, endpoint);

    std::vector<char> data(msg_size, 'A');
    std::vector<char> buf(msg_size + 1024);

    std::thread receiver([&]() {
        char hbuf[1024];
        if (pattern == 2 || pattern == 3) { int rl = zmq_recv(server, buf.data(), buf.size(), 0); zmq_recv(server, hbuf, 1024, 0); 
            zmq_send(server, buf.data(), rl, ZMQ_SNDMORE); zmq_send(server, "GO", 2, 0);
        } else { zmq_recv(server, hbuf, 1024, 0); zmq_send(server, "GO", 2, 0); }

        for (int i = 0; i < msg_count; i++) {
            if (pattern == 2 || pattern == 3) { int rl = zmq_recv(server, buf.data(), buf.size(), 0); }
            zmq_recv(server, buf.data(), buf.size(), 0);
            if (is_latency) {
                if (pattern == 2 || pattern == 3) { /* echo ID */ }
                zmq_send(server, buf.data(), msg_size, 0);
            }
        }
    });

    char hbuf[1024];
    if (pattern == 3) { zmq_send(client, "SRV", 3, ZMQ_SNDMORE); }
    zmq_send(client, "READY", 5, 0);
    if (pattern == 3) { zmq_recv(client, hbuf, 1024, 0); }
    zmq_recv(client, hbuf, 1024, 0);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < msg_count; i++) {
        if (pattern == 3) { zmq_send(client, "SRV", 3, ZMQ_SNDMORE); }
        zmq_send(client, data.data(), msg_size, 0);
        if (is_latency) { if (pattern == 3) zmq_recv(client, hbuf, 1024, 0); zmq_recv(client, hbuf, 1024, 0); }
    }
    auto end = std::chrono::high_resolution_clock::now();
    receiver.join();

    double elapsed = std::chrono::duration<double>(end - start).count();
    if (is_latency) printf("%.2f", (elapsed * 1000000.0) / msg_count);
    else printf("%.0f", msg_count / elapsed);

    zmq_close(client); zmq_close(server);
    zmq_ctx_term(ctx);
    return 0;
}

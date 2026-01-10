#include <serverlink/serverlink.h>
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
    int type = (strstr(argv[0], "dealer") ? SLK_DEALER : SLK_PAIR);

    void *ctx = slk_ctx_new();
    void *sb = slk_socket(ctx, type);
    void *sc = slk_socket(ctx, type);

    slk_bind(sb, endpoint);
    slk_connect(sc, endpoint);

    std::vector<char> data(msg_size, 'A');
    std::vector<char> buf(msg_size + 1024);

    // [Handshake]
    slk_send(sc, "READY", 5, 0);
    slk_recv(sb, buf.data(), buf.size(), 0);
    slk_send(sb, "GO", 2, 0);
    slk_recv(sc, buf.data(), buf.size(), 0);

    if (is_latency) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < msg_count; i++) {
            slk_send(sc, data.data(), msg_size, 0);
            slk_recv(sb, buf.data(), buf.size(), 0);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double total_us = std::chrono::duration<double, std::micro>(end - start).count();
        printf("%.2f", total_us / msg_count);
    } else {
        std::thread receiver([&]() {
            for (int i = 0; i < msg_count; i++) {
                slk_recv(sb, buf.data(), buf.size(), 0);
            }
        });
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < msg_count; i++) {
            slk_send(sc, data.data(), msg_size, 0);
        }
        receiver.join();
        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        printf("%.0f", msg_count / elapsed);
    }

    slk_close(sc); slk_close(sb);
    slk_ctx_term(ctx);
    return 0;
}

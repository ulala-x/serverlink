#include <serverlink/serverlink.h>
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

    void *ctx = slk_ctx_new();
    void *server = slk_socket(ctx, SLK_ROUTER);
    void *client = slk_socket(ctx, SLK_ROUTER);

    slk_setsockopt(server, SLK_ROUTING_ID, "SRV", 3);
    slk_setsockopt(client, SLK_ROUTING_ID, "CLI", 3);

    slk_bind(server, endpoint);
    slk_connect(client, endpoint);

    std::vector<char> data(msg_size, 'A');
    char buf[262144 + 1024];

    std::thread receiver([&]() {
        slk_recv(server, buf, sizeof(buf), 0); // CLI
        slk_recv(server, buf, sizeof(buf), 0); // READY
        slk_send(server, "CLI", 3, SLK_SNDMORE);
        slk_send(server, "GO", 2, 0);

        for (int i = 0; i < msg_count; i++) {
            slk_recv(server, buf, sizeof(buf), 0); // ID
            slk_recv(server, buf, sizeof(buf), 0); // Data
            if (is_latency) {
                slk_send(server, "CLI", 3, SLK_SNDMORE);
                slk_send(server, buf, msg_size, 0);
            }
        }
    });

    slk_send(client, "SRV", 3, SLK_SNDMORE);
    slk_send(client, "READY", 5, 0);
    slk_recv(client, buf, sizeof(buf), 0); // SRV
    slk_recv(client, buf, sizeof(buf), 0); // GO

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < msg_count; i++) {
        slk_send(client, "SRV", 3, SLK_SNDMORE);
        slk_send(client, data.data(), msg_size, 0);
        if (is_latency) {
            slk_recv(client, buf, sizeof(buf), 0);
            slk_recv(client, buf, sizeof(buf), 0);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    receiver.join();

    double total_us = std::chrono::duration<double, std::micro>(end - start).count();
    if (is_latency) printf("%.2f", total_us / msg_count);
    else printf("%.0f", msg_count / (total_us / 1000000.0));

    slk_close(client); slk_close(server);
    slk_ctx_term(ctx);
    return 0;
}

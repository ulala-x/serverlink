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

    slk_ctx_t *ctx = slk_ctx_new();
    slk_socket_t *pub = slk_socket(ctx, SLK_XPUB);
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);

    // Bind and Connect
    slk_bind(pub, endpoint);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    slk_connect(sub, endpoint);
    
    // Subscribe to all topics
    slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);

    // [Handshake] XPUB waits for subscription notification (\x01 + topic)
    char sync_buf[256];
    bool subscribed = false;
    int retries = 500; // Max 5 seconds
    while (retries-- > 0) {
        int rc = slk_recv(pub, sync_buf, sizeof(sync_buf), SLK_DONTWAIT);
        if (rc > 0 && sync_buf[0] == 1) {
            subscribed = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!subscribed) {
        slk_close(sub); slk_close(pub); slk_ctx_destroy(ctx);
        return 1; // Failed to sync
    }

    std::vector<char> data(msg_size, 'A');
    char buf[msg_size + 1024];

    std::thread receiver([&]() {
        for (int i = 0; i < msg_count; i++) {
            slk_recv(sub, buf, sizeof(buf), 0);
            if (is_latency) slk_send(sub, "ACK", 3, 0); // Latency echo
        }
    });

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < msg_count; i++) {
        slk_send(pub, data.data(), msg_size, 0);
        if (is_latency) slk_recv(pub, buf, sizeof(buf), 0);
    }
    receiver.join();
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed = std::chrono::duration<double>(end - start).count();
    if (is_latency) printf("%.2f\n", (elapsed * 1000000.0) / msg_count);
    else printf("%.0f\n", msg_count / elapsed);

    slk_close(sub); slk_close(pub);
    slk_ctx_destroy(ctx);
    return 0;
}

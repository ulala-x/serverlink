/* SPDX-License-Identifier: MPL-2.0 */
#include <serverlink/serverlink.h>
#include <thread>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <assert.h>

int main(int argc, char **argv) {
    const char *endpoint = (argc > 1) ? argv[1] : "inproc://bench";
    int msg_count = (argc > 2) ? atoi(argv[2]) : 100000;
    int msg_size = (argc > 3) ? atoi(argv[3]) : 1024;

    slk_ctx_t *ctx = slk_ctx_new();
    slk_socket_t *sb = slk_socket(ctx, SLK_PAIR);
    slk_socket_t *sc = slk_socket(ctx, SLK_PAIR);

    slk_bind(sb, endpoint);
    slk_connect(sc, endpoint);

    std::vector<char> data(msg_size, 'A');
    std::vector<char> buf(msg_size);

    auto start = std::chrono::high_resolution_clock::now();

    std::thread receiver([&]() {
        for (int i = 0; i < msg_count; i++) {
            int rc = slk_recv(sb, buf.data(), buf.size(), 0);
            assert(rc == msg_size);
        }
    });

    for (int i = 0; i < msg_count; i++) {
        int rc = slk_send(sc, data.data(), data.size(), 0);
        assert(rc == msg_size);
    }

    receiver.join();
    auto end = std::chrono::high_resolution_clock::now();
    
    double elapsed = std::chrono::duration<double>(end - start).count();
    double throughput = msg_count / elapsed;
    double bandwidth = (msg_count * msg_size) / (elapsed * 1024 * 1024);

    printf("Result: %.2f msg/s, %.2f MB/s\n", throughput, bandwidth);

    slk_close(sc);
    slk_close(sb);
    slk_ctx_destroy(ctx);
    return 0;
}

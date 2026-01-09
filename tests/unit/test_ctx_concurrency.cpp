/* SPDX-License-Identifier: MPL-2.0 */

#include <serverlink/serverlink.h>
#include <thread>
#include <vector>
#include <stdio.h> 
#include <assert.h>

#define NUM_THREADS 20
#define ITERATIONS 100

void socket_spam_thread(slk_ctx_t* ctx) {
    for (int i = 0; i < ITERATIONS; ++i) {
        slk_socket_t* s = slk_socket(ctx, SLK_ROUTER);
        if (s) {
            slk_close(s);
        }
    }
}

int main() {
    printf("Starting Context Concurrency Stress Test...\n");
    printf("Threads: %d, Iterations: %d (Total sockets: %d)\n", NUM_THREADS, ITERATIONS, NUM_THREADS * ITERATIONS);

    slk_ctx_t* ctx = slk_ctx_new();
    assert(ctx);

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(socket_spam_thread, ctx);
    }

    for (auto& t : threads) {
        t.join();
    }

    slk_ctx_destroy(ctx);
    printf("SUCCESS: Context remained stable during concurrent socket management.\n");

    return 0;
}


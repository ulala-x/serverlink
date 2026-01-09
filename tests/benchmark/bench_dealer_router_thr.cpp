/* SPDX-License-Identifier: MPL-2.0 */

#include <serverlink/serverlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <chrono>

#define MESSAGES_COUNT 100000
#define MESSAGE_SIZE 64
#define ENDPOINT "tcp://127.0.0.1:5555"

void server_thread(slk_ctx_t* ctx) {
    slk_socket_t* sock = slk_socket(ctx, SLK_ROUTER);
    slk_bind(sock, ENDPOINT);

    char* buffer = (char*)malloc(MESSAGE_SIZE + 256);
    
    // Receive messages
    for (int i = 0; i < MESSAGES_COUNT; i++) {
        // ROUTER receives: [Sender ID][Data]
        slk_recv(sock, buffer, MESSAGE_SIZE + 256, 0); // ID
        slk_recv(sock, buffer, MESSAGE_SIZE + 256, 0); // Data
    }

    free(buffer);
    slk_close(sock);
}

void client_thread(slk_ctx_t* ctx, double* duration) {
    slk_socket_t* sock = slk_socket(ctx, SLK_DEALER);
    slk_connect(sock, ENDPOINT);

    // Initial wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    char* data = (char*)malloc(MESSAGE_SIZE);
    memset(data, 'A', MESSAGE_SIZE);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < MESSAGES_COUNT; i++) {
        // DEALER sends: [Data] (Identity is added automatically by ROUTER on receive)
        slk_send(sock, data, MESSAGE_SIZE, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    *duration = std::chrono::duration<double>(end - start).count();

    free(data);
    slk_close(sock);
}

int main() {
    printf("ServerLink DEALER-ROUTER Throughput Benchmark\n");
    printf("Message size: %d [B], Count: %d\n", MESSAGE_SIZE, MESSAGES_COUNT);

    slk_ctx_t* ctx = slk_ctx_new();
    
    double duration = 0;
    std::thread s(server_thread, ctx);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::thread c(client_thread, ctx, &duration);

    c.join();
    s.join();

    double throughput = (double)MESSAGES_COUNT / duration;
    printf("Throughput: %.0f [msg/s]\n", throughput);

    slk_ctx_destroy(ctx);
    return 0;
}

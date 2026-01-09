/* SPDX-License-Identifier: MPL-2.0 */

#include <serverlink/serverlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <chrono>

#define ROUNDTRIPS 10000
#define MESSAGE_SIZE 64
#define ENDPOINT "tcp://127.0.0.1:5557"

void server_thread(slk_ctx_t* ctx) {
    slk_socket_t* sock = slk_socket(ctx, SLK_ROUTER);
    slk_bind(sock, ENDPOINT);

    char id[256];
    char data[MESSAGE_SIZE];
    
    for (int i = 0; i < ROUNDTRIPS; i++) {
        int id_len = slk_recv(sock, id, sizeof(id), 0);
        int data_len = slk_recv(sock, data, sizeof(data), 0);
        
        slk_send(sock, id, id_len, SLK_SNDMORE);
        slk_send(sock, data, data_len, 0);
    }

    slk_close(sock);
}

void client_thread(slk_ctx_t* ctx, double* avg_latency) {
    slk_socket_t* sock = slk_socket(ctx, SLK_DEALER);
    slk_connect(sock, ENDPOINT);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    char data[MESSAGE_SIZE];
    memset(data, 'A', MESSAGE_SIZE);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ROUNDTRIPS; i++) {
        slk_send(sock, data, MESSAGE_SIZE, 0);
        slk_recv(sock, data, MESSAGE_SIZE, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double, std::micro>(end - start).count();
    *avg_latency = total_time / ROUNDTRIPS;

    slk_close(sock);
}

int main() {
    printf("ServerLink DEALER-ROUTER Latency Benchmark\n");
    printf("Message size: %d [B], Roundtrips: %d\n", MESSAGE_SIZE, ROUNDTRIPS);

    slk_ctx_t* ctx = slk_ctx_new();
    
    double avg_latency = 0;
    std::thread s(server_thread, ctx);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::thread c(client_thread, ctx, &avg_latency);

    c.join();
    s.join();

    printf("Average Latency: %.2f [us]\n", avg_latency);

    slk_ctx_destroy(ctx);
    return 0;
}

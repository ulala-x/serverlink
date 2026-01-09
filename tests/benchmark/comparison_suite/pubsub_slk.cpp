/* SPDX-License-Identifier: MPL-2.0 */
#include <serverlink/serverlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <chrono>
#include <atomic>

#define MESSAGES_COUNT 50000 
#define TCP_ADDR "tcp://127.0.0.1:18007"

std::atomic<bool> server_ready{false};

void run_server(slk_ctx_t* ctx, int size) {
    slk_socket_t* sock = slk_socket(ctx, SLK_SUB);
    int hwm = 0;
    slk_setsockopt(sock, SLK_SNDHWM, &hwm, sizeof(hwm));
    slk_setsockopt(sock, SLK_RCVHWM, &hwm, sizeof(hwm));
    slk_setsockopt(sock, SLK_SUBSCRIBE, "", 0);
    if (slk_bind(sock, TCP_ADDR) != 0) return;
    server_ready = true;
    char* buffer = (char*)malloc(size + 1024);
    slk_recv(sock, buffer, size + 1024, 0); // WAKEUP
    for (int i = 0; i < MESSAGES_COUNT; i++) {
        slk_recv(sock, buffer, size + 1024, 0);
    }
    free(buffer);
    slk_close(sock);
}

void run_client(slk_ctx_t* ctx, int size, double* result) {
    slk_socket_t* sock = slk_socket(ctx, SLK_PUB);
    int hwm = 0;
    slk_setsockopt(sock, SLK_SNDHWM, &hwm, sizeof(hwm));
    slk_setsockopt(sock, SLK_RCVHWM, &hwm, sizeof(hwm));
    while(!server_ready) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    slk_connect(sock, TCP_ADDR);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    slk_send(sock, "WAKEUP", 6, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    char* data = (char*)malloc(size);
    memset(data, 'A', size);
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < MESSAGES_COUNT; i++) {
        slk_send(sock, data, size, 0);
    }
    auto end = std::chrono::high_resolution_clock::now();
    *result = (double)MESSAGES_COUNT / std::chrono::duration<double>(end - start).count();
    free(data);
    slk_close(sock);
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    slk_ctx_t* ctx = slk_ctx_new();
    double result = 0;
    std::thread s(run_server, ctx, atoi(argv[1]));
    std::thread c(run_client, ctx, atoi(argv[1]), &result);
    c.join(); s.join();
    printf("%.2f\n", result);
    slk_ctx_destroy(ctx);
    return 0;
}

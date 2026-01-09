/* SPDX-License-Identifier: MPL-2.0 */
#include <serverlink/serverlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <chrono>
#include <atomic>

#define MESSAGES_COUNT 50000 
#define TCP_ADDR "tcp://127.0.0.1:18003"

std::atomic<bool> server_ready{false};

void run_server(slk_ctx_t* ctx, int mode, int size) {
    slk_socket_t* sock = slk_socket(ctx, SLK_ROUTER);
    int hwm = 0;
    slk_setsockopt(sock, SLK_SNDHWM, &hwm, sizeof(hwm));
    slk_setsockopt(sock, SLK_RCVHWM, &hwm, sizeof(hwm));
    slk_setsockopt(sock, SLK_ROUTING_ID, "SERVER", 6);
    if (slk_bind(sock, TCP_ADDR) != 0) return;
    server_ready = true;
    char id[256];
    char* buffer = (char*)malloc(size + 1024);
    int ilen = slk_recv(sock, id, sizeof(id), 0);
    slk_recv(sock, buffer, size + 1024, 0); // READY
    slk_send(sock, id, ilen, SLK_SNDMORE);
    slk_send(sock, "GO", 2, 0);
    int count = (mode == 0) ? MESSAGES_COUNT : 5000;
    for (int i = 0; i < count; i++) {
        ilen = slk_recv(sock, id, sizeof(id), 0);
        slk_recv(sock, buffer, size + 1024, 0);
        if (mode == 1) {
            slk_send(sock, id, ilen, SLK_SNDMORE);
            slk_send(sock, buffer, size, 0);
        }
    }
    free(buffer);
    slk_close(sock);
}

void run_client(slk_ctx_t* ctx, int mode, int size, double* result) {
    slk_socket_t* sock = slk_socket(ctx, SLK_DEALER);
    int hwm = 0;
    slk_setsockopt(sock, SLK_SNDHWM, &hwm, sizeof(hwm));
    slk_setsockopt(sock, SLK_RCVHWM, &hwm, sizeof(hwm));
    while(!server_ready) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    slk_connect(sock, TCP_ADDR);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    char tmp[256];
    slk_send(sock, "READY", 5, 0);
    slk_recv(sock, tmp, sizeof(tmp), 0); // GO
    char* data = (char*)malloc(size);
    memset(data, 'A', size);
    int count = (mode == 0) ? MESSAGES_COUNT : 5000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < count; i++) {
        slk_send(sock, data, size, 0);
        if (mode == 1) slk_recv(sock, data, size, 0);
    }
    auto end = std::chrono::high_resolution_clock::now();
    *result = (mode == 0) ? (double)count / std::chrono::duration<double>(end - start).count() : (std::chrono::duration<double>(end - start).count() * 1000000.0) / count;
    free(data);
    slk_close(sock);
}

int main(int argc, char** argv) {
    if (argc < 3) return 1;
    slk_ctx_t* ctx = slk_ctx_new();
    double result = 0;
    std::thread s(run_server, ctx, atoi(argv[2]), atoi(argv[1]));
    std::thread c(run_client, ctx, atoi(argv[2]), atoi(argv[1]), &result);
    c.join(); s.join();
    printf("%.2f\n", result);
    slk_ctx_destroy(ctx);
    return 0;
}

/* SPDX-License-Identifier: MPL-2.0 */
#include <serverlink/serverlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <chrono>
#include <atomic>

#define MESSAGES_COUNT 50000 
#define LATENCY_COUNT 5000
#define TCP_ADDR "tcp://127.0.0.1:18888"

std::atomic<bool> server_ready{false};
int global_msg_size = 64;

void run_server(slk_ctx_t* ctx, int type, int mode) {
    slk_socket_t* sock = slk_socket(ctx, type);
    int hwm = 0;
    slk_setsockopt(sock, SLK_SNDHWM, &hwm, sizeof(hwm));
    slk_setsockopt(sock, SLK_RCVHWM, &hwm, sizeof(hwm));
    
    if (type == SLK_ROUTER) slk_setsockopt(sock, SLK_ROUTING_ID, "SERVER", 6);

    if (slk_bind(sock, TCP_ADDR) != 0) return;
    if (type == SLK_SUB) slk_setsockopt(sock, SLK_SUBSCRIBE, "", 0);
    
    server_ready = true;
    char id[256];
    char* buffer = (char*)malloc(global_msg_size + 1024);

    // --- 1. Handshake (except PUB/SUB) ---
    if (type != SLK_PUB && type != SLK_XPUB) {
        // Recv READY
        if (type == SLK_ROUTER) {
            int ilen = slk_recv(sock, id, sizeof(id), 0);
            slk_recv(sock, buffer, global_msg_size + 1024, 0);
            slk_send(sock, id, ilen, SLK_SNDMORE);
        } else {
            slk_recv(sock, buffer, global_msg_size + 1024, 0);
        }
        // Send GO
        slk_send(sock, "GO", 2, 0);
    }

    // --- 2. Benchmark ---
    int count = (mode == 0) ? MESSAGES_COUNT : LATENCY_COUNT;
    for (int i = 0; i < count; i++) {
        int id_len = 0;
        if (type == SLK_ROUTER) {
            id_len = slk_recv(sock, id, sizeof(id), 0);
        }
        int data_len = slk_recv(sock, buffer, global_msg_size + 1024, 0);
        
        if (mode == 1 && type != SLK_SUB) {
            if (type == SLK_ROUTER) slk_send(sock, id, id_len, SLK_SNDMORE);
            slk_send(sock, buffer, data_len, 0);
        }
    }
    free(buffer);
    slk_close(sock);
}

void run_client(slk_ctx_t* ctx, int type, int mode, double* result) {
    slk_socket_t* sock = slk_socket(ctx, type);
    int hwm = 0;
    slk_setsockopt(sock, SLK_SNDHWM, &hwm, sizeof(hwm));
    slk_setsockopt(sock, SLK_RCVHWM, &hwm, sizeof(hwm));

    while(!server_ready) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    if (type == SLK_ROUTER) {
        slk_setsockopt(sock, SLK_ROUTING_ID, "CLIENT", 6);
        slk_setsockopt(sock, SLK_CONNECT_ROUTING_ID, "SERVER", 6);
    }
    slk_connect(sock, TCP_ADDR);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // --- 1. Handshake (except SUB) ---
    if (type != SLK_SUB && type != SLK_XSUB) {
        // Send READY
        if (type == SLK_ROUTER) slk_send(sock, "SERVER", 6, SLK_SNDMORE);
        slk_send(sock, "READY", 5, 0);
        
        // Recv GO
        char tmp[256];
        if (type == SLK_ROUTER) slk_recv(sock, tmp, sizeof(tmp), 0);
        slk_recv(sock, tmp, sizeof(tmp), 0);
    }

    // --- 2. Benchmark ---
    char* data = (char*)malloc(global_msg_size);
    memset(data, 'A', global_msg_size);
    int count = (mode == 0) ? MESSAGES_COUNT : LATENCY_COUNT;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < count; i++) {
        if (type == SLK_ROUTER) slk_send(sock, "SERVER", 6, SLK_SNDMORE);
        slk_send(sock, data, global_msg_size, 0);
        
        if (mode == 1 && type != SLK_PUB) {
            if (type == SLK_ROUTER) { char tid[256]; slk_recv(sock, tid, sizeof(tid), 0); }
            slk_recv(sock, data, global_msg_size, 0);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end - start).count();

    if (mode == 0) *result = (double)count / duration;
    else *result = (duration * 1000000.0) / count;

    free(data);
    slk_close(sock);
}

int main(int argc, char** argv) {
    if (argc < 5) return 1;
    int s_type = atoi(argv[1]);
    int c_type = atoi(argv[2]);
    global_msg_size = atoi(argv[3]);
    int mode = atoi(argv[4]);

    slk_ctx_t* ctx = slk_ctx_new();
    double result = 0;
    server_ready = false;

    std::thread s(run_server, ctx, s_type, mode);
    std::thread c(run_client, ctx, c_type, mode, &result);
    
    c.join(); s.join();
    printf("%.2f\n", result);
    slk_ctx_destroy(ctx);
    return 0;
}
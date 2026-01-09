/* SPDX-License-Identifier: MPL-2.0 */
#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <chrono>
#include <atomic>

#define MESSAGES_COUNT 50000 
#define TCP_ADDR "tcp://127.0.0.1:18008"

std::atomic<bool> server_ready{false};

void run_server(void* ctx, int size) {
    void* sock = zmq_socket(ctx, ZMQ_SUB);
    int hwm = 0;
    zmq_setsockopt(sock, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_setsockopt(sock, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
    if (zmq_bind(sock, TCP_ADDR) != 0) return;
    server_ready = true;
    char* buffer = (char*)malloc(size + 1024);
    zmq_recv(sock, buffer, size + 1024, 0); // WAKEUP
    for (int i = 0; i < MESSAGES_COUNT; i++) {
        zmq_recv(sock, buffer, size + 1024, 0);
    }
    free(buffer);
    zmq_close(sock);
}

void run_client(void* ctx, int size, double* result) {
    void* sock = zmq_socket(ctx, ZMQ_PUB);
    int hwm = 0;
    zmq_setsockopt(sock, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_setsockopt(sock, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    while(!server_ready) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    zmq_connect(sock, TCP_ADDR);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    zmq_send(sock, "WAKEUP", 6, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    char* data = (char*)malloc(size);
    memset(data, 'A', size);
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < MESSAGES_COUNT; i++) {
        zmq_send(sock, data, size, 0);
    }
    auto end = std::chrono::high_resolution_clock::now();
    *result = (double)MESSAGES_COUNT / std::chrono::duration<double>(end - start).count();
    free(data);
    zmq_close(sock);
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    void* ctx = zmq_ctx_new();
    double result = 0;
    std::thread s(run_server, ctx, atoi(argv[1]));
    std::thread c(run_client, ctx, atoi(argv[1]), &result);
    c.join(); s.join();
    printf("%.2f\n", result);
    zmq_ctx_term(ctx);
    return 0;
}

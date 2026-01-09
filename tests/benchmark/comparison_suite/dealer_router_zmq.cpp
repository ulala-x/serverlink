/* SPDX-License-Identifier: MPL-2.0 */
#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <chrono>
#include <atomic>

#define MESSAGES_COUNT 50000 
#define TCP_ADDR "tcp://127.0.0.1:18004"

std::atomic<bool> server_ready{false};

void run_server(void* ctx, int mode, int size) {
    void* sock = zmq_socket(ctx, ZMQ_ROUTER);
    int hwm = 0;
    zmq_setsockopt(sock, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_setsockopt(sock, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    zmq_setsockopt(sock, ZMQ_ROUTING_ID, "SERVER", 6);
    if (zmq_bind(sock, TCP_ADDR) != 0) return;
    server_ready = true;
    char id[256];
    char* buffer = (char*)malloc(size + 1024);
    int ilen = zmq_recv(sock, id, sizeof(id), 0);
    zmq_recv(sock, buffer, size + 1024, 0); // READY
    zmq_send(sock, id, ilen, ZMQ_SNDMORE);
    zmq_send(sock, "GO", 2, 0);
    int count = (mode == 0) ? MESSAGES_COUNT : 5000;
    for (int i = 0; i < count; i++) {
        ilen = zmq_recv(sock, id, sizeof(id), 0);
        zmq_recv(sock, buffer, size + 1024, 0);
        if (mode == 1) {
            zmq_send(sock, id, ilen, ZMQ_SNDMORE);
            zmq_send(sock, buffer, size, 0);
        }
    }
    free(buffer);
    zmq_close(sock);
}

void run_client(void* ctx, int mode, int size, double* result) {
    void* sock = zmq_socket(ctx, ZMQ_DEALER);
    int hwm = 0;
    zmq_setsockopt(sock, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_setsockopt(sock, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    while(!server_ready) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    zmq_connect(sock, TCP_ADDR);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    char tmp[256];
    zmq_send(sock, "READY", 5, 0);
    zmq_recv(sock, tmp, sizeof(tmp), 0); // GO
    char* data = (char*)malloc(size);
    memset(data, 'A', size);
    int count = (mode == 0) ? MESSAGES_COUNT : 5000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < count; i++) {
        zmq_send(sock, data, size, 0);
        if (mode == 1) zmq_recv(sock, data, size, 0);
    }
    auto end = std::chrono::high_resolution_clock::now();
    *result = (mode == 0) ? (double)count / std::chrono::duration<double>(end - start).count() : (std::chrono::duration<double>(end - start).count() * 1000000.0) / count;
    free(data);
    zmq_close(sock);
}

int main(int argc, char** argv) {
    if (argc < 3) return 1;
    void* ctx = zmq_ctx_new();
    double result = 0;
    std::thread s(run_server, ctx, atoi(argv[2]), atoi(argv[1]));
    std::thread c(run_client, ctx, atoi(argv[2]), atoi(argv[1]), &result);
    c.join(); s.join();
    printf("%.2f\n", result);
    zmq_ctx_term(ctx);
    return 0;
}

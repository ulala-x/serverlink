/* SPDX-License-Identifier: MPL-2.0 */
#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MESSAGES_COUNT 50000
#define LATENCY_COUNT 5000
#define TCP_ADDR "tcp://127.0.0.1:19999"

int server_ready = 0;
int test_failed = 0;
int global_msg_size = 64;

void* run_server(void* arg) {
    int* args = (int*)arg;
    int type = args[0];
    int mode = args[2];

    void* ctx = zmq_ctx_new();
    void* sock = zmq_socket(ctx, type);
    int hwm = 0;
    zmq_setsockopt(sock, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_setsockopt(sock, ZMQ_RCVHWM, &hwm, sizeof(hwm));

    if (type == ZMQ_ROUTER) zmq_setsockopt(sock, ZMQ_ROUTING_ID, "SERVER", 6);
    if (zmq_bind(sock, TCP_ADDR) != 0) { test_failed = 1; return NULL; }
    if (type == ZMQ_SUB) zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
    
    server_ready = 1;
    char id[256];
    int id_len = 0;
    char* buffer = (char*)malloc(global_msg_size + 1024);

    // --- Wait for READY Signal ---
    if (type != ZMQ_PUB) {
        if (type == ZMQ_ROUTER) {
            id_len = zmq_recv(sock, id, sizeof(id), 0);
        }
        zmq_recv(sock, buffer, global_msg_size + 1024, 0);
        if (type == ZMQ_ROUTER) zmq_send(sock, id, id_len, ZMQ_SNDMORE);
        zmq_send(sock, "GO", 2, 0);
    }

    int count = (mode == 0) ? MESSAGES_COUNT : LATENCY_COUNT;
    for (int i = 0; i < count; i++) {
        int cur_id_len = 0;
        if (type == ZMQ_ROUTER) cur_id_len = zmq_recv(sock, id, sizeof(id), 0);
        int data_len = zmq_recv(sock, buffer, global_msg_size + 1024, 0);
        if (mode == 1 && type != ZMQ_SUB) {
            if (type == ZMQ_ROUTER) zmq_send(sock, id, cur_id_len, ZMQ_SNDMORE);
            zmq_send(sock, buffer, data_len, 0);
        }
    }
    free(buffer);
    zmq_close(sock);
    zmq_ctx_term(ctx);
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 5) return 1;
    int s_type = atoi(argv[1]);
    int c_type = atoi(argv[2]);
    global_msg_size = atoi(argv[3]);
    int mode = atoi(argv[4]);
    int args[3] = {s_type, c_type, mode};

    server_ready = 0; test_failed = 0;
    pthread_t s_thread;
    pthread_create(&s_thread, NULL, run_server, args);
    while(!server_ready && !test_failed) usleep(10000); 
    if (test_failed) return 1;

    void* ctx = zmq_ctx_new();
    void* client_sock = zmq_socket(ctx, c_type);
    int hwm = 0;
    zmq_setsockopt(client_sock, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_setsockopt(client_sock, ZMQ_RCVHWM, &hwm, sizeof(hwm));

    if (c_type == ZMQ_ROUTER) {
        zmq_setsockopt(client_sock, ZMQ_ROUTING_ID, "CLIENT", 6);
        zmq_setsockopt(client_sock, ZMQ_CONNECT_ROUTING_ID, "SERVER", 6);
    }
    zmq_connect(client_sock, TCP_ADDR);
    usleep(200000);

    char tmp[256];
    // --- Send READY Signal ---
    if (c_type != ZMQ_SUB) {
        if (c_type == ZMQ_ROUTER) zmq_send(client_sock, "SERVER", 6, ZMQ_SNDMORE);
        zmq_send(client_sock, "READY", 5, 0);
        if (c_type == ZMQ_ROUTER) zmq_recv(client_sock, tmp, sizeof(tmp), 0);
        zmq_recv(client_sock, tmp, sizeof(tmp), 0);
    }

    char* data = (char*)malloc(global_msg_size);
    memset(data, 'A', global_msg_size);
    int count = (mode == 0) ? MESSAGES_COUNT : LATENCY_COUNT;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < count; i++) {
        if (c_type == ZMQ_ROUTER) zmq_send(client_sock, "SERVER", 6, ZMQ_SNDMORE);
        zmq_send(client_sock, data, global_msg_size, 0);
        if (mode == 1 && c_type != ZMQ_PUB) {
            if (c_type == ZMQ_ROUTER) zmq_recv(client_sock, tmp, sizeof(tmp), 0);
            zmq_recv(client_sock, data, global_msg_size, 0);
        }
    }
    pthread_join(s_thread, NULL);
    clock_gettime(CLOCK_MONOTONIC, &end);

    double duration = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    if (mode == 0) printf("%.2f\n", (double)count / duration);
    else printf("%.2f\n", (duration * 1000000.0) / count);

    free(data);
    zmq_close(client_sock);
    zmq_ctx_term(ctx);
    return 0;
}

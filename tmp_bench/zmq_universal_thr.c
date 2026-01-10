#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MESSAGES_COUNT 50000
#define TCP_ADDR "tcp://127.0.0.1:7777"

int global_msg_size = 64;

void* server_routine(void* arg) {
    int type = *(int*)arg;
    void* ctx = zmq_ctx_new();
    void* sock = zmq_socket(ctx, type);
    zmq_bind(sock, TCP_ADDR);
    if (type == ZMQ_SUB || type == ZMQ_XSUB) zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
    char* buffer = malloc(global_msg_size + 256);
    for (int i = 0; i < MESSAGES_COUNT; i++) {
        if (type == ZMQ_ROUTER) zmq_recv(sock, buffer, global_msg_size + 256, 0);
        zmq_recv(sock, buffer, global_msg_size + 256, 0);
    }
    free(buffer);
    zmq_close(sock);
    zmq_ctx_term(ctx);
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 3) return 1;
    int type = atoi(argv[1]);
    const char* name = argv[2];
    if (argc >= 4) global_msg_size = atoi(argv[3]);

    void* ctx = zmq_ctx_new();
    
    pthread_t s_thread;
    int s_type = type;
    if (type == ZMQ_PUB) s_type = ZMQ_SUB;
    else if (type == ZMQ_XPUB) s_type = ZMQ_XSUB;
    else if (type == ZMQ_DEALER) s_type = ZMQ_ROUTER;
    
    pthread_create(&s_thread, NULL, server_routine, &s_type);
    usleep(200000);

    void* c_sock = zmq_socket(ctx, type);
    if (type == ZMQ_ROUTER) zmq_setsockopt(c_sock, ZMQ_ROUTING_ID, "CLIENT", 6);
    zmq_connect(c_sock, TCP_ADDR);
    usleep(200000);

    char* data = malloc(global_msg_size);
    memset(data, 'A', global_msg_size);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < MESSAGES_COUNT; i++) {
        if (type == ZMQ_ROUTER) zmq_send(c_sock, "SERVER", 6, ZMQ_SNDMORE);
        zmq_send(c_sock, data, global_msg_size, 0);
    }
    pthread_join(s_thread, NULL);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    printf("%s Throughput (%d bytes): %.0f msg/s\n", name, global_msg_size, (double)MESSAGES_COUNT / duration);
    free(data);
    zmq_close(c_sock);
    zmq_ctx_term(ctx);
    return 0;
}
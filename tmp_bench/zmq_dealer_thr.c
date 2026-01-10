/* libzmq DEALER-ROUTER Throughput Benchmark */

#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MESSAGES_COUNT 100000
#define MESSAGE_SIZE 64
#define ENDPOINT "tcp://127.0.0.1:5556"

void* server_routine(void* ctx) {
    void* sock = zmq_socket(ctx, ZMQ_ROUTER);
    zmq_bind(sock, ENDPOINT);

    char* buffer = (char*)malloc(MESSAGE_SIZE + 256);
    
    for (int i = 0; i < MESSAGES_COUNT; i++) {
        zmq_recv(sock, buffer, MESSAGE_SIZE + 256, 0); // ID
        zmq_recv(sock, buffer, MESSAGE_SIZE + 256, 0); // Data
    }

    free(buffer);
    zmq_close(sock);
    return NULL;
}

int main() {
    printf("libzmq DEALER-ROUTER Throughput Benchmark\n");
    printf("Message size: %d [B], Count: %d\n", MESSAGE_SIZE, MESSAGES_COUNT);

    void* ctx = zmq_ctx_new();
    
    pthread_t s_thread;
    pthread_create(&s_thread, NULL, server_routine, ctx);
    usleep(100000);

    void* client_sock = zmq_socket(ctx, ZMQ_DEALER);
    zmq_connect(client_sock, ENDPOINT);
    usleep(200000);

    char* data = (char*)malloc(MESSAGE_SIZE);
    memset(data, 'A', MESSAGE_SIZE);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < MESSAGES_COUNT; i++) {
        zmq_send(client_sock, data, MESSAGE_SIZE, 0);
    }

    pthread_join(s_thread, NULL);
    clock_gettime(CLOCK_MONOTONIC, &end);

    double duration = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput = (double)MESSAGES_COUNT / duration;
    printf("Throughput: %.0f [msg/s]\n", throughput);

    free(data);
    zmq_close(client_sock);
    zmq_ctx_term(ctx);
    return 0;
}

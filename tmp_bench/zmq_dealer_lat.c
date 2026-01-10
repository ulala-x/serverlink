/* libzmq DEALER-ROUTER Latency Benchmark */

#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define ROUNDTRIPS 10000
#define MESSAGE_SIZE 64
#define ENDPOINT "tcp://127.0.0.1:5558"

void* server_routine(void* ctx) {
    void* sock = zmq_socket(ctx, ZMQ_ROUTER);
    zmq_bind(sock, ENDPOINT);

    char id[256];
    char data[MESSAGE_SIZE];
    
    for (int i = 0; i < ROUNDTRIPS; i++) {
        int id_len = zmq_recv(sock, id, sizeof(id), 0);
        int data_len = zmq_recv(sock, data, sizeof(data), 0);
        
        zmq_send(sock, id, id_len, ZMQ_SNDMORE);
        zmq_send(sock, data, data_len, 0);
    }

    zmq_close(sock);
    return NULL;
}

int main() {
    printf("libzmq DEALER-ROUTER Latency Benchmark\n");
    printf("Message size: %d [B], Roundtrips: %d\n", MESSAGE_SIZE, ROUNDTRIPS);

    void* ctx = zmq_ctx_new();
    
    pthread_t s_thread;
    pthread_create(&s_thread, NULL, server_routine, ctx);
    usleep(100000);

    void* client_sock = zmq_socket(ctx, ZMQ_DEALER);
    zmq_connect(client_sock, ENDPOINT);
    usleep(200000);

    char data[MESSAGE_SIZE];
    memset(data, 'A', MESSAGE_SIZE);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ROUNDTRIPS; i++) {
        zmq_send(client_sock, data, MESSAGE_SIZE, 0);
        zmq_recv(client_sock, data, MESSAGE_SIZE, 0);
    }

    pthread_join(s_thread, NULL);
    clock_gettime(CLOCK_MONOTONIC, &end);

    double total_time = (double)(end.tv_sec - start.tv_sec) * 1e6 + (double)(end.tv_nsec - start.tv_nsec) / 1e3;
    printf("Average Latency: %.2f [us]\n", total_time / ROUNDTRIPS);

    zmq_close(client_sock);
    zmq_ctx_term(ctx);
    return 0;
}

/* SPDX-License-Identifier: MPL-2.0 */

#include <serverlink/serverlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <chrono>
#include <atomic>

#define MESSAGES_COUNT 1000 
#define TCP_ADDR "tcp://127.0.0.1:6666"

int global_msg_size = 64;
std::atomic<bool> server_ready{false};

void run_server(slk_ctx_t* ctx, int server_type, int client_type) {
    slk_socket_t* sock = slk_socket(ctx, server_type);
    
    int hwm = 0;
    slk_setsockopt(sock, SLK_SNDHWM, &hwm, sizeof(hwm));
    slk_setsockopt(sock, SLK_RCVHWM, &hwm, sizeof(hwm));

    int rc = -1;
    for (int i = 0; i < 10; i++) {
        rc = slk_bind(sock, TCP_ADDR);
        if (rc == 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    if (rc != 0) {
        printf("Server: Bind failed\n");
        slk_close(sock);
        return;
    }

    server_ready = true;

    char* buffer = (char*)malloc(global_msg_size + 1024);
    for (int i = 0; i < MESSAGES_COUNT; i++) {
        if (server_type == SLK_ROUTER) {
            // 1. Receive sender's identity
            if (slk_recv(sock, buffer, global_msg_size + 1024, 0) < 0) {
                printf("Server error recv ID at %d\n", i);
                break;
            }
            
            // 2. If client is also ROUTER, it sent an extra routing frame
            if (client_type == SLK_ROUTER) {
                if (slk_recv(sock, buffer, global_msg_size + 1024, 0) < 0) {
                    printf("Server error recv extra frame at %d\n", i);
                    break;
                }
            }
        }
        // 3. Receive the actual data
        if (slk_recv(sock, buffer, global_msg_size + 1024, 0) < 0) {
            printf("Server error recv data at %d\n", i);
            break;
        }
    }
    free(buffer);
    slk_close(sock);
}

void run_client(slk_ctx_t* ctx, int client_type, double* duration) {
    slk_socket_t* sock = slk_socket(ctx, client_type);
    
    int hwm = 0;
    slk_setsockopt(sock, SLK_SNDHWM, &hwm, sizeof(hwm));
    slk_setsockopt(sock, SLK_RCVHWM, &hwm, sizeof(hwm));

    while(!server_ready) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (slk_connect(sock, TCP_ADDR) != 0) {
        printf("Client: Connect failed\n");
        slk_close(sock);
        return;
    }

    if (client_type == SLK_ROUTER) {
        slk_setsockopt(sock, SLK_CONNECT_ROUTING_ID, "SERVER", 6);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    char* data = (char*)malloc(global_msg_size);
    memset(data, 'A', global_msg_size);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < MESSAGES_COUNT; i++) {
        if (client_type == SLK_ROUTER) {
            if (slk_send(sock, "SERVER", 6, SLK_SNDMORE) < 0) {
                printf("Client error send target ID at %d\n", i);
                break;
            }
        }
        if (slk_send(sock, data, global_msg_size, 0) < 0) {
            printf("Client error send data at %d\n", i);
            break;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    *duration = std::chrono::duration<double>(end - start).count();

    free(data);
    slk_close(sock);
}

int main(int argc, char** argv) {
    if (argc < 3) return 1;
    int client_type = atoi(argv[1]);
    const char* name = argv[2];
    if (argc >= 4) global_msg_size = atoi(argv[3]);

    slk_ctx_t* ctx = slk_ctx_new();
    double duration = 0;

    int server_type = SLK_ROUTER;
    if (client_type == SLK_PAIR) server_type = SLK_PAIR;
    if (client_type == SLK_PUB) server_type = SLK_SUB;

    server_ready = false;
    std::thread s(run_server, ctx, server_type, client_type);
    std::thread c(run_client, ctx, client_type, &duration);
    
    c.join(); s.join();

    if (duration > 0) {
        printf("%s Throughput (%d bytes): %.0f msg/s\n", name, global_msg_size, (double)MESSAGES_COUNT / duration);
    }
    
    slk_ctx_destroy(ctx);
    return 0;
}
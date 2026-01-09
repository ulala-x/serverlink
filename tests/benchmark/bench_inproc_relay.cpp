/* SPDX-License-Identifier: MPL-2.0 */

#include <serverlink/serverlink.h>
#include <thread>
#include <vector>
#include <string>
#include <stdio.h>
#include <assert.h>
#include <atomic>
#include <chrono>

#define NUM_CLIENTS 10
#define MESSAGES_PER_CLIENT 50000
#define BRIDGE_ADDR "inproc://internal-bridge"
#define BACKEND_ADDR "inproc://real-backend"

std::atomic<long> total_received{0};

void client_thread(slk_ctx_t* ctx, int id) {
    slk_socket_t* sock = slk_socket(ctx, SLK_PAIR);
    slk_connect(sock, BRIDGE_ADDR);

    for (int i = 0; i < MESSAGES_PER_CLIENT; ++i) {
        std::string payload = "Msg-" + std::to_string(id) + "-" + std::to_string(i);
        slk_send(sock, payload.c_str(), payload.size(), 0);

        slk_msg_t* msg = slk_msg_new();
        if (slk_msg_recv(msg, sock, 0) >= 0) {
            total_received++;
        }
        slk_msg_destroy(msg);
    }
    slk_close(sock);
}

void bridge_thread(slk_ctx_t* ctx) {
    slk_socket_t* frontend = slk_socket(ctx, SLK_ROUTER);
    slk_bind(frontend, BRIDGE_ADDR);

    // Use PAIR instead of DEALER since DEALER is not implemented
    slk_socket_t* backend = slk_socket(ctx, SLK_PAIR);
    slk_connect(backend, BACKEND_ADDR);

    slk_pollitem_t items[2];
    items[0].socket = frontend; items[0].events = SLK_POLLIN;
    items[1].socket = backend;  items[1].events = SLK_POLLIN;

    while (total_received < NUM_CLIENTS * MESSAGES_PER_CLIENT) {
        int rc = slk_poll(items, 2, 100); // Increased timeout slightly
        if (rc < 0) break;
        
        if (items[0].revents & SLK_POLLIN) {
            // Frontend(ROUTER) receives: Identity + Data
            slk_msg_t* id = slk_msg_new();
            slk_msg_t* data = slk_msg_new();
            
            if (slk_msg_recv(id, frontend, 0) >= 0) {
                // Ensure we read the data part too
                if (slk_msg_recv(data, frontend, 0) >= 0) {
                    // Relay to Backend(PAIR): Send Identity as first frame, Data as second
                    slk_msg_send(id, backend, SLK_SNDMORE);
                    slk_msg_send(data, backend, 0);
                }
            }
            slk_msg_destroy(id);
            slk_msg_destroy(data);
        }
        
        if (items[1].revents & SLK_POLLIN) {
            // Backend(PAIR) receives: Identity + Reply
            slk_msg_t* id = slk_msg_new();
            slk_msg_t* reply = slk_msg_new();
            
            if (slk_msg_recv(id, backend, 0) >= 0) {
                if (slk_msg_recv(reply, backend, 0) >= 0) {
                    // Relay to Frontend(ROUTER): Identity used for routing
                    slk_msg_send(id, frontend, SLK_SNDMORE);
                    slk_msg_send(reply, frontend, 0);
                }
            }
            slk_msg_destroy(id);
            slk_msg_destroy(reply);
        }
    }
    slk_close(frontend);
    slk_close(backend);
}

void backend_thread(slk_ctx_t* ctx) {
    // Use PAIR for backend to match bridge's PAIR
    slk_socket_t* sock = slk_socket(ctx, SLK_PAIR);
    slk_bind(sock, BACKEND_ADDR);

    while (total_received < NUM_CLIENTS * MESSAGES_PER_CLIENT) {
        slk_msg_t* id = slk_msg_new();
        slk_msg_t* data = slk_msg_new();
        
        // PAIR socket receives frames sequentially
        if (slk_msg_recv(id, sock, 0) >= 0) {
            if (slk_msg_recv(data, sock, 0) >= 0) {
                // Echo back: Identity first, then Data
                slk_msg_send(id, sock, SLK_SNDMORE);
                slk_msg_send(data, sock, 0);
            }
        }
        slk_msg_destroy(id);
        slk_msg_destroy(data);
    }
    slk_close(sock);
}

int main() {
    printf("Starting High-Load Multi-Thread Inproc Relay Benchmark (ROUTER-PAIR-PAIR)...");
    slk_ctx_t* ctx = slk_ctx_new();

    auto start = std::chrono::high_resolution_clock::now();

    std::thread bridge(bridge_thread, ctx);
    std::thread backend(backend_thread, ctx);
    
    std::vector<std::thread> clients;
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        clients.emplace_back(client_thread, ctx, i);
    }

    for (auto& t : clients) t.join();
    bridge.join();
    backend.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    printf("Benchmark Finished.\n");
    printf("Total Messages Relayed: %ld\n", total_received.load());
    printf("Total Time: %.3f sec\n", diff.count());
    printf("Throughput: %.0f msg/sec\n", total_received.load() / diff.count());

    slk_ctx_destroy(ctx);
    return 0;
}

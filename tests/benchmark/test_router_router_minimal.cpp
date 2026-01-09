/* SPDX-License-Identifier: MPL-2.0 */
#include <serverlink/serverlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <chrono>
#include <assert.h>

#define ADDR "tcp://127.0.0.1:12345"

void server() {
    slk_ctx_t *ctx = slk_ctx_new();
    slk_socket_t *sock = slk_socket(ctx, SLK_ROUTER);
    slk_setsockopt(sock, SLK_ROUTING_ID, "SERVER", 6);
    
    printf("[S] Binding...\n");
    assert(slk_bind(sock, ADDR) == 0);
    
    char id[256];
    char data[256];
    
    printf("[S] Waiting for ID...\n");
    int rc = slk_recv(sock, id, sizeof(id), 0);
    printf("[S] Recv ID rc=%d (%.*s)\n", rc, rc, id);
    
    printf("[S] Waiting for Data...\n");
    rc = slk_recv(sock, data, sizeof(data), 0);
    printf("[S] Recv Data rc=%d (%.*s)\n", rc, rc, data);
    
    slk_close(sock);
    slk_ctx_destroy(ctx);
}

void client() {
    slk_ctx_t *ctx = slk_ctx_new();
    slk_socket_t *sock = slk_socket(ctx, SLK_ROUTER);
    slk_setsockopt(sock, SLK_ROUTING_ID, "CLIENT", 6);
    slk_setsockopt(sock, SLK_CONNECT_ROUTING_ID, "SERVER", 6);
    
    printf("[C] Connecting...\n");
    assert(slk_connect(sock, ADDR) == 0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    printf("[C] Sending...\n");
    slk_send(sock, "SERVER", 6, SLK_SNDMORE);
    slk_send(sock, "HELLO", 5, 0);
    printf("[C] Sent.\n");
    
    slk_close(sock);
    slk_ctx_destroy(ctx);
}

int main() {
    std::thread s(server);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::thread c(client);
    s.join();
    c.join();
    return 0;
}

/* worker_node.c: ROUTER peer for long-term testing */
#include <serverlink/serverlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <endpoint> [identity]\n", argv[0]);
        return 1;
    }
    const char *endpoint = argv[1];
    char identity[256];
    if (argc >= 3) {
        strncpy(identity, argv[2], sizeof(identity));
    } else {
        if (gethostname(identity, sizeof(identity)) != 0) {
            snprintf(identity, sizeof(identity), "worker-%d", getpid());
        }
    }

    void *ctx = slk_ctx_new();
    void *socket = slk_socket(ctx, SLK_ROUTER);

    int mandatory = 1;
    slk_setsockopt(socket, SLK_ROUTER_MANDATORY, &mandatory, sizeof(mandatory));
    slk_setsockopt(socket, SLK_ROUTING_ID, identity, strlen(identity));

    if (slk_connect(socket, endpoint) != 0) {
        fprintf(stderr, "Failed to connect to %s\n", endpoint);
        return 1;
    }

    printf("Worker [%s] connected to %s, waiting for connection to stabilize...\n", identity, endpoint);
    fflush(stdout);
    sleep(2); // Give it time to connect

    char body[64];
    memset(body, 'X', sizeof(body));
    
    long long sent_count = 0;
    while (1) {
        const char *master_id = "MASTER";
        
        int rc_send = slk_send(socket, master_id, strlen(master_id), SLK_SNDMORE);
        if (rc_send < 0) {
            usleep(100000);
            continue;
        }
        slk_send(socket, body, sizeof(body), 0);

        char recv_id[256];
        char recv_body[1024];
        
        // Receive echo reply
        int rc_recv = slk_recv(socket, recv_id, sizeof(recv_id), 0);
        if (rc_recv >= 0) {
            slk_recv(socket, recv_body, sizeof(recv_body), 0);
            sent_count++;
        }

        if (sent_count % 1000 == 0) {
            printf("Worker [%s] processed %lld echoes...\n", identity, sent_count);
            fflush(stdout);
        }

        // Small sleep to avoid absolute CPU saturation in 1000 connections
        usleep(10000); // 10ms
    }

    slk_close(socket);
    slk_ctx_destroy(ctx);
    return 0;
}
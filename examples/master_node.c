/* master_node.c: ROUTER hub for long-term testing */
#include <serverlink/serverlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    const char *endpoint = (argc > 1) ? argv[1] : "tcp://*:5555";
    void *ctx = slk_ctx_new();
    void *socket = slk_socket(ctx, SLK_ROUTER);

    const char *master_id = "MASTER";
    slk_setsockopt(socket, SLK_ROUTING_ID, master_id, strlen(master_id));

    if (slk_bind(socket, endpoint) != 0) {
        fprintf(stderr, "Failed to bind to %s\n", endpoint);
        return 1;
    }

    printf("Master node started on %s\n", endpoint);
    fflush(stdout);

    long long msg_count = 0;
    while (1) {
        char identity[256];
        char body[1024];
        
        // Receive identity
        int rc = slk_recv(socket, identity, sizeof(identity), 0);
        if (rc < 0) {
            continue;
        }
        int id_len = rc;
        identity[id_len] = '\0';

        // Receive body
        rc = slk_recv(socket, body, sizeof(body), 0);
        if (rc < 0) {
            continue;
        }

        if (msg_count == 0) {
            printf("Received first message from [%s]\n", identity);
            fflush(stdout);
        }

        msg_count++;
        if (msg_count % 10000 == 0) {
            printf("Total messages received: %lld\n", msg_count);
            fflush(stdout);
        }

        // Echo back to sender
        slk_send(socket, identity, id_len, SLK_SNDMORE);
        slk_send(socket, body, rc, 0);
    }

    slk_close(socket);
    slk_ctx_destroy(ctx);
    return 0;
}
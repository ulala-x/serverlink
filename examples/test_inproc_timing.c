/* Test for inproc timing bug where messages are not readable after flush */

#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("=== Testing inproc timing bug ===\n\n");

    /* Create context */
    slk_ctx_t *ctx = slk_ctx_new();
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    /* Create server ROUTER socket and bind to inproc */
    slk_socket_t *server = slk_socket(ctx, SLK_ROUTER);
    if (!server) {
        fprintf(stderr, "Failed to create server socket\n");
        slk_ctx_destroy(ctx);
        return 1;
    }

    const char *server_id = "SERVER";
    if (slk_setsockopt(server, SLK_ROUTING_ID, server_id, strlen(server_id)) != 0) {
        fprintf(stderr, "Failed to set server routing ID\n");
        slk_close(server);
        slk_ctx_destroy(ctx);
        return 1;
    }

    printf("Binding server to inproc://test...\n");
    if (slk_bind(server, "inproc://test") != 0) {
        fprintf(stderr, "Failed to bind server: %s\n", slk_strerror(slk_errno()));
        slk_close(server);
        slk_ctx_destroy(ctx);
        return 1;
    }

    /* Create client ROUTER socket and connect to inproc */
    slk_socket_t *client = slk_socket(ctx, SLK_ROUTER);
    if (!client) {
        fprintf(stderr, "Failed to create client socket\n");
        slk_close(server);
        slk_ctx_destroy(ctx);
        return 1;
    }

    const char *client_id = "CLIENT";
    if (slk_setsockopt(client, SLK_ROUTING_ID, client_id, strlen(client_id)) != 0) {
        fprintf(stderr, "Failed to set client routing ID\n");
        slk_close(client);
        slk_close(server);
        slk_ctx_destroy(ctx);
        return 1;
    }

    printf("Connecting client to inproc://test...\n");
    if (slk_connect(client, "inproc://test") != 0) {
        fprintf(stderr, "Failed to connect client: %s\n", slk_strerror(slk_errno()));
        slk_close(client);
        slk_close(server);
        slk_ctx_destroy(ctx);
        return 1;
    }

    /* Send message from client to server - NO SLEEP! This is the test. */
    printf("\nSending message from client to server (no delay)...\n");
    const char *msg = "Hello";

    /* ROUTER sends: [Routing ID][Empty][Payload] */
    int rc = slk_send(client, server_id, strlen(server_id), SLK_SNDMORE);
    if (rc < 0) {
        fprintf(stderr, "Failed to send routing ID: %s\n", slk_strerror(slk_errno()));
        goto cleanup;
    }

    rc = slk_send(client, "", 0, SLK_SNDMORE);
    if (rc < 0) {
        fprintf(stderr, "Failed to send delimiter: %s\n", slk_strerror(slk_errno()));
        goto cleanup;
    }

    rc = slk_send(client, msg, strlen(msg), 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to send payload: %s\n", slk_strerror(slk_errno()));
        goto cleanup;
    }

    printf("Message sent!\n");

    /* Poll server immediately - this is where the bug shows up */
    printf("\nPolling server for message...\n");
    slk_pollitem_t items[] = {{server, -1, SLK_POLLIN, 0}};
    rc = slk_poll(items, 1, 2000);  /* 2 second timeout */

    if (rc <= 0) {
        fprintf(stderr, "ERROR: Poll timeout or error (rc=%d)\n", rc);
        fprintf(stderr, "Server not readable!\n");
        fprintf(stderr, "\nBUG REPRODUCED: Message was sent but not readable\n");
        goto cleanup;
    }

    if (!(items[0].revents & SLK_POLLIN)) {
        fprintf(stderr, "ERROR: Server not readable!\n");
        fprintf(stderr, "\nBUG REPRODUCED: Message was sent but not readable\n");
        goto cleanup;
    }

    /* Receive the message */
    char identity[256];
    char delimiter[256];
    char payload[256];

    rc = slk_recv(server, identity, sizeof(identity), 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to receive identity: %s\n", slk_strerror(slk_errno()));
        goto cleanup;
    }
    identity[rc] = '\0';

    rc = slk_recv(server, delimiter, sizeof(delimiter), 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to receive delimiter: %s\n", slk_strerror(slk_errno()));
        goto cleanup;
    }

    rc = slk_recv(server, payload, sizeof(payload), 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to receive payload: %s\n", slk_strerror(slk_errno()));
        goto cleanup;
    }
    payload[rc] = '\0';

    printf("Server received:\n");
    printf("  Identity: %s\n", identity);
    printf("  Payload:  %s\n", payload);

    if (strcmp(payload, "Hello") == 0) {
        printf("\n=== SUCCESS: Message received correctly! ===\n");
    } else {
        printf("\n=== ERROR: Wrong message received ===\n");
    }

cleanup:
    slk_close(client);
    slk_close(server);
    slk_ctx_destroy(ctx);
    return 0;
}

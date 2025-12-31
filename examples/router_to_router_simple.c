/* ServerLink Simple Router-to-Router Example */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

/*
 * Simple Router-to-Router Example
 *
 * This example demonstrates basic Router-to-Router communication:
 * - Server ROUTER socket binds to tcp://127.0.0.1:5555
 * - Client ROUTER socket connects to the server
 * - Client sends a message to server
 * - Server receives and replies back
 * - Both sockets have routing IDs set
 *
 * ROUTER message format:
 *   [Routing ID][Empty delimiter][Payload]
 */

int main()
{
    printf("=== ServerLink Router-to-Router Simple Example ===\n\n");

    /* Get ServerLink version */
    int major, minor, patch;
    slk_version(&major, &minor, &patch);
    printf("ServerLink version: %d.%d.%d\n\n", major, minor, patch);

    /* Create context */
    printf("Creating context...\n");
    slk_ctx_t *ctx = slk_ctx_new();
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    /* Create SERVER socket */
    printf("Creating server ROUTER socket...\n");
    slk_socket_t *server = slk_socket(ctx, SLK_ROUTER);
    if (!server) {
        fprintf(stderr, "Failed to create server socket\n");
        slk_ctx_destroy(ctx);
        return 1;
    }

    /* Set server routing ID */
    const char *server_id = "SERVER";
    if (slk_setsockopt(server, SLK_ROUTING_ID, server_id, strlen(server_id)) != 0) {
        fprintf(stderr, "Failed to set server routing ID\n");
        slk_close(server);
        slk_ctx_destroy(ctx);
        return 1;
    }
    printf("  Server routing ID set to: %s\n", server_id);

    /* Bind server */
    const char *endpoint = "tcp://127.0.0.1:5555";
    printf("Binding server to %s...\n", endpoint);
    if (slk_bind(server, endpoint) != 0) {
        fprintf(stderr, "Failed to bind server: %s\n", slk_strerror(slk_errno()));
        slk_close(server);
        slk_ctx_destroy(ctx);
        return 1;
    }
    printf("  Server bound successfully\n\n");

    /* Create CLIENT socket */
    printf("Creating client ROUTER socket...\n");
    slk_socket_t *client = slk_socket(ctx, SLK_ROUTER);
    if (!client) {
        fprintf(stderr, "Failed to create client socket\n");
        slk_close(server);
        slk_ctx_destroy(ctx);
        return 1;
    }

    /* Set client routing ID */
    const char *client_id = "CLIENT";
    if (slk_setsockopt(client, SLK_ROUTING_ID, client_id, strlen(client_id)) != 0) {
        fprintf(stderr, "Failed to set client routing ID\n");
        slk_close(client);
        slk_close(server);
        slk_ctx_destroy(ctx);
        return 1;
    }
    printf("  Client routing ID set to: %s\n", client_id);

    /* Connect client */
    printf("Connecting client to %s...\n", endpoint);
    if (slk_connect(client, endpoint) != 0) {
        fprintf(stderr, "Failed to connect client: %s\n", slk_strerror(slk_errno()));
        slk_close(client);
        slk_close(server);
        slk_ctx_destroy(ctx);
        return 1;
    }
    printf("  Client connected successfully\n\n");

    /* Wait for connection to establish */
    printf("Waiting for connection to establish...\n");
    sleep_ms(200);

    /* ===== CLIENT SENDS TO SERVER ===== */
    printf("\n--- Client -> Server ---\n");
    const char *client_msg = "Hello from CLIENT!";

    /* ROUTER sends 3-part message: [Routing ID][Empty][Payload] */
    printf("Client sending:\n");
    printf("  Frame 1 (Routing ID): %s\n", server_id);
    printf("  Frame 2 (Delimiter):  (empty)\n");
    printf("  Frame 3 (Payload):    %s\n", client_msg);

    /* Send routing ID */
    int rc = slk_send(client, server_id, strlen(server_id), SLK_SNDMORE);
    if (rc < 0) {
        fprintf(stderr, "Failed to send routing ID\n");
        goto cleanup;
    }

    /* Send empty delimiter */
    rc = slk_send(client, "", 0, SLK_SNDMORE);
    if (rc < 0) {
        fprintf(stderr, "Failed to send delimiter\n");
        goto cleanup;
    }

    /* Send payload */
    rc = slk_send(client, client_msg, strlen(client_msg), 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to send payload\n");
        goto cleanup;
    }

    printf("Message sent successfully!\n");

    /* ===== SERVER RECEIVES ===== */
    printf("\n--- Server Receiving ---\n");
    sleep_ms(100);

    /* Poll for incoming message */
    slk_pollitem_t items[] = {{server, -1, SLK_POLLIN, 0}};
    rc = slk_poll(items, 1, 2000);
    if (rc <= 0 || !(items[0].revents & SLK_POLLIN)) {
        fprintf(stderr, "No message received by server (timeout)\n");
        goto cleanup;
    }

    /* Receive 3-part message */
    char identity[256];
    char delimiter[256];
    char payload[256];

    /* Receive routing ID (client's ID) */
    rc = slk_recv(server, identity, sizeof(identity), 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to receive identity\n");
        goto cleanup;
    }
    identity[rc] = '\0';

    /* Receive delimiter */
    rc = slk_recv(server, delimiter, sizeof(delimiter), 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to receive delimiter\n");
        goto cleanup;
    }

    /* Receive payload */
    rc = slk_recv(server, payload, sizeof(payload), 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to receive payload\n");
        goto cleanup;
    }
    payload[rc] = '\0';

    printf("Server received:\n");
    printf("  Frame 1 (Identity):  %s\n", identity);
    printf("  Frame 2 (Delimiter): (empty, %d bytes)\n", 0);
    printf("  Frame 3 (Payload):   %s\n", payload);

    /* ===== SERVER SENDS REPLY ===== */
    printf("\n--- Server -> Client ---\n");
    const char *server_msg = "Hello from SERVER!";

    printf("Server sending reply:\n");
    printf("  Frame 1 (Routing ID): %s\n", identity);
    printf("  Frame 2 (Delimiter):  (empty)\n");
    printf("  Frame 3 (Payload):    %s\n", server_msg);

    /* Send to the identity we received */
    rc = slk_send(server, identity, strlen(identity), SLK_SNDMORE);
    if (rc < 0) {
        fprintf(stderr, "Failed to send routing ID\n");
        goto cleanup;
    }

    rc = slk_send(server, "", 0, SLK_SNDMORE);
    if (rc < 0) {
        fprintf(stderr, "Failed to send delimiter\n");
        goto cleanup;
    }

    rc = slk_send(server, server_msg, strlen(server_msg), 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to send payload\n");
        goto cleanup;
    }

    printf("Reply sent successfully!\n");

    /* ===== CLIENT RECEIVES REPLY ===== */
    printf("\n--- Client Receiving ---\n");
    sleep_ms(100);

    /* Poll for reply */
    slk_pollitem_t client_items[] = {{client, -1, SLK_POLLIN, 0}};
    rc = slk_poll(client_items, 1, 2000);
    if (rc <= 0 || !(client_items[0].revents & SLK_POLLIN)) {
        fprintf(stderr, "No reply received by client (timeout)\n");
        goto cleanup;
    }

    /* Receive 3-part reply */
    rc = slk_recv(client, identity, sizeof(identity), 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to receive identity\n");
        goto cleanup;
    }
    identity[rc] = '\0';

    rc = slk_recv(client, delimiter, sizeof(delimiter), 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to receive delimiter\n");
        goto cleanup;
    }

    rc = slk_recv(client, payload, sizeof(payload), 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to receive payload\n");
        goto cleanup;
    }
    payload[rc] = '\0';

    printf("Client received:\n");
    printf("  Frame 1 (Identity):  %s\n", identity);
    printf("  Frame 2 (Delimiter): (empty)\n");
    printf("  Frame 3 (Payload):   %s\n", payload);

    printf("\n=== Communication Successful! ===\n");

cleanup:
    /* Cleanup */
    printf("\nCleaning up...\n");
    slk_close(client);
    slk_close(server);
    slk_ctx_destroy(ctx);

    printf("Done.\n");
    return 0;
}

/* ServerLink SPOT PUB/SUB - MMORPG Cell-Based Pub/Sub Example
 *
 * This example demonstrates SPOT's location transparency in a game server scenario.
 *
 * Scenario:
 *   - Game world is divided into cells (grid-based spatial partitioning)
 *   - This game server manages zone1: cell(5,7) and cell(5,8) (LOCAL)
 *   - Adjacent cell(6,7) is managed by another server (REMOTE)
 *   - When a player moves, events are broadcast to adjacent cells
 *   - Servers subscribe to adjacent cells to receive player events
 *
 * Key Features Demonstrated:
 *   1. Location Transparency:
 *      - Same API for publishing to local and remote cells
 *      - Receiver doesn't know if message came from local or remote
 *
 *   2. Spatial Interest Management:
 *      - Cells subscribe to adjacent cells for Area of Interest (AoI)
 *      - Efficient event distribution based on proximity
 *
 *   3. Distributed Game World:
 *      - Multiple servers collaborate to host a single game world
 *      - Seamless player experience across server boundaries
 *
 * Architecture:
 *
 *   Server A (this example)       Server B (remote)
 *   ┌─────────────────┐           ┌─────────────────┐
 *   │ cell(5,7) LOCAL │◄─┐     ┌─►│ cell(6,7) LOCAL │
 *   │ cell(5,8) LOCAL │  │     │  └─────────────────┘
 *   └─────────────────┘  │     │
 *         │              │     │
 *         │  Subscribe   │     │  Publish
 *         │  to adjacent │     │  to adjacent
 *         │              │     │
 *         └──────────────┴─────┘
 *              tcp://...
 */

#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

/* Player event structure (JSON-like format) */
typedef struct {
    const char *player_id;
    int cell_x;
    int cell_y;
    const char *action;
    int health;
} player_event_t;

static void format_player_event(char *buf, size_t size, const player_event_t *event)
{
    snprintf(buf, size,
            "{\"player\":\"%s\",\"cell\":\"(%d,%d)\",\"action\":\"%s\",\"health\":%d}",
            event->player_id, event->cell_x, event->cell_y,
            event->action, event->health);
}

static void print_separator(void)
{
    printf("════════════════════════════════════════════════════════════\n");
}

int main(void)
{
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║     SPOT MMORPG Cell-Based Pub/Sub Example             ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* Initialize */
    slk_ctx_t *ctx = slk_ctx_new();
    assert(ctx != NULL);

    slk_spot_t *spot = slk_spot_new(ctx);
    assert(spot != NULL);

    slk_spot_set_hwm(spot, 10000, 10000);  /* High throughput for game events */

    print_separator();

    /* Step 1: Start server mode
     *
     * Bind ROUTER socket to accept connections from other game servers.
     * This allows Server B to connect and synchronize topics.
     */
    printf("STEP 1: Starting Game Server A (this server)\n\n");

    const char *bind_endpoint = "tcp://*:5555";
    if (slk_spot_bind(spot, bind_endpoint) < 0) {
        fprintf(stderr, "Failed to bind: %s\n", slk_strerror(slk_errno()));
        goto cleanup;
    }
    printf("  ✓ Server listening on %s\n", bind_endpoint);
    printf("  ✓ Ready to accept connections from Server B\n\n");

    /* Step 2: Create LOCAL cells
     *
     * These cells are owned by this server. Players in these cells
     * generate events that are published locally.
     */
    printf("STEP 2: Creating LOCAL cells (owned by Server A)\n\n");

    const char *local_cells[] = {
        "zone1:cell:5,7",   /* Cell at position (5,7) */
        "zone1:cell:5,8",   /* Cell at position (5,8) */
        NULL
    };

    for (int i = 0; local_cells[i] != NULL; i++) {
        if (slk_spot_topic_create(spot, local_cells[i]) < 0) {
            fprintf(stderr, "Failed to create cell: %s\n", slk_strerror(slk_errno()));
            goto cleanup;
        }
        printf("  ✓ Created cell: %s [LOCAL]\n", local_cells[i]);
    }
    printf("\n");

    /* Step 3: Route REMOTE cells
     *
     * Adjacent cell(6,7) is managed by Server B.
     * We register it as REMOTE and route it to Server B's endpoint.
     *
     * In a real game, you would:
     *   - Discover remote cells via cluster sync
     *   - Use a service registry (etcd, Consul, etc.)
     *   - Load from configuration
     */
    printf("STEP 3: Routing REMOTE cells (owned by Server B)\n\n");

    const char *remote_cell = "zone1:cell:6,7";
    const char *serverB_endpoint = "tcp://localhost:6666";  /* Server B address */

    printf("  ℹ In production, Server B would be running at %s\n", serverB_endpoint);
    printf("  ℹ For this demo, we'll simulate it locally\n\n");

    /* Note: This would fail if Server B is not running.
     * In production, you would:
     * 1. Use slk_spot_cluster_add() to connect to Server B
     * 2. Use slk_spot_cluster_sync() to discover remote cells
     * 3. Use slk_spot_topic_route() to route specific cells
     *
     * For this demo, we skip the remote connection and show the API usage.
     */

    printf("  → slk_spot_topic_route(spot, \"%s\", \"%s\")\n",
           remote_cell, serverB_endpoint);
    printf("    (skipped in demo - Server B not running)\n\n");

    /* Step 4: Subscribe to adjacent cells (Area of Interest)
     *
     * Subscribe to cells adjacent to our owned cells.
     * This implements spatial interest management.
     *
     * For cell(5,7), adjacent cells are:
     *   (4,6) (5,6) (6,6)
     *   (4,7) [5,7] (6,7)  ← (6,7) is managed by Server B!
     *   (4,8) (5,8) (6,8)
     *
     * We subscribe to:
     *   - cell(5,8): local cell, inproc communication
     *   - cell(6,7): remote cell, TCP communication (if Server B running)
     */
    printf("STEP 4: Subscribing to adjacent cells (Area of Interest)\n\n");

    const char *subscribe_cells[] = {
        "zone1:cell:5,8",   /* Adjacent local cell */
        /* "zone1:cell:6,7" would be remote - skipped in demo */
        NULL
    };

    for (int i = 0; subscribe_cells[i] != NULL; i++) {
        if (slk_spot_subscribe(spot, subscribe_cells[i]) < 0) {
            fprintf(stderr, "Failed to subscribe: %s\n", slk_strerror(slk_errno()));
        } else {
            int is_local = slk_spot_topic_is_local(spot, subscribe_cells[i]);
            printf("  ✓ Subscribed to %s [%s]\n",
                   subscribe_cells[i], is_local ? "LOCAL" : "REMOTE");
        }
    }
    printf("\n");
    printf("  ℹ When Server B is running, we would also subscribe to cell(6,7)\n");
    printf("  ℹ Messages from cell(6,7) would arrive over TCP transparently!\n\n");

    print_separator();

    /* Give inproc subscriptions time to establish */
    slk_sleep(10);

    /* Step 5: Simulate player events
     *
     * Players in different cells perform actions.
     * Events are published to the cell's topic.
     */
    printf("STEP 5: Simulating Player Events\n\n");

    player_event_t events[] = {
        {"hero1", 5, 7, "move", 100},
        {"hero2", 5, 8, "attack", 85},
        {"hero1", 5, 7, "cast_spell", 95},
        {"hero3", 5, 8, "pickup_item", 100},
        {NULL, 0, 0, NULL, 0}
    };

    char event_buf[256];

    printf("Publishing player events to cells:\n\n");

    for (int i = 0; events[i].player_id != NULL; i++) {
        /* Format cell topic */
        char cell_topic[64];
        snprintf(cell_topic, sizeof(cell_topic), "zone1:cell:%d,%d",
                events[i].cell_x, events[i].cell_y);

        /* Format event data */
        format_player_event(event_buf, sizeof(event_buf), &events[i]);

        /* Publish - same API for local and remote! */
        if (slk_spot_publish(spot, cell_topic, event_buf, strlen(event_buf)) == 0) {
            int is_local = slk_spot_topic_is_local(spot, cell_topic);
            printf("  [%d] %s [%s]\n", i + 1, cell_topic,
                   is_local ? "LOCAL" : "REMOTE");
            printf("      → %s: %s (HP: %d)\n",
                   events[i].player_id, events[i].action, events[i].health);
        } else {
            fprintf(stderr, "  Publish failed: %s\n", slk_strerror(slk_errno()));
        }
    }

    printf("\n");
    print_separator();

    /* Step 6: Receive events from adjacent cells
     *
     * We subscribed to cell(5,8), so we should receive events from there.
     * In production, we would also receive events from remote cell(6,7).
     */
    printf("STEP 6: Receiving Events from Adjacent Cells\n\n");
    printf("Expected: Events from cell(5,8) [hero2 and hero3]\n");
    printf("(Events from cell(5,7) filtered - we didn't subscribe to our own cell)\n\n");

    char recv_topic[128];
    char recv_data[1024];
    size_t topic_len, data_len;
    int received_count = 0;

    for (int attempts = 0; attempts < 50 && received_count < 2; attempts++) {
        int rc = slk_spot_recv(spot, recv_topic, sizeof(recv_topic), &topic_len,
                              recv_data, sizeof(recv_data), &data_len,
                              SLK_DONTWAIT);

        if (rc == 0) {
            recv_topic[topic_len] = '\0';
            recv_data[data_len] = '\0';

            printf("  [%d] From: %s\n", received_count + 1, recv_topic);
            printf("      Data: %s\n", recv_data);

            received_count++;
        } else if (slk_errno() == SLK_EAGAIN) {
            slk_sleep(10);
        } else {
            fprintf(stderr, "Receive error: %s\n", slk_strerror(slk_errno()));
            break;
        }
    }

    printf("\n✓ Received %d events from adjacent cells\n\n", received_count);

    print_separator();

    /* Step 7: Demonstrate location transparency */
    printf("STEP 7: Location Transparency Benefits\n\n");

    printf("✓ Benefits demonstrated:\n\n");
    printf("  1. SAME API for local and remote cells:\n");
    printf("     slk_spot_publish(spot, \"zone1:cell:5,7\", ...)  // local\n");
    printf("     slk_spot_publish(spot, \"zone1:cell:6,7\", ...)  // remote\n\n");

    printf("  2. TRANSPARENT message routing:\n");
    printf("     - Local cell → inproc (high performance)\n");
    printf("     - Remote cell → TCP (automatic routing)\n\n");

    printf("  3. SIMPLIFIED game logic:\n");
    printf("     - No need to check if cell is local or remote\n");
    printf("     - No manual socket management\n");
    printf("     - Focus on game logic, not networking\n\n");

    printf("  4. SCALABLE architecture:\n");
    printf("     - Add/remove servers dynamically\n");
    printf("     - Cells can migrate between servers\n");
    printf("     - Horizontal scaling without code changes\n\n");

    print_separator();

    /* List final topology */
    char **topics;
    size_t topic_count;

    if (slk_spot_list_topics(spot, &topics, &topic_count) == 0) {
        printf("Final Cell Topology (%zu cells):\n\n", topic_count);
        for (size_t i = 0; i < topic_count; i++) {
            int is_local = slk_spot_topic_is_local(spot, topics[i]);
            printf("  %zu. %s [%s]\n", i + 1, topics[i],
                   is_local ? "LOCAL - owned by this server" :
                              "REMOTE - owned by another server");
        }
        slk_spot_list_topics_free(topics, topic_count);
    }

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║              Example Completed Successfully             ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");

cleanup:
    slk_spot_destroy(&spot);
    slk_ctx_destroy(ctx);

    return 0;
}

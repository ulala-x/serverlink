/*
 * Mesh Topology Example - MMORPG Cell Pattern
 *
 * Demonstrates using basic PUB/SUB sockets to create a mesh network topology.
 * This is commonly used in MMORPGs where each game cell only needs to
 * communicate with adjacent cells.
 *
 * Topology:
 *
 *         [Cell B]
 *            |
 * [Cell A] - [Cell C] - [Cell D]
 *            |
 *         [Cell E]
 *
 * Each cell:
 *   - Publishes events on its own channel (PUB socket)
 *   - Subscribes to events from adjacent cells (SUB socket)
 *   - Can dynamically add/remove neighbors
 *
 * Use case: Game server where players in one cell can see/interact with
 * players in adjacent cells, but not distant cells.
 *
 * Build:
 *   cmake --build build
 *   ./build/examples/pubsub/mesh_topology_example
 */

#include <stdio.h>
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include <serverlink/serverlink.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#define CHECK(expr, msg) do { \
    if ((expr) < 0) { \
        perror(msg); \
        return 1; \
    } \
} while(0)

#define MAX_NEIGHBORS 8

typedef struct {
    char name;                          // Cell identifier (A, B, C, D, E)
    slk_ctx_t *ctx;
    slk_socket_t *pub;                  // Publish events to this cell
    slk_socket_t *sub;                  // Subscribe to neighbor cells
    char endpoint[64];                  // This cell's endpoint
    char neighbors[MAX_NEIGHBORS][64];  // Neighbor endpoints
    int neighbor_count;
    volatile int running;
    pthread_t thread;
} cell_t;

// Initialize a cell
int cell_init(cell_t *cell, char name, slk_ctx_t *ctx) {
    cell->name = name;
    cell->ctx = ctx;
    cell->neighbor_count = 0;
    cell->running = 1;

    // Create endpoint for this cell
    snprintf(cell->endpoint, sizeof(cell->endpoint), "inproc://cell_%c", name);

    // Create PUB socket for broadcasting events
    cell->pub = slk_socket(ctx, SLK_PUB);
    if (!cell->pub) {
        perror("slk_socket(PUB)");
        return -1;
    }

    if (slk_bind(cell->pub, cell->endpoint) < 0) {
        perror("slk_bind");
        slk_close(cell->pub);
        return -1;
    }

    // Create SUB socket for receiving neighbor events
    cell->sub = slk_socket(ctx, SLK_SUB);
    if (!cell->sub) {
        perror("slk_socket(SUB)");
        slk_close(cell->pub);
        return -1;
    }

    // Subscribe to all messages (we'll filter by which neighbors we connect to)
    if (slk_setsockopt(cell->sub, SLK_SUBSCRIBE, "", 0) < 0) {
        perror("SLK_SUBSCRIBE");
        slk_close(cell->pub);
        slk_close(cell->sub);
        return -1;
    }

    printf("[Cell %c] Initialized on %s\n", cell->name, cell->endpoint);
    return 0;
}

// Add a neighbor cell
int cell_add_neighbor(cell_t *cell, const char *neighbor_endpoint) {
    if (cell->neighbor_count >= MAX_NEIGHBORS) {
        fprintf(stderr, "[Cell %c] Maximum neighbors reached\n", cell->name);
        return -1;
    }

    // Connect to neighbor's PUB socket
    if (slk_connect(cell->sub, neighbor_endpoint) < 0) {
        perror("slk_connect");
        return -1;
    }

    strncpy(cell->neighbors[cell->neighbor_count], neighbor_endpoint, 64);
    cell->neighbor_count++;

    printf("[Cell %c] Added neighbor: %s (total: %d)\n",
           cell->name, neighbor_endpoint, cell->neighbor_count);
    return 0;
}

// Remove a neighbor cell
int cell_remove_neighbor(cell_t *cell, const char *neighbor_endpoint) {
    // Disconnect from neighbor
    if (slk_disconnect(cell->sub, neighbor_endpoint) < 0) {
        perror("slk_disconnect");
        return -1;
    }

    // Remove from neighbor list
    for (int i = 0; i < cell->neighbor_count; i++) {
        if (strcmp(cell->neighbors[i], neighbor_endpoint) == 0) {
            // Shift remaining neighbors
            for (int j = i; j < cell->neighbor_count - 1; j++) {
                strcpy(cell->neighbors[j], cell->neighbors[j + 1]);
            }
            cell->neighbor_count--;
            printf("[Cell %c] Removed neighbor: %s (remaining: %d)\n",
                   cell->name, neighbor_endpoint, cell->neighbor_count);
            return 0;
        }
    }

    fprintf(stderr, "[Cell %c] Neighbor not found: %s\n", cell->name, neighbor_endpoint);
    return -1;
}

// Broadcast an event from this cell
int cell_broadcast(cell_t *cell, const char *event_type, const char *data) {
    char event[512];
    snprintf(event, sizeof(event), "[Cell %c] %s: %s", cell->name, event_type, data);

    if (slk_send(cell->pub, event, strlen(event), 0) < 0) {
        perror("slk_send");
        return -1;
    }

    printf("[Cell %c] Broadcast: %s\n", cell->name, event);
    return 0;
}

// Receive events from neighbor cells (non-blocking)
int cell_receive(cell_t *cell, char *buf, size_t buf_size, int timeout_ms) {
    // Use non-blocking receive
    int rc = slk_recv(cell->sub, buf, buf_size, SLK_DONTWAIT);

    if (rc > 0) {
        buf[rc] = '\0';
        printf("[Cell %c] Received: %s\n", cell->name, buf);
    } else if (rc < 0 && slk_errno() == SLK_EAGAIN && timeout_ms > 0) {
        // If no message and timeout specified, sleep a bit
        usleep(timeout_ms * 1000);
    }

    return rc;
}

// Cleanup cell
void cell_cleanup(cell_t *cell) {
    printf("[Cell %c] Cleaning up\n", cell->name);
    slk_close(cell->sub);
    slk_close(cell->pub);
}

// Simulation: cell receives and processes events
void *cell_event_loop(void *arg) {
    cell_t *cell = (cell_t *)arg;
    char buf[1024];

    printf("[Cell %c] Event loop started\n", cell->name);

    while (cell->running) {
        int rc = cell_receive(cell, buf, sizeof(buf), 1000);  // 1 second timeout

        if (rc > 0) {
            // Process event (in real game, would update game state)
            // For demo, just print it
        } else if (rc < 0 && slk_errno() != SLK_EAGAIN) {
            perror("cell_receive");
            break;
        }
    }

    printf("[Cell %c] Event loop stopped\n", cell->name);
    return NULL;
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("=== ServerLink Mesh Topology Example (MMORPG Cell Pattern) ===\n\n");

    // Initialize context
    slk_ctx_t *ctx = slk_ctx_new();
    if (!ctx) {
        perror("slk_ctx_new");
        return 1;
    }

    printf("Topology:\n");
    printf("\n");
    printf("        [Cell B]\n");
    printf("           |\n");
    printf("[Cell A] - [Cell C] - [Cell D]\n");
    printf("           |\n");
    printf("        [Cell E]\n\n");

    // Create cells
    cell_t cells[5];
    char cell_names[] = {'A', 'B', 'C', 'D', 'E'};

    for (int i = 0; i < 5; i++) {
        if (cell_init(&cells[i], cell_names[i], ctx) < 0) {
            fprintf(stderr, "Failed to initialize cell %c\n", cell_names[i]);
            slk_ctx_destroy(ctx);
            return 1;
        }
    }

    // Allow all cells to bind
    usleep(100000);  // 100ms

    printf("\n=== Setting up Mesh Topology ===\n\n");

    // Cell A neighbors: C
    cell_add_neighbor(&cells[0], cells[2].endpoint);  // A -> C

    // Cell B neighbors: C
    cell_add_neighbor(&cells[1], cells[2].endpoint);  // B -> C

    // Cell C neighbors: A, B, D, E
    cell_add_neighbor(&cells[2], cells[0].endpoint);  // C -> A
    cell_add_neighbor(&cells[2], cells[1].endpoint);  // C -> B
    cell_add_neighbor(&cells[2], cells[3].endpoint);  // C -> D
    cell_add_neighbor(&cells[2], cells[4].endpoint);  // C -> E

    // Cell D neighbors: C
    cell_add_neighbor(&cells[3], cells[2].endpoint);  // D -> C

    // Cell E neighbors: C
    cell_add_neighbor(&cells[4], cells[2].endpoint);  // E -> C

    // Allow subscriptions to propagate
    usleep(200000);  // 200ms

    printf("\n=== Simulating Game Events ===\n\n");

    // Simulate player enters Cell C
    cell_broadcast(&cells[2], "PlayerEnter", "player123");

    // Simulate combat in Cell A
    cell_broadcast(&cells[0], "Combat", "player123 vs monster");

    // Simulate treasure spawn in Cell E
    cell_broadcast(&cells[4], "TreasureSpawn", "legendary_sword");

    // Allow messages to propagate
    usleep(100000);  // 100ms

    // Check what each cell received
    printf("\n=== Checking Cell Message Reception ===\n\n");

    char buf[1024];

    printf("Cell A should receive only Cell C events:\n");
    while (cell_receive(&cells[0], buf, sizeof(buf), 100) > 0);

    printf("\nCell B should receive only Cell C events:\n");
    while (cell_receive(&cells[1], buf, sizeof(buf), 100) > 0);

    printf("\nCell C should receive events from A, B, D, E:\n");
    while (cell_receive(&cells[2], buf, sizeof(buf), 100) > 0);

    printf("\nCell D should receive only Cell C events:\n");
    while (cell_receive(&cells[3], buf, sizeof(buf), 100) > 0);

    printf("\nCell E should receive only Cell C events:\n");
    while (cell_receive(&cells[4], buf, sizeof(buf), 100) > 0);

    printf("\n=== Dynamic Neighbor Management ===\n\n");

    // Simulate: Player in Cell A moves toward Cell D
    // Add direct A-D connection temporarily
    printf("Player moving from A toward D - adding direct A<->D link\n");
    cell_add_neighbor(&cells[0], cells[3].endpoint);  // A -> D
    cell_add_neighbor(&cells[3], cells[0].endpoint);  // D -> A

    usleep(100000);  // 100ms

    // Broadcast from both cells
    cell_broadcast(&cells[0], "PlayerMoving", "player123 -> D");
    cell_broadcast(&cells[3], "PlayerVisible", "player123 visible from D");

    usleep(100000);  // 100ms

    // Cell A and D should now see each other's events
    printf("\nCell A receives from D:\n");
    while (cell_receive(&cells[0], buf, sizeof(buf), 100) > 0);

    printf("\nCell D receives from A:\n");
    while (cell_receive(&cells[3], buf, sizeof(buf), 100) > 0);

    // Player completes move - remove temporary connection
    printf("\n\nPlayer completed move - removing A<->D link\n");
    cell_remove_neighbor(&cells[0], cells[3].endpoint);
    cell_remove_neighbor(&cells[3], cells[0].endpoint);

    printf("\n=== Use Case Summary ===\n\n");
    printf("This mesh topology is ideal for:\n");
    printf("  - MMORPGs with spatial zones/cells\n");
    printf("  - Distributed simulations\n");
    printf("  - Sensor networks with local communication\n");
    printf("  - Any system where locality matters\n\n");

    printf("Benefits:\n");
    printf("  - Reduced network traffic (only adjacent cells communicate)\n");
    printf("  - Scalable (adding cells doesn't affect existing ones)\n");
    printf("  - Dynamic (can add/remove neighbors at runtime)\n");
    printf("  - No central broker (distributed architecture)\n");

    printf("\n=== Cleanup ===\n");

    for (int i = 0; i < 5; i++) {
        cell_cleanup(&cells[i]);
    }

    slk_ctx_destroy(ctx);

    printf("Done.\n");
    return 0;
}

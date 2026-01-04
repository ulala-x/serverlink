/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - SPOT Cluster Synchronization Example */

#include "../src/spot/spot_pubsub.hpp"
#include "../src/core/ctx.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace slk;

/**
 * Example: SPOT Cluster Synchronization
 *
 * This example demonstrates how to use cluster_add(), cluster_sync(),
 * and bind() to create a distributed SPOT PUB/SUB system.
 *
 * Architecture:
 *   [Node A: tcp://*:5555]  ◄────►  [Node B: tcp://*:5556]
 *      - Topics: "game:player1", "game:score"
 *      - bind("tcp://*:5555")
 *      - cluster_add("tcp://localhost:5556")
 *      - cluster_sync() → discovers Node B's topics
 *
 *   [Node B: tcp://*:5556]
 *      - Topics: "chat:room1", "chat:lobby"
 *      - bind("tcp://*:5556")
 *      - cluster_add("tcp://localhost:5555")
 *      - cluster_sync() → discovers Node A's topics
 *
 * After sync:
 *   - Node A knows about Node B's topics (chat:room1, chat:lobby)
 *   - Node B knows about Node A's topics (game:player1, game:score)
 *   - Both nodes can subscribe to remote topics transparently
 */

void run_node_a()
{
    ctx_t ctx;
    spot_pubsub_t spot(&ctx);

    // Bind to accept cluster connections
    if (spot.bind("tcp://*:5555") != 0) {
        std::cerr << "Node A: Failed to bind\n";
        return;
    }
    std::cout << "Node A: Bound to tcp://*:5555\n";

    // Create local topics
    spot.topic_create("game:player1");
    spot.topic_create("game:score");
    std::cout << "Node A: Created topics: game:player1, game:score\n";

    // Add Node B to cluster
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait for Node B to bind
    if (spot.cluster_add("tcp://localhost:5556") != 0) {
        std::cerr << "Node A: Failed to add Node B to cluster\n";
        return;
    }
    std::cout << "Node A: Added Node B to cluster\n";

    // Synchronize topics with cluster
    if (spot.cluster_sync(1000) != 0) {
        std::cerr << "Node A: Failed to sync with cluster\n";
        return;
    }
    std::cout << "Node A: Cluster sync complete\n";

    // List all topics (LOCAL + REMOTE)
    auto topics = spot.list_topics();
    std::cout << "Node A: All topics after sync:\n";
    for (const auto &topic : topics) {
        std::cout << "  - " << topic << " ("
                  << (spot.topic_is_local(topic) ? "LOCAL" : "REMOTE") << ")\n";
    }

    // Now can subscribe to remote topics from Node B
    if (spot.subscribe("chat:room1") == 0) {
        std::cout << "Node A: Subscribed to remote topic 'chat:room1'\n";
    }

    // Publish to local topic
    const char *msg = "Player joined!";
    spot.publish("game:player1", msg, strlen(msg));
    std::cout << "Node A: Published to game:player1\n";

    std::this_thread::sleep_for(std::chrono::seconds(2));
}

void run_node_b()
{
    ctx_t ctx;
    spot_pubsub_t spot(&ctx);

    // Bind to accept cluster connections
    if (spot.bind("tcp://*:5556") != 0) {
        std::cerr << "Node B: Failed to bind\n";
        return;
    }
    std::cout << "Node B: Bound to tcp://*:5556\n";

    // Create local topics
    spot.topic_create("chat:room1");
    spot.topic_create("chat:lobby");
    std::cout << "Node B: Created topics: chat:room1, chat:lobby\n";

    // Add Node A to cluster
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Wait for Node A to bind and add us
    if (spot.cluster_add("tcp://localhost:5555") != 0) {
        std::cerr << "Node B: Failed to add Node A to cluster\n";
        return;
    }
    std::cout << "Node B: Added Node A to cluster\n";

    // Synchronize topics with cluster
    if (spot.cluster_sync(1000) != 0) {
        std::cerr << "Node B: Failed to sync with cluster\n";
        return;
    }
    std::cout << "Node B: Cluster sync complete\n";

    // List all topics (LOCAL + REMOTE)
    auto topics = spot.list_topics();
    std::cout << "Node B: All topics after sync:\n";
    for (const auto &topic : topics) {
        std::cout << "  - " << topic << " ("
                  << (spot.topic_is_local(topic) ? "LOCAL" : "REMOTE") << ")\n";
    }

    // Now can subscribe to remote topics from Node A
    if (spot.subscribe("game:player1") == 0) {
        std::cout << "Node B: Subscribed to remote topic 'game:player1'\n";
    }

    // Publish to local topic
    const char *msg = "Welcome to chat!";
    spot.publish("chat:room1", msg, strlen(msg));
    std::cout << "Node B: Published to chat:room1\n";

    std::this_thread::sleep_for(std::chrono::seconds(2));
}

int main()
{
    std::cout << "=== SPOT Cluster Synchronization Example ===\n\n";

    // Run both nodes in separate threads to simulate distributed system
    std::thread thread_a(run_node_a);
    std::thread thread_b(run_node_b);

    thread_a.join();
    thread_b.join();

    std::cout << "\n=== Example Complete ===\n";
    return 0;
}

/*
 * Expected Output:
 *
 * Node B: Bound to tcp://*:5556
 * Node B: Created topics: chat:room1, chat:lobby
 * Node A: Bound to tcp://*:5555
 * Node A: Created topics: game:player1, game:score
 * Node A: Added Node B to cluster
 * Node A: Cluster sync complete
 * Node A: All topics after sync:
 *   - game:player1 (LOCAL)
 *   - game:score (LOCAL)
 *   - chat:room1 (REMOTE)
 *   - chat:lobby (REMOTE)
 * Node A: Subscribed to remote topic 'chat:room1'
 * Node A: Published to game:player1
 * Node B: Added Node A to cluster
 * Node B: Cluster sync complete
 * Node B: All topics after sync:
 *   - chat:room1 (LOCAL)
 *   - chat:lobby (LOCAL)
 *   - game:player1 (REMOTE)
 *   - game:score (REMOTE)
 * Node B: Subscribed to remote topic 'game:player1'
 * Node B: Published to chat:room1
 */

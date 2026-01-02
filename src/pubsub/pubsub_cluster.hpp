/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Cluster Pub/Sub Manager (Server-to-Server) */

#ifndef SL_PUBSUB_CLUSTER_HPP_INCLUDED
#define SL_PUBSUB_CLUSTER_HPP_INCLUDED

#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>

namespace slk
{

class ctx_t;
class cluster_node_t;
class socket_base_t;
class shard_hash_t;

/**
 * @brief Cluster pub/sub manager for server-to-server distribution
 *
 * This class provides Redis Cluster-style distributed pub/sub across
 * multiple servers. It manages connections to cluster nodes and
 * automatically routes messages based on channel hashing.
 *
 * Architecture:
 *   - Each node is a remote XPUB endpoint
 *   - Channels are hashed to determine target node
 *   - Pattern subscriptions are propagated to all nodes
 *   - Local XPUB socket for receiving messages from all nodes
 *
 * Thread Safety:
 *   - All public methods are thread-safe
 *   - Uses std::shared_mutex for reader-writer lock
 *   - Node operations protected by node-level mutex
 *
 * Routing Strategy:
 *   - Exact channels: hash(channel) % node_count → specific node
 *   - Pattern subscriptions: broadcast to all nodes
 *
 * Fault Tolerance:
 *   - Automatic reconnection with exponential backoff
 *   - Subscription restoration on reconnect
 *   - Node removal on persistent failure (optional)
 */
class pubsub_cluster_t
{
  public:
    /**
     * @brief Construct a new pubsub cluster
     *
     * @param ctx Context for creating sockets
     */
    pubsub_cluster_t (ctx_t *ctx);

    /**
     * @brief Destroy the pubsub cluster
     *
     * Automatically disconnects all nodes and cleans up resources.
     */
    ~pubsub_cluster_t ();

    // Non-copyable and non-movable
    pubsub_cluster_t (const pubsub_cluster_t &) = delete;
    pubsub_cluster_t &operator= (const pubsub_cluster_t &) = delete;
    pubsub_cluster_t (pubsub_cluster_t &&) = delete;
    pubsub_cluster_t &operator= (pubsub_cluster_t &&) = delete;

    // === Node Management ===

    /**
     * @brief Add a node to the cluster
     *
     * Creates a connection to the remote node and subscribes to
     * existing patterns.
     *
     * Thread-safe: can be called from multiple threads concurrently.
     *
     * @param endpoint Remote XPUB endpoint (e.g., "tcp://node1:5555")
     * @return 0 on success, -1 on error
     */
    int add_node (const std::string &endpoint);

    /**
     * @brief Remove a node from the cluster
     *
     * Disconnects from the node and removes it from routing.
     *
     * Thread-safe: can be called from multiple threads concurrently.
     *
     * @param endpoint Remote endpoint to remove
     * @return 0 on success, -1 if node not found
     */
    int remove_node (const std::string &endpoint);

    // === Publishing API ===

    /**
     * @brief Publish a message to a channel (with automatic routing)
     *
     * Routes the message to the appropriate node based on channel hash.
     * Uses the same CRC16 algorithm as sharded_pubsub_t.
     *
     * Thread-safe: can be called from multiple threads concurrently.
     *
     * @param channel Channel name (supports {tag} hash tags)
     * @param data Message data
     * @param len Message length
     * @return 0 on success, -1 on error
     */
    int publish (const std::string &channel, const void *data, size_t len);

    // === Subscription API ===

    /**
     * @brief Subscribe to a channel
     *
     * Routes subscription to the appropriate node based on channel hash.
     *
     * Thread-safe: can be called from multiple threads concurrently.
     *
     * @param channel Channel name
     * @return 0 on success, -1 on error
     */
    int subscribe (const std::string &channel);

    /**
     * @brief Subscribe to a pattern (broadcast to all nodes)
     *
     * Pattern subscriptions are sent to ALL nodes in the cluster.
     *
     * Thread-safe: can be called from multiple threads concurrently.
     *
     * @param pattern Glob pattern (e.g., "news.*")
     * @return 0 on success, -1 on error
     */
    int psubscribe (const std::string &pattern);

    /**
     * @brief Unsubscribe from a channel
     *
     * @param channel Channel name
     * @return 0 on success, -1 on error
     */
    int unsubscribe (const std::string &channel);

    /**
     * @brief Unsubscribe from a pattern
     *
     * @param pattern Glob pattern
     * @return 0 on success, -1 on error
     */
    int punsubscribe (const std::string &pattern);

    // === Message Reception ===

    /**
     * @brief Receive a message from any node
     *
     * Receives messages from all connected nodes via the local receiver.
     *
     * Thread-safe: should only be called from one thread at a time.
     * (Multiple concurrent receivers would compete for messages)
     *
     * @param channel Output channel name
     * @param data Output message data
     * @param flags Receive flags (0 or SLK_DONTWAIT)
     * @return Number of bytes received, -1 on error
     */
    int recv (std::string &channel, std::vector<uint8_t> &data, int flags);

    // === Introspection ===

    /**
     * @brief Get list of all node endpoints
     *
     * @return Vector of endpoint strings
     */
    std::vector<std::string> get_nodes () const;

    /**
     * @brief Get node count
     *
     * @return Number of connected nodes
     */
    size_t get_node_count () const;

    /**
     * @brief Get subscription count
     *
     * @return Number of active subscriptions (exact channels only)
     */
    size_t get_subscription_count () const;

    /**
     * @brief Get pattern subscription count
     *
     * @return Number of active pattern subscriptions
     */
    size_t get_pattern_subscription_count () const;

  private:
    // Context reference
    ctx_t *_ctx;

    // Reader-writer lock for node list and subscriptions
    mutable std::shared_mutex _mutex;

    // Cluster nodes (remote XPUB endpoints we subscribe to)
    std::vector<std::unique_ptr<cluster_node_t>> _nodes;

    // Local XPUB socket for publishing to cluster
    // Other cluster nodes subscribe to this
    socket_base_t *_local_pub;
    std::string _local_pub_endpoint;

    // Active subscriptions (for routing and restoration)
    std::unordered_set<std::string> _subscriptions;
    std::unordered_set<std::string> _pattern_subscriptions;

    // Slot-to-node mapping (for routing exact channels)
    // Uses CRC16 hash like Redis Cluster (16384 slots)
    std::unordered_map<int, size_t> _slot_to_node;

    // Helper: Get node index for a channel
    size_t get_node_for_channel (const std::string &channel) const;

    // Helper: Update slot-to-node mapping
    void rebuild_slot_mapping ();

    // Helper: Find node by endpoint
    cluster_node_t *find_node (const std::string &endpoint);
    const cluster_node_t *find_node (const std::string &endpoint) const;
};

} // namespace slk

#endif

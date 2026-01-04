/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - SPOT PUB/SUB implementation (Scalable Partitioned Ordered Topics) */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <cstdint>

namespace slk
{
class ctx_t;
class socket_base_t;
class topic_registry_t;
class subscription_manager_t;
class spot_node_t;

/**
 * @brief SPOT PUB/SUB - Single Point Of Topic
 *
 * SPOT provides location-transparent pub/sub using topic ID-based routing:
 * - Topic registration (local topic creation)
 * - Subscription management with exact and pattern matching
 * - Position-transparent publish/subscribe (inproc/tcp)
 *
 * Phase 2 Implementation:
 * - Local topics only (inproc transport)
 * - Pattern subscriptions (LOCAL only)
 * - XPUB/XSUB socket pattern
 *
 * Architecture (Phase 2):
 *   topic_create() → XPUB socket per topic (inproc://spot-{counter})
 *   subscribe()    → XSUB connects to topic's inproc endpoint
 *   publish()      → Send to topic's XPUB
 *   recv()         → Receive from XSUB
 *
 * Thread-safety:
 *   - All public methods are thread-safe
 *   - Internal state protected by shared_mutex
 */
class spot_pubsub_t
{
  public:
    /**
     * @brief Construct a new SPOT PUB/SUB instance
     *
     * @param ctx Context for creating sockets
     */
    explicit spot_pubsub_t (ctx_t *ctx);

    /**
     * @brief Destroy the SPOT PUB/SUB instance
     *
     * Automatically cleans up all topics and sockets.
     */
    ~spot_pubsub_t ();

    // Non-copyable and non-movable
    spot_pubsub_t (const spot_pubsub_t &) = delete;
    spot_pubsub_t &operator= (const spot_pubsub_t &) = delete;
    spot_pubsub_t (spot_pubsub_t &&) = delete;
    spot_pubsub_t &operator= (spot_pubsub_t &&) = delete;

    // ========================================================================
    // Topic Ownership
    // ========================================================================

    /**
     * @brief Create a local topic (this node is the publisher)
     *
     * Creates an XPUB socket bound to inproc://spot-{counter}.
     * Registers the topic in topic_registry as LOCAL.
     *
     * @param topic_id Topic identifier
     * @return 0 on success, -1 on error (sets errno to EEXIST if already exists)
     */
    int topic_create (const std::string &topic_id);

    /**
     * @brief Destroy a topic
     *
     * Closes the XPUB socket and unregisters from topic_registry.
     *
     * @param topic_id Topic identifier
     * @return 0 on success, -1 on error (sets errno to ENOENT if not found)
     */
    int topic_destroy (const std::string &topic_id);

    /**
     * @brief Route a topic to a remote endpoint
     *
     * Creates spot_node_t connection to remote endpoint if needed.
     * Registers topic as REMOTE in topic_registry.
     *
     * @param topic_id Topic identifier
     * @param endpoint Remote endpoint (e.g., "tcp://192.168.1.100:5555")
     * @return 0 on success, -1 on error
     *         errno = EEXIST if topic already exists
     *         errno = EHOSTUNREACH if connection fails
     */
    int topic_route (const std::string &topic_id, const std::string &endpoint);

    // ========================================================================
    // Subscription API
    // ========================================================================

    /**
     * @brief Subscribe to a topic
     *
     * Looks up topic in registry and connects XSUB to its inproc endpoint.
     *
     * @param topic_id Topic identifier
     * @return 0 on success, -1 on error (sets errno to ENOENT if topic not found)
     */
    int subscribe (const std::string &topic_id);

    /**
     * @brief Subscribe to a pattern (LOCAL only)
     *
     * Pattern matching uses prefix matching with '*' wildcard.
     * Example: "player:*" matches "player:123", "player:456"
     *
     * @param pattern Pattern string with optional '*' wildcard
     * @return 0 on success, -1 on error
     */
    int subscribe_pattern (const std::string &pattern);

    /**
     * @brief Subscribe to multiple topics at once
     *
     * @param topics Vector of topic IDs to subscribe to
     * @return 0 on success, -1 on error (partial success possible)
     */
    int subscribe_many (const std::vector<std::string> &topics);

    /**
     * @brief Unsubscribe from a topic
     *
     * @param topic_id Topic identifier
     * @return 0 on success, -1 on error (sets errno to ENOENT if not subscribed)
     */
    int unsubscribe (const std::string &topic_id);

    // ========================================================================
    // Publishing API
    // ========================================================================

    /**
     * @brief Publish a message to a topic
     *
     * Sends message to topic's XPUB socket.
     * Message format: [topic_id][data]
     *
     * @param topic_id Topic identifier
     * @param data Message data
     * @param len Message length
     * @return 0 on success, -1 on error
     *         errno = ENOENT if topic not found
     *         errno = EAGAIN if HWM reached (non-blocking)
     */
    int publish (const std::string &topic_id, const void *data, size_t len);

    // ========================================================================
    // Receiving API
    // ========================================================================

    /**
     * @brief Receive a message (topic and data separated)
     *
     * Receives from XSUB socket and separates topic from data.
     *
     * @param topic_buf Buffer for topic ID
     * @param topic_buf_size Size of topic buffer
     * @param topic_len [out] Actual topic length
     * @param data_buf Buffer for message data
     * @param data_buf_size Size of data buffer
     * @param data_len [out] Actual data length
     * @param flags Receive flags (SLK_DONTWAIT)
     * @return 0 on success, -1 on error
     *         errno = EAGAIN if no message available (non-blocking)
     */
    int recv (char *topic_buf, size_t topic_buf_size, size_t *topic_len,
              void *data_buf, size_t data_buf_size, size_t *data_len,
              int flags);

    // ========================================================================
    // Introspection API
    // ========================================================================

    /**
     * @brief List all registered topics
     *
     * @return Vector of topic IDs
     */
    std::vector<std::string> list_topics () const;

    /**
     * @brief Check if a topic exists
     *
     * @param topic_id Topic identifier
     * @return true if topic exists, false otherwise
     */
    bool topic_exists (const std::string &topic_id) const;

    /**
     * @brief Check if a topic is local
     *
     * @param topic_id Topic identifier
     * @return true if topic is LOCAL, false if REMOTE or not found
     */
    bool topic_is_local (const std::string &topic_id) const;

    // ========================================================================
    // Configuration API
    // ========================================================================

    /**
     * @brief Set high water marks
     *
     * @param sndhwm Send high water mark (messages)
     * @param rcvhwm Receive high water mark (messages)
     * @return 0 on success, -1 on error
     */
    int set_hwm (int sndhwm, int rcvhwm);

    // ========================================================================
    // Cluster Management
    // ========================================================================

    /**
     * @brief Bind to an endpoint for server mode (accepting cluster connections)
     *
     * Creates a ROUTER socket to accept connections from other SPOT nodes.
     * Enables this node to respond to QUERY requests from cluster peers.
     *
     * @param endpoint Bind endpoint (e.g., "tcp://*:5555")
     * @return 0 on success, -1 on error
     */
    int bind (const std::string &endpoint);

    /**
     * @brief Add a cluster node
     *
     * Establishes connection to a remote SPOT node for cluster synchronization.
     *
     * @param endpoint Remote node endpoint (e.g., "tcp://192.168.1.100:5555")
     * @return 0 on success, -1 on error
     *         errno = EEXIST if node already added
     *         errno = EHOSTUNREACH if connection fails
     */
    int cluster_add (const std::string &endpoint);

    /**
     * @brief Remove a cluster node
     *
     * Disconnects from a remote SPOT node and removes it from the cluster.
     *
     * @param endpoint Remote node endpoint
     * @return 0 on success, -1 on error
     *         errno = ENOENT if node not found
     */
    int cluster_remove (const std::string &endpoint);

    /**
     * @brief Synchronize topics with cluster nodes
     *
     * Sends QUERY commands to all cluster nodes and updates the local topic
     * registry with discovered remote topics.
     *
     * @param timeout_ms Timeout in milliseconds for sync operation
     * @return 0 on success, -1 on error
     */
    int cluster_sync (int timeout_ms);

    // ========================================================================
    // Event Loop Integration
    // ========================================================================

    /**
     * @brief Get pollable file descriptor
     *
     * Returns the file descriptor for the receive socket (XSUB).
     * Can be used with poll/epoll/select.
     *
     * @param fd [out] File descriptor
     * @return 0 on success, -1 on error
     */
    int fd (int *fd) const;

  private:
    // Context
    ctx_t *_ctx;

    // Core components
    std::unique_ptr<topic_registry_t> _registry;
    std::unique_ptr<subscription_manager_t> _sub_manager;

    // Local topic publishers: topic_id → XPUB socket
    std::unordered_map<std::string, socket_base_t *> _local_publishers;

    // Remote node connections: endpoint → spot_node_t
    std::unordered_map<std::string, std::unique_ptr<spot_node_t>> _nodes;

    // Remote topic routing: topic_id → spot_node_t*
    std::unordered_map<std::string, spot_node_t *> _remote_topic_nodes;

    // Receive socket (XSUB) - connects to all subscribed LOCAL topics
    socket_base_t *_recv_socket;

    // Server socket (ROUTER) - for accepting cluster connections and QUERY requests
    socket_base_t *_server_socket;

    // High water marks
    int _sndhwm;
    int _rcvhwm;

    // Thread safety
    mutable std::shared_mutex _mutex;

    // Counter for generating unique inproc endpoints
    uint64_t _topic_counter;

    // Internal helper methods
    void process_incoming_messages ();
    int handle_query_request (const std::string &routing_id);
};

} // namespace slk

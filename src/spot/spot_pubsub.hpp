/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - SPOT PUB/SUB implementation (Scalable Partitioned Ordered Topics) */

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <memory>
#include <shared_mutex>
#include <cstdint>
#include "util/clock.hpp"

namespace slk
{
class ctx_t;
class socket_base_t;
class topic_registry_t;
class subscription_manager_t;

/**
 * @brief SPOT PUB/SUB - Single Point Of Topic
 *
 * SPOT provides location-transparent pub/sub using topic ID-based routing:
 * - Topic registration (local topic creation)
 * - Subscription management with exact and pattern matching
 * - Position-transparent publish/subscribe (inproc/tcp)
 *
 * Architecture:
 *   - One shared XPUB socket per SPOT instance (bound to inproc + optional TCP)
 *   - One shared XSUB socket per SPOT instance (connects to local and remote XPUBs)
 *   - topic_create() → registers topic, publishes through shared XPUB
 *   - subscribe()    → connects XSUB to XPUB (local or remote)
 *   - publish()      → sends to shared XPUB
 *   - recv()         → receives from XSUB
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
     * Registers the topic. Messages are published through the shared XPUB.
     *
     * @param topic_id Topic identifier
     * @return 0 on success, -1 on error (sets errno to EEXIST if already exists)
     */
    int topic_create (const std::string &topic_id);

    /**
     * @brief Destroy a topic
     *
     * Unregisters the topic.
     *
     * @param topic_id Topic identifier
     * @return 0 on success, -1 on error (sets errno to ENOENT if not found)
     */
    int topic_destroy (const std::string &topic_id);

    /**
     * @brief Route a topic to a remote endpoint
     *
     * Registers topic as REMOTE. The actual connection happens in subscribe().
     *
     * @param topic_id Topic identifier
     * @param endpoint Remote endpoint (e.g., "tcp://192.168.1.100:5555")
     * @return 0 on success, -1 on error
     *         errno = EEXIST if topic already exists
     */
    int topic_route (const std::string &topic_id, const std::string &endpoint);

    // ========================================================================
    // Subscription API
    // ========================================================================

    /**
     * @brief Subscribe to a topic
     *
     * For LOCAL topics: sends subscription filter to local XPUB.
     * For REMOTE topics: connects XSUB to remote XPUB and sends subscription filter.
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
     * Sends message to shared XPUB socket.
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

    /**
     * @brief Set socket option
     *
     * @param option Option identifier (e.g., SLK_RCVTIMEO)
     * @param value Option value
     * @param len Value size
     * @return 0 on success, -1 on error
     */
    int setsockopt (int option, const void *value, size_t len);

    /**
     * @brief Get socket option
     *
     * @param option Option identifier (e.g., SLK_RCVTIMEO)
     * @param value [out] Option value
     * @param len [in/out] Value size
     * @return 0 on success, -1 on error
     */
    int getsockopt (int option, void *value, size_t *len) const;

    // ========================================================================
    // Cluster Management
    // ========================================================================

    /**
     * @brief Bind XPUB to an endpoint for remote subscribers
     *
     * Binds the shared XPUB socket to accept remote connections.
     *
     * @param endpoint Bind endpoint (e.g., "tcp://*:5555")
     * @return 0 on success, -1 on error
     */
    int bind (const std::string &endpoint);

    /**
     * @brief Add a cluster node (connect XSUB to remote XPUB)
     *
     * @param endpoint Remote node endpoint (e.g., "tcp://192.168.1.100:5555")
     * @return 0 on success, -1 on error
     *         errno = EEXIST if node already added
     */
    int cluster_add (const std::string &endpoint);

    /**
     * @brief Remove a cluster node (disconnect XSUB from remote XPUB)
     *
     * @param endpoint Remote node endpoint
     * @return 0 on success, -1 on error
     *         errno = ENOENT if node not found
     */
    int cluster_remove (const std::string &endpoint);

    /**
     * @brief Synchronize topics with cluster nodes
     *
     * With XPUB/XSUB architecture, this is a no-op (topics discovered via subscription).
     *
     * @param timeout_ms Timeout in milliseconds (unused)
     * @return 0 always
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

    // Shared publish socket (XPUB) - bound to inproc and optionally TCP
    socket_base_t *_pub_socket;

    // Receive socket (XSUB) - connects to local XPUB and remote XPUBs
    socket_base_t *_recv_socket;

    // Endpoints
    std::string _inproc_endpoint;  // Local inproc endpoint (always set)
    std::string _bind_endpoint;    // Primary TCP bind endpoint (first TCP bind)
    std::unordered_set<std::string> _bind_endpoints;  // All bound endpoints

    // Connected remote endpoints (for deduplication)
    std::unordered_set<std::string> _connected_endpoints;

    // High water marks
    int _sndhwm;
    int _rcvhwm;

    // Timeout support
    int _rcvtimeo;     // Receive timeout in milliseconds (-1 = infinite)
    clock_t _clock;    // Clock for timeout calculation

    // Thread safety
    mutable std::shared_mutex _mutex;
};

} // namespace slk

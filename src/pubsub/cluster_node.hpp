/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Cluster Node Connection Manager */

#ifndef SL_CLUSTER_NODE_HPP_INCLUDED
#define SL_CLUSTER_NODE_HPP_INCLUDED

#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

namespace slk
{

class ctx_t;
class socket_base_t;

/**
 * @brief Connection state of a cluster node
 */
enum class node_state_t
{
    DISCONNECTED,  // Not connected
    CONNECTING,    // Connection attempt in progress
    CONNECTED      // Successfully connected
};

/**
 * @brief Individual cluster node connection manager
 *
 * This class manages a connection to a single node in the cluster,
 * handling reconnection logic, state tracking, and message forwarding.
 *
 * Architecture:
 *   - Maintains DEALER socket connection to remote node
 *   - Tracks connection state (disconnected, connecting, connected)
 *   - Implements exponential backoff reconnection
 *   - Restores subscriptions on reconnect
 *
 * Thread Safety:
 *   - All public methods are thread-safe
 *   - Internal state protected by mutex
 */
class cluster_node_t
{
  public:
    /**
     * @brief Construct a new cluster node
     *
     * @param ctx Context for creating sockets
     * @param endpoint Remote endpoint (e.g., "tcp://node1:5555")
     */
    cluster_node_t (ctx_t *ctx, const std::string &endpoint);

    /**
     * @brief Destroy the cluster node
     *
     * Automatically disconnects if connected.
     */
    ~cluster_node_t ();

    // Non-copyable and non-movable
    cluster_node_t (const cluster_node_t &) = delete;
    cluster_node_t &operator= (const cluster_node_t &) = delete;
    cluster_node_t (cluster_node_t &&) = delete;
    cluster_node_t &operator= (cluster_node_t &&) = delete;

    /**
     * @brief Connect to the remote node
     *
     * Creates a DEALER socket and connects to the endpoint.
     * This is a non-blocking call; actual connection may be asynchronous.
     *
     * @return 0 on success, -1 on error
     */
    int connect ();

    /**
     * @brief Disconnect from the remote node
     *
     * Closes the socket and transitions to DISCONNECTED state.
     *
     * @return 0 on success, -1 on error
     */
    int disconnect ();

    /**
     * @brief Publish a message to this node
     *
     * Sends a message to the remote node. If the connection is down,
     * returns an error immediately (no buffering).
     *
     * @param channel Channel name
     * @param data Message data
     * @param len Message length
     * @return 0 on success, -1 on error
     */
    int publish (const std::string &channel, const void *data, size_t len);

    /**
     * @brief Receive a message from this node
     *
     * Receives a message from the remote node.
     *
     * @param channel Output channel name
     * @param data Output message data
     * @param flags Receive flags (0 or SLK_DONTWAIT)
     * @return Number of bytes received, -1 on error
     */
    int recv (std::string &channel, std::vector<uint8_t> &data, int flags);

    /**
     * @brief Add a subscription to restore on reconnect
     *
     * Tracks subscriptions so they can be restored after reconnection.
     *
     * @param channel Channel name
     */
    void add_subscription (const std::string &channel);

    /**
     * @brief Add a pattern subscription to restore on reconnect
     *
     * @param pattern Pattern string
     */
    void add_pattern_subscription (const std::string &pattern);

    /**
     * @brief Remove a subscription
     *
     * @param channel Channel name
     */
    void remove_subscription (const std::string &channel);

    /**
     * @brief Remove a pattern subscription
     *
     * @param pattern Pattern string
     */
    void remove_pattern_subscription (const std::string &pattern);

    /**
     * @brief Get current connection state
     *
     * @return Current state
     */
    node_state_t get_state () const;

    /**
     * @brief Get endpoint string
     *
     * @return Endpoint string
     */
    std::string get_endpoint () const { return _endpoint; }

    /**
     * @brief Check if node is connected
     *
     * @return true if connected, false otherwise
     */
    bool is_connected () const;

    /**
     * @brief Update last heartbeat timestamp
     *
     * Called when a message is received from this node.
     */
    void update_heartbeat ();

    /**
     * @brief Check if heartbeat timeout has occurred
     *
     * @param timeout_ms Timeout in milliseconds (default: 5000ms)
     * @return true if timeout occurred, false otherwise
     */
    bool is_heartbeat_timeout (int timeout_ms = 5000) const;

    /**
     * @brief Get socket for polling
     *
     * Returns the underlying socket for use with poller.
     *
     * @return Socket pointer (may be nullptr if not connected)
     */
    socket_base_t *get_socket ();

  private:
    // Context reference
    ctx_t *_ctx;

    // Remote endpoint
    std::string _endpoint;

    // DEALER socket for communication
    socket_base_t *_socket;

    // Connection state
    std::atomic<node_state_t> _state;

    // Mutex for thread-safe operations
    mutable std::mutex _mutex;

    // Subscriptions to restore on reconnect
    std::vector<std::string> _subscriptions;
    std::vector<std::string> _pattern_subscriptions;

    // Heartbeat tracking
    std::chrono::steady_clock::time_point _last_heartbeat;

    // Reconnection tracking
    int _reconnect_attempts;
    std::chrono::steady_clock::time_point _last_reconnect_attempt;

    // Helper: Restore all subscriptions
    int restore_subscriptions ();
};

} // namespace slk

#endif

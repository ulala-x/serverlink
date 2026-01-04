/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - SPOT Node (Remote Node Connection) */

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

// Forward declarations
using slk_fd_t = int;

namespace slk
{
class ctx_t;
class socket_base_t;

/**
 * @brief SPOT Message Protocol Commands
 */
enum class spot_command_t : uint8_t
{
    PUBLISH = 0x01,
    SUBSCRIBE = 0x02,
    UNSUBSCRIBE = 0x03,
    QUERY = 0x04,
    QUERY_RESP = 0x05
};

/**
 * @brief SPOT Node - Remote Node Connection
 *
 * Manages connection to a remote SPOT node using DEALER socket:
 * - Establishes and maintains TCP connection
 * - Sends PUBLISH/SUBSCRIBE/UNSUBSCRIBE commands
 * - Receives messages from remote topics
 * - Provides automatic reconnection
 *
 * Message Protocol:
 *   Frame 0: Command (uint8_t) - PUBLISH/SUBSCRIBE/UNSUBSCRIBE
 *   Frame 1: Topic ID (string)
 *   Frame 2: Data (PUBLISH only) / Subscriber endpoint (SUBSCRIBE/UNSUBSCRIBE)
 *
 * Architecture:
 *   Local SPOT → spot_node_t (DEALER) → Remote SPOT (via TCP)
 *
 * Thread-safety:
 *   - All public methods are thread-safe
 */
class spot_node_t
{
  public:
    /**
     * @brief Construct a new SPOT node connection
     *
     * @param ctx Context for creating sockets
     * @param endpoint Remote node endpoint (e.g., "tcp://192.168.1.100:5555")
     */
    explicit spot_node_t (ctx_t *ctx, const std::string &endpoint);

    /**
     * @brief Destroy the SPOT node connection
     *
     * Automatically disconnects if connected.
     */
    ~spot_node_t ();

    // Non-copyable and non-movable
    spot_node_t (const spot_node_t &) = delete;
    spot_node_t &operator= (const spot_node_t &) = delete;
    spot_node_t (spot_node_t &&) = delete;
    spot_node_t &operator= (spot_node_t &&) = delete;

    /**
     * @brief Connect to the remote node
     *
     * Creates DEALER socket and connects to endpoint.
     *
     * @return 0 on success, -1 on error
     */
    int connect ();

    /**
     * @brief Disconnect from the remote node
     *
     * @return 0 on success, -1 on error
     */
    int disconnect ();

    /**
     * @brief Check if connected
     *
     * @return true if connected, false otherwise
     */
    bool is_connected () const;

    /**
     * @brief Send PUBLISH message to remote node
     *
     * @param topic_id Topic identifier
     * @param data Message data
     * @param len Message length
     * @return 0 on success, -1 on error
     */
    int send_publish (const std::string &topic_id, const void *data, size_t len);

    /**
     * @brief Send SUBSCRIBE message to remote node
     *
     * @param topic_id Topic identifier
     * @return 0 on success, -1 on error
     */
    int send_subscribe (const std::string &topic_id);

    /**
     * @brief Send UNSUBSCRIBE message to remote node
     *
     * @param topic_id Topic identifier
     * @return 0 on success, -1 on error
     */
    int send_unsubscribe (const std::string &topic_id);

    /**
     * @brief Send QUERY message to remote node (request topic list)
     *
     * @return 0 on success, -1 on error
     */
    int send_query ();

    /**
     * @brief Receive QUERY_RESP from remote node (topic list response)
     *
     * @param topics [out] Vector of topic IDs received from remote node
     * @param flags Receive flags (SL_DONTWAIT)
     * @return 0 on success, -1 on error
     */
    int recv_query_response (std::vector<std::string> &topics, int flags);

    /**
     * @brief Receive message from remote node
     *
     * @param topic_id [out] Received topic ID
     * @param data [out] Received message data
     * @param flags Receive flags (SL_DONTWAIT)
     * @return 0 on success, -1 on error
     */
    int recv (std::string &topic_id, std::vector<uint8_t> &data, int flags);

    /**
     * @brief Get pollable file descriptor
     *
     * @param fd [out] File descriptor
     * @return 0 on success, -1 on error
     */
    int fd (slk_fd_t *fd) const;

    /**
     * @brief Get remote endpoint
     *
     * @return Remote endpoint string
     */
    const std::string &endpoint () const;

  private:
    // Context and configuration
    ctx_t *_ctx;
    std::string _endpoint;

    // DEALER socket for communication
    socket_base_t *_socket;

    // Connection state
    bool _connected;

    // Reconnection parameters
    int _reconnect_ivl;
    int _reconnect_ivl_max;

    // Thread safety
    mutable std::mutex _mutex;
};

} // namespace slk

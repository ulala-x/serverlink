/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Broker Pub/Sub implementation (XSUB/XPUB Proxy wrapper) */

#ifndef SL_PUBSUB_BROKER_HPP_INCLUDED
#define SL_PUBSUB_BROKER_HPP_INCLUDED

#include <string>
#include <thread>
#include <atomic>
#include <mutex>

namespace slk
{
class ctx_t;
class socket_base_t;

/**
 * @brief High-level Pub/Sub broker wrapper around XSUB/XPUB proxy
 *
 * This class provides a simple broker for Pub/Sub patterns by wrapping
 * the XSUB/XPUB proxy functionality. It runs the proxy in a background
 * thread and provides statistics tracking.
 *
 * Architecture:
 *   Publishers → XSUB (frontend) → Proxy → XPUB (backend) → Subscribers
 *
 * Thread-safety:
 *   - All public methods are thread-safe
 *   - Internal state protected by atomic variables
 *   - Proxy runs in isolated background thread
 */
class pubsub_broker_t
{
  public:
    /**
     * @brief Construct a new pubsub broker
     *
     * @param ctx Context for creating sockets
     * @param frontend Frontend endpoint for publishers (e.g., "tcp://0.0.0.0:5555")
     * @param backend Backend endpoint for subscribers (e.g., "tcp://0.0.0.0:5556")
     */
    pubsub_broker_t (ctx_t *ctx,
                     const std::string &frontend,
                     const std::string &backend);

    /**
     * @brief Destroy the pubsub broker
     *
     * Automatically stops the broker if running and cleans up resources.
     */
    ~pubsub_broker_t ();

    // Non-copyable and non-movable
    pubsub_broker_t (const pubsub_broker_t &) = delete;
    pubsub_broker_t &operator= (const pubsub_broker_t &) = delete;
    pubsub_broker_t (pubsub_broker_t &&) = delete;
    pubsub_broker_t &operator= (pubsub_broker_t &&) = delete;

    /**
     * @brief Run the broker in the current thread (blocking)
     *
     * This method blocks until an error occurs or stop() is called from
     * another thread.
     *
     * @return 0 on success (normal termination), -1 on error
     */
    int run ();

    /**
     * @brief Start the broker in a background thread
     *
     * This method creates a new thread and runs the proxy in it.
     * Returns immediately.
     *
     * @return 0 on success, -1 on error
     */
    int start ();

    /**
     * @brief Stop the broker
     *
     * Gracefully shuts down the broker by:
     * 1. Setting stop flag
     * 2. Closing sockets (interrupts proxy)
     * 3. Waiting for thread to finish (if background mode)
     *
     * @return 0 on success, -1 on error
     */
    int stop ();

    /**
     * @brief Check if the broker is currently running
     *
     * @return true if the broker is running, false otherwise
     */
    bool is_running () const;

    /**
     * @brief Get total message count
     *
     * Note: This counts messages in both directions (frontend and backend).
     * The actual implementation increments on each message forwarded.
     *
     * @return Total number of messages forwarded
     */
    size_t get_message_count () const;

  private:
    // Context and endpoint configuration
    ctx_t *_ctx;
    std::string _frontend;
    std::string _backend;

    // Sockets (created in run())
    socket_base_t *_xsub;
    socket_base_t *_xpub;
    std::mutex _socket_mutex;  // Protects socket access

    // Control sockets for steerable proxy (PAIR)
    socket_base_t *_control_pub;  // PAIR socket (bind side) - for sending commands
    socket_base_t *_control_sub;  // PAIR socket (connect side) - passed to proxy
    std::string _control_endpoint;

    // Threading
    std::thread _proxy_thread;
    std::atomic<bool> _running;
    std::atomic<bool> _stop_requested;

    // Statistics
    std::atomic<size_t> _message_count;

    // Internal thread entry point
    void run_thread ();

    // Initialize sockets (bind to endpoints)
    int init_sockets ();

    // Cleanup sockets (thread-safe)
    void cleanup_sockets ();

    // Cleanup sockets without locking (must be called with lock held)
    void cleanup_sockets_unlocked ();
};

} // namespace slk

#endif

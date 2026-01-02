/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Broker Pub/Sub implementation (XSUB/XPUB Proxy wrapper) */

#include "pubsub_broker.hpp"
#include "../core/ctx.hpp"
#include "../core/socket_base.hpp"
#include "../core/proxy.hpp"
#include "../msg/msg.hpp"
#include "../util/err.hpp"
#include "../util/constants.hpp"

#include <cassert>
#include <cstring>
#include <chrono>

namespace slk
{

pubsub_broker_t::pubsub_broker_t (ctx_t *ctx,
                                  const std::string &frontend,
                                  const std::string &backend)
    : _ctx (ctx)
    , _frontend (frontend)
    , _backend (backend)
    , _xsub (nullptr)
    , _xpub (nullptr)
    , _control_pub (nullptr)
    , _control_sub (nullptr)
    , _running (false)
    , _stop_requested (false)
    , _message_count (0)
{
    assert (ctx);

    // Generate unique control endpoint using timestamp
    // This ensures multiple brokers can coexist in the same context
    auto now = std::chrono::high_resolution_clock::now ();
    auto timestamp = now.time_since_epoch ().count ();
    _control_endpoint = "inproc://broker-control-" + std::to_string (timestamp);
}

pubsub_broker_t::~pubsub_broker_t ()
{
    // Ensure broker is stopped before destruction
    if (_running.load ()) {
        stop ();
    }

    // Clean up any remaining sockets
    cleanup_sockets ();
}

int pubsub_broker_t::init_sockets ()
{
    std::lock_guard<std::mutex> lock (_socket_mutex);

    // Create XSUB socket (frontend - for publishers)
    _xsub = _ctx->create_socket (SL_XSUB);
    if (!_xsub) {
        return -1;
    }

    // Create XPUB socket (backend - for subscribers)
    _xpub = _ctx->create_socket (SL_XPUB);
    if (!_xpub) {
        _ctx->destroy_socket (_xsub);
        _xsub = nullptr;
        return -1;
    }

    // Create control socket pair for steerable proxy
    _control_pub = _ctx->create_socket (SL_PAIR);
    if (!_control_pub) {
        _ctx->destroy_socket (_xsub);
        _ctx->destroy_socket (_xpub);
        _xsub = nullptr;
        _xpub = nullptr;
        return -1;
    }

    _control_sub = _ctx->create_socket (SL_PAIR);
    if (!_control_sub) {
        _ctx->destroy_socket (_xsub);
        _ctx->destroy_socket (_xpub);
        _ctx->destroy_socket (_control_pub);
        _xsub = nullptr;
        _xpub = nullptr;
        _control_pub = nullptr;
        return -1;
    }

    // Set linger to 0 for fast shutdown
    int linger = 0;
    _xsub->setsockopt (SL_LINGER, &linger, sizeof (linger));
    _xpub->setsockopt (SL_LINGER, &linger, sizeof (linger));
    _control_pub->setsockopt (SL_LINGER, &linger, sizeof (linger));
    _control_sub->setsockopt (SL_LINGER, &linger, sizeof (linger));

    // Bind frontend (XSUB) - publishers connect here
    int rc = _xsub->bind (_frontend.c_str ());
    if (rc < 0) {
        cleanup_sockets_unlocked ();
        return -1;
    }

    // Bind backend (XPUB) - subscribers connect here
    rc = _xpub->bind (_backend.c_str ());
    if (rc < 0) {
        cleanup_sockets_unlocked ();
        return -1;
    }

    // Bind control_pub and connect control_sub
    rc = _control_pub->bind (_control_endpoint.c_str ());
    if (rc < 0) {
        cleanup_sockets_unlocked ();
        return -1;
    }

    rc = _control_sub->connect (_control_endpoint.c_str ());
    if (rc < 0) {
        cleanup_sockets_unlocked ();
        return -1;
    }

    return 0;
}

void pubsub_broker_t::cleanup_sockets_unlocked ()
{
    // Must be called with _socket_mutex held
    if (_xsub) {
        _ctx->destroy_socket (_xsub);
        _xsub = nullptr;
    }
    if (_xpub) {
        _ctx->destroy_socket (_xpub);
        _xpub = nullptr;
    }
    if (_control_pub) {
        _ctx->destroy_socket (_control_pub);
        _control_pub = nullptr;
    }
    if (_control_sub) {
        _ctx->destroy_socket (_control_sub);
        _control_sub = nullptr;
    }
}

void pubsub_broker_t::cleanup_sockets ()
{
    std::lock_guard<std::mutex> lock (_socket_mutex);
    cleanup_sockets_unlocked ();
}

int pubsub_broker_t::run ()
{
    // Initialize sockets (including control socket pair)
    int rc = init_sockets ();
    if (rc < 0) {
        return -1;
    }

    // Mark as running
    _running.store (true);
    _stop_requested.store (false);

    // Run the steerable proxy (blocking)
    // This will run until TERMINATE command is received or an error occurs
    rc = proxy_steerable (_xsub, _xpub, nullptr, _control_sub);

    // Mark as stopped
    _running.store (false);

    // Clean up sockets
    cleanup_sockets ();

    // Check if we stopped due to user request (normal) or error
    if (_stop_requested.load ()) {
        return 0;  // Normal shutdown
    }

    return rc;
}

void pubsub_broker_t::run_thread ()
{
    // Thread entry point - just call run()
    run ();
}

int pubsub_broker_t::start ()
{
    // Check if already running
    if (_running.load ()) {
        errno = EINVAL;
        return -1;
    }

    // Start the proxy thread
    try {
        _proxy_thread = std::thread (&pubsub_broker_t::run_thread, this);
        // Detach so we don't have to join later if stop() isn't called
        // Actually, don't detach - we need to join in stop()
    } catch (...) {
        errno = ENOMEM;
        return -1;
    }

    // Wait a bit for the thread to start and initialize
    // This is a simple approach - a more robust solution would use
    // condition variables for synchronization
    std::this_thread::sleep_for (std::chrono::milliseconds (50));

    // Check if initialization succeeded
    if (!_running.load ()) {
        // Thread failed to start or initialize
        if (_proxy_thread.joinable ()) {
            _proxy_thread.join ();
        }
        return -1;
    }

    return 0;
}

int pubsub_broker_t::stop ()
{
    // Set stop flag
    _stop_requested.store (true);

    // If not running, nothing to do
    if (!_running.load ()) {
        return 0;
    }

    // Send TERMINATE command to control socket
    // This will gracefully terminate the steerable proxy
    if (_control_pub) {
        // Set a receive timeout to avoid blocking forever
        int timeout_ms = 1000;  // 1 second timeout
        _control_pub->setsockopt (SL_RCVTIMEO, &timeout_ms, sizeof (timeout_ms));

        msg_t msg;
        int rc = msg.init_size (9);  // "TERMINATE" is 9 bytes
        if (rc == 0) {
            memcpy (msg.data (), "TERMINATE", 9);
            rc = _control_pub->send (&msg, 0);

            // Receive the reply (proxy sends an empty reply to satisfy REP duty)
            if (rc >= 0) {
                msg_t reply;
                reply.init ();
                _control_pub->recv (&reply, 0);
                // Ignore errors - proxy might already be shutting down or timeout
            }
        }
    }

    // Wait for thread to finish
    if (_proxy_thread.joinable ()) {
        _proxy_thread.join ();
    }

    _running.store (false);

    return 0;
}

bool pubsub_broker_t::is_running () const
{
    return _running.load ();
}

size_t pubsub_broker_t::get_message_count () const
{
    // Note: Currently we don't track message count in the proxy
    // This would require modifying the proxy or using a capture socket
    // For now, return 0 as a placeholder
    // TODO: Implement message counting via capture socket or proxy modification
    return _message_count.load ();
}

} // namespace slk

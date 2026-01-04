/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - SPOT PUB/SUB implementation (Scalable Partitioned Ordered Topics) */

#include "spot_pubsub.hpp"
#include "topic_registry.hpp"
#include "subscription_manager.hpp"
#include "../core/ctx.hpp"
#include "../core/socket_base.hpp"
#include "../msg/msg.hpp"
#include "../util/err.hpp"
#include "../util/constants.hpp"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <atomic>

namespace slk
{

// Generate unique instance ID for inproc endpoints
static std::string generate_instance_id ()
{
    static std::atomic<uint64_t> counter {0};
    char buf[64];
    snprintf (buf, sizeof (buf), "spot-%llu",
              static_cast<unsigned long long> (counter.fetch_add (1, std::memory_order_relaxed)));
    return buf;
}

spot_pubsub_t::spot_pubsub_t (ctx_t *ctx_)
    : _ctx (ctx_)
    , _registry (new topic_registry_t ())
    , _sub_manager (new subscription_manager_t ())
    , _recv_socket (nullptr)
    , _pub_socket (nullptr)
    , _sndhwm (1000)
    , _rcvhwm (1000)
    , _rcvtimeo (-1)
{
    if (!_ctx) {
        throw std::invalid_argument ("Context pointer is null");
    }

    // Generate unique inproc endpoint for this instance
    _inproc_endpoint = "inproc://" + generate_instance_id ();

    // Create shared XPUB socket for publishing (all topics go through this)
    _pub_socket = _ctx->create_socket (SL_XPUB);
    if (!_pub_socket) {
        throw std::runtime_error ("Failed to create XPUB socket");
    }

    // Set HWM for publisher socket
    _pub_socket->setsockopt (SL_SNDHWM, &_sndhwm, sizeof (_sndhwm));

    // Bind to inproc endpoint (local subscriptions will connect here)
    if (_pub_socket->bind (_inproc_endpoint.c_str ()) != 0) {
        _pub_socket->close ();
        throw std::runtime_error ("Failed to bind XPUB socket to inproc");
    }

    // Create receive socket (XSUB) for all subscriptions
    _recv_socket = _ctx->create_socket (SL_XSUB);
    if (!_recv_socket) {
        _pub_socket->close ();
        throw std::runtime_error ("Failed to create XSUB socket");
    }

    // Set HWM for receive socket
    _recv_socket->setsockopt (SL_RCVHWM, &_rcvhwm, sizeof (_rcvhwm));

    // Connect XSUB to local XPUB for receiving local messages
    if (_recv_socket->connect (_inproc_endpoint.c_str ()) != 0) {
        _recv_socket->close ();
        _pub_socket->close ();
        throw std::runtime_error ("Failed to connect XSUB to local XPUB");
    }
}

spot_pubsub_t::~spot_pubsub_t ()
{
    // Destroy receive socket
    if (_recv_socket) {
        _recv_socket->close ();
        _recv_socket = nullptr;
    }

    // Destroy publish socket
    if (_pub_socket) {
        _pub_socket->close ();
        _pub_socket = nullptr;
    }
}

// ============================================================================
// Topic Ownership
// ============================================================================

int spot_pubsub_t::topic_create (const std::string &topic_id)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check if topic already exists
    if (_registry->has_topic (topic_id)) {
        errno = EEXIST;
        return -1;
    }

    // Register in topic registry as LOCAL
    // The endpoint is our shared inproc (or TCP if bound)
    std::string endpoint = _bind_endpoint.empty () ? _inproc_endpoint : _bind_endpoint;
    if (_registry->register_local (topic_id, endpoint) != 0) {
        return -1; // errno already set by registry
    }

    return 0;
}

int spot_pubsub_t::topic_destroy (const std::string &topic_id)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check if topic exists
    auto entry = _registry->lookup (topic_id);
    if (!entry.has_value ()) {
        errno = ENOENT;
        return -1;
    }

    if (entry->location != topic_registry_t::topic_location_t::LOCAL) {
        errno = EINVAL;
        return -1;
    }

    // Unregister from topic registry
    _registry->unregister (topic_id);

    return 0;
}

int spot_pubsub_t::topic_route (const std::string &topic_id,
                                 const std::string &endpoint)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check if topic already exists
    if (_registry->has_topic (topic_id)) {
        errno = EEXIST;
        return -1;
    }

    // Register topic as REMOTE in registry
    // We just store the endpoint, the actual connection happens in subscribe()
    if (_registry->register_remote (topic_id, endpoint) != 0) {
        return -1; // errno already set by registry
    }

    return 0;
}

// ============================================================================
// Subscription API
// ============================================================================

int spot_pubsub_t::subscribe (const std::string &topic_id)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Lookup topic in registry
    auto entry = _registry->lookup (topic_id);

    // If topic not found but we have cluster connections, treat as remote subscription
    if (!entry.has_value ()) {
        if (_connected_endpoints.empty ()) {
            errno = ENOENT;
            return -1;
        }

        // Add REMOTE subscription to manager (subscribed through cluster)
        subscription_manager_t::subscriber_t sub;
        sub.type = subscription_manager_t::subscriber_type_t::REMOTE;
        sub.socket = _recv_socket;

        if (_sub_manager->add_subscription (topic_id, sub) != 0) {
            // Already subscribed - not an error, just idempotent
            if (errno == EEXIST) {
                return 0;
            }
            return -1;
        }

        // Send subscription message to all connected cluster endpoints
        msg_t msg;
        if (msg.init_subscribe (topic_id.size (),
                                reinterpret_cast<const unsigned char *> (topic_id.data ())) != 0) {
            return -1;
        }

        int rc = _recv_socket->send (&msg, 0);
        msg.close ();

        return rc;
    }

    if (entry->location == topic_registry_t::topic_location_t::LOCAL) {
        // LOCAL topic: XSUB is already connected to local XPUB
        // Just need to send subscription filter

        // Add LOCAL subscription to manager
        subscription_manager_t::subscriber_t sub;
        sub.type = subscription_manager_t::subscriber_type_t::LOCAL;
        sub.socket = _recv_socket;

        if (_sub_manager->add_subscription (topic_id, sub) != 0) {
            // Already subscribed - not an error, just idempotent
            if (errno == EEXIST) {
                return 0;
            }
            return -1;
        }

        // Send subscription message to XPUB (topic filter)
        msg_t msg;
        if (msg.init_subscribe (topic_id.size (),
                                reinterpret_cast<const unsigned char *> (topic_id.data ())) != 0) {
            return -1;
        }

        int rc = _recv_socket->send (&msg, 0);
        msg.close ();

        return rc;

    } else {
        // REMOTE topic: Connect XSUB to remote XPUB endpoint
        const std::string &remote_endpoint = entry->endpoint;

        // Check if already connected to this endpoint
        if (_connected_endpoints.find (remote_endpoint) == _connected_endpoints.end ()) {
            // Connect XSUB to remote XPUB
            if (_recv_socket->connect (remote_endpoint.c_str ()) != 0) {
                return -1;
            }
            _connected_endpoints.insert (remote_endpoint);
        }

        // Add REMOTE subscription to manager
        subscription_manager_t::subscriber_t sub;
        sub.type = subscription_manager_t::subscriber_type_t::REMOTE;
        sub.socket = _recv_socket;

        if (_sub_manager->add_subscription (topic_id, sub) != 0) {
            // Already subscribed - not an error, just idempotent
            if (errno == EEXIST) {
                return 0;
            }
            return -1;
        }

        // Send subscription message to remote XPUB
        msg_t msg;
        if (msg.init_subscribe (topic_id.size (),
                                reinterpret_cast<const unsigned char *> (topic_id.data ())) != 0) {
            return -1;
        }

        int rc = _recv_socket->send (&msg, 0);
        msg.close ();

        return rc;
    }
}

int spot_pubsub_t::subscribe_pattern (const std::string &pattern)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Pattern subscriptions are LOCAL only
    subscription_manager_t::subscriber_t sub;
    sub.type = subscription_manager_t::subscriber_type_t::LOCAL;
    sub.socket = _recv_socket;

    if (_sub_manager->add_pattern_subscription (pattern, sub) != 0) {
        return -1;
    }

    // Convert glob pattern to XPUB prefix filter
    // XPUB uses prefix matching, not glob patterns
    // "events:*" -> "events:" (matches anything starting with "events:")
    // "events:*:data" -> "events:" (only prefix up to first *)
    std::string prefix = pattern;
    size_t star_pos = prefix.find ('*');
    if (star_pos != std::string::npos) {
        prefix = prefix.substr (0, star_pos);
    }

    // Send subscription filter with prefix
    msg_t msg;
    if (msg.init_subscribe (prefix.size (),
                            reinterpret_cast<const unsigned char *> (prefix.data ())) != 0) {
        return -1;
    }

    int rc = _recv_socket->send (&msg, 0);
    msg.close ();

    return rc;
}

int spot_pubsub_t::subscribe_many (const std::vector<std::string> &topics)
{
    int failed_count = 0;

    for (const auto &topic_id : topics) {
        if (subscribe (topic_id) != 0) {
            failed_count++;
        }
    }

    if (failed_count > 0) {
        // Partial failure - errno from last failed subscribe
        return -1;
    }

    return 0;
}

int spot_pubsub_t::unsubscribe (const std::string &topic_id)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Lookup topic in registry
    auto entry = _registry->lookup (topic_id);
    if (!entry.has_value ()) {
        errno = ENOENT;
        return -1;
    }

    // Remove subscription from manager
    subscription_manager_t::subscriber_t sub;
    sub.type = (entry->location == topic_registry_t::topic_location_t::LOCAL)
                   ? subscription_manager_t::subscriber_type_t::LOCAL
                   : subscription_manager_t::subscriber_type_t::REMOTE;
    sub.socket = _recv_socket;

    if (_sub_manager->remove_subscription (topic_id, sub) != 0) {
        return -1; // errno already set
    }

    // Send unsubscription message to XPUB
    msg_t msg;
    if (msg.init_cancel (topic_id.size (),
                         reinterpret_cast<const unsigned char *> (topic_id.data ())) != 0) {
        return -1;
    }

    int rc = _recv_socket->send (&msg, 0);
    msg.close ();

    return rc;
}

// ============================================================================
// Publishing API
// ============================================================================

int spot_pubsub_t::publish (const std::string &topic_id,
                             const void *data,
                             size_t len)
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    // Lookup topic in registry
    auto entry = _registry->lookup (topic_id);
    if (!entry.has_value ()) {
        errno = ENOENT;
        return -1;
    }

    if (entry->location != topic_registry_t::topic_location_t::LOCAL) {
        // Can only publish to LOCAL topics
        errno = EINVAL;
        return -1;
    }

    // Send message through shared XPUB: [topic_id][data]
    // Frame 1: Topic ID
    msg_t topic_msg;
    if (topic_msg.init_buffer (topic_id.data (), topic_id.size ()) != 0) {
        return -1;
    }

    if (_pub_socket->send (&topic_msg, SL_SNDMORE) != 0) {
        topic_msg.close ();
        return -1;
    }
    topic_msg.close ();

    // Frame 2: Data
    msg_t data_msg;
    if (data_msg.init_buffer (data, len) != 0) {
        return -1;
    }

    int rc = _pub_socket->send (&data_msg, 0);
    data_msg.close ();

    return rc;
}

// ============================================================================
// Receiving API
// ============================================================================

int spot_pubsub_t::recv (char *topic_buf, size_t topic_buf_size,
                          size_t *topic_len,
                          void *data_buf, size_t data_buf_size,
                          size_t *data_len,
                          int flags)
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    // Determine if we should use non-blocking mode or timeout
    bool use_timeout = !(flags & SL_DONTWAIT) && _rcvtimeo != 0;

    // Calculate deadline for timeout
    int timeout = _rcvtimeo;
    uint64_t deadline = 0;
    if (use_timeout && timeout > 0) {
        deadline = _clock.now_ms () + timeout;
    }

    // Retry loop for blocking mode with timeout
    while (true) {
        // Try to receive from XSUB
        msg_t topic_msg;
        if (topic_msg.init () != 0) {
            return -1;
        }

        int rc = _recv_socket->recv (&topic_msg, SL_DONTWAIT);
        if (rc == 0) {
            // Got message
            // Copy topic to buffer
            size_t topic_size = topic_msg.size ();
            if (topic_size > topic_buf_size) {
                topic_msg.close ();
                errno = EMSGSIZE;
                return -1;
            }

            memcpy (topic_buf, topic_msg.data (), topic_size);
            *topic_len = topic_size;
            topic_msg.close ();

            // Receive Frame 2: Data
            msg_t data_msg;
            if (data_msg.init () != 0) {
                return -1;
            }

            if (_recv_socket->recv (&data_msg, 0) != 0) {
                data_msg.close ();
                return -1;
            }

            // Copy data to buffer
            size_t data_size = data_msg.size ();
            if (data_size > data_buf_size) {
                data_msg.close ();
                errno = EMSGSIZE;
                return -1;
            }

            memcpy (data_buf, data_msg.data (), data_size);
            *data_len = data_size;
            data_msg.close ();

            return 0;
        }

        topic_msg.close ();

        // No message available
        if (flags & SL_DONTWAIT) {
            errno = EAGAIN;
            return -1;
        }

        // Check timeout
        if (timeout > 0) {
            uint64_t now = _clock.now_ms ();
            if (now >= deadline) {
                errno = EAGAIN;
                return -1;
            }
        } else if (timeout == 0) {
            errno = EAGAIN;
            return -1;
        }

        // If we reach here in blocking mode, sleep briefly to avoid busy-waiting
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
}

// ============================================================================
// Introspection API
// ============================================================================

std::vector<std::string> spot_pubsub_t::list_topics () const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);
    return _registry->get_all_topics ();
}

bool spot_pubsub_t::topic_exists (const std::string &topic_id) const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);
    return _registry->has_topic (topic_id);
}

bool spot_pubsub_t::topic_is_local (const std::string &topic_id) const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    auto entry = _registry->lookup (topic_id);
    if (!entry.has_value ()) {
        return false;
    }

    return entry->location == topic_registry_t::topic_location_t::LOCAL;
}

// ============================================================================
// Configuration API
// ============================================================================

int spot_pubsub_t::set_hwm (int sndhwm, int rcvhwm)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    _sndhwm = sndhwm;
    _rcvhwm = rcvhwm;

    // Apply to receive socket
    if (_recv_socket) {
        if (_recv_socket->setsockopt (SL_RCVHWM, &_rcvhwm, sizeof (_rcvhwm)) != 0) {
            return -1;
        }
    }

    // Apply to publish socket
    if (_pub_socket) {
        if (_pub_socket->setsockopt (SL_SNDHWM, &_sndhwm, sizeof (_sndhwm)) != 0) {
            return -1;
        }
    }

    return 0;
}

int spot_pubsub_t::setsockopt (int option, const void *value, size_t len)
{
    if (!value) {
        errno = EINVAL;
        return -1;
    }

    switch (option) {
        case SL_RCVTIMEO:
            if (len != sizeof (int)) {
                errno = EINVAL;
                return -1;
            }
            _rcvtimeo = *static_cast<const int *> (value);
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

int spot_pubsub_t::getsockopt (int option, void *value, size_t *len) const
{
    if (!value || !len) {
        errno = EINVAL;
        return -1;
    }

    switch (option) {
        case SL_RCVTIMEO:
            if (*len < sizeof (int)) {
                errno = EINVAL;
                return -1;
            }
            *static_cast<int *> (value) = _rcvtimeo;
            *len = sizeof (int);
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

// ============================================================================
// Cluster Management
// ============================================================================

int spot_pubsub_t::bind (const std::string &endpoint)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check if already bound to this endpoint
    if (_bind_endpoints.find (endpoint) != _bind_endpoints.end ()) {
        errno = EEXIST;
        return -1;
    }

    // Bind shared XPUB to the endpoint (in addition to inproc)
    if (_pub_socket->bind (endpoint.c_str ()) != 0) {
        return -1;
    }

    _bind_endpoints.insert (endpoint);

    // Keep first TCP endpoint as primary for topic registration
    if (_bind_endpoint.empty () && endpoint.find ("tcp://") != std::string::npos) {
        _bind_endpoint = endpoint;
    }

    return 0;
}

int spot_pubsub_t::cluster_add (const std::string &endpoint)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check if already connected
    if (_connected_endpoints.find (endpoint) != _connected_endpoints.end ()) {
        errno = EEXIST;
        return -1;
    }

    // Connect XSUB to remote XPUB
    if (_recv_socket->connect (endpoint.c_str ()) != 0) {
        return -1;
    }

    _connected_endpoints.insert (endpoint);

    return 0;
}

int spot_pubsub_t::cluster_remove (const std::string &endpoint)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    auto it = _connected_endpoints.find (endpoint);
    if (it == _connected_endpoints.end ()) {
        errno = ENOENT;
        return -1;
    }

    // Disconnect XSUB from remote XPUB using term_endpoint
    if (_recv_socket->term_endpoint (endpoint.c_str ()) != 0) {
        // Endpoint might already be disconnected, continue anyway
        if (errno != ENOENT) {
            return -1;
        }
    }

    _connected_endpoints.erase (it);

    return 0;
}

int spot_pubsub_t::cluster_sync (int timeout_ms)
{
    // With the simplified XPUB/XSUB architecture, cluster sync is not needed
    // Topics are discovered through subscription messages
    (void) timeout_ms;
    return 0;
}

// ============================================================================
// Event Loop Integration
// ============================================================================

int spot_pubsub_t::fd (int *fd) const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    if (!_recv_socket) {
        errno = EINVAL;
        return -1;
    }

    // Get file descriptor from receive socket
    size_t fd_size = sizeof (*fd);
    return _recv_socket->getsockopt (SL_FD, fd, &fd_size);
}

} // namespace slk

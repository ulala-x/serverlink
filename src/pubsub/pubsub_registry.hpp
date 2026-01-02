/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Pub/Sub Introspection Registry */

#ifndef SL_PUBSUB_REGISTRY_HPP_INCLUDED
#define SL_PUBSUB_REGISTRY_HPP_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include "../util/config.hpp"
#include "../util/mutex.hpp"

namespace slk
{

/**
 * Thread-safe registry for tracking pub/sub subscriptions
 *
 * This class provides introspection capabilities for pub/sub patterns,
 * tracking active channels and pattern subscriptions across the system.
 *
 * Thread Safety:
 * - Uses slk::mutex_t for synchronization
 * - All public methods are thread-safe
 * - Simple mutex-based locking (not reader-writer)
 */
class pubsub_registry_t
{
  public:
    pubsub_registry_t ();
    ~pubsub_registry_t ();

    // === Channel Subscription Management (exact matching) ===

    /**
     * Register a subscription to a channel
     * @param channel The channel name
     */
    void register_subscription (const std::string &channel_);

    /**
     * Unregister a subscription from a channel
     * @param channel The channel name
     */
    void unregister_subscription (const std::string &channel_);

    // === Pattern Subscription Management (glob patterns) ===

    /**
     * Register a pattern subscription
     * @param pattern The glob pattern (e.g., "news.*")
     */
    void register_pattern (const std::string &pattern_);

    /**
     * Unregister a pattern subscription
     * @param pattern The glob pattern
     */
    void unregister_pattern (const std::string &pattern_);

    // === Introspection API (Redis-compatible) ===

    /**
     * Get list of active channels matching a pattern
     * @param pattern Glob pattern (empty string matches all)
     * @return Vector of channel names
     */
    std::vector<std::string> get_channels (const std::string &pattern_) const;

    /**
     * Get subscriber count for a specific channel
     * @param channel The channel name
     * @return Number of subscribers (0 if channel not found)
     */
    size_t get_numsub (const std::string &channel_) const;

    /**
     * Get total number of pattern subscriptions
     * @return Total pattern subscription count
     */
    size_t get_numpat () const;

    // === Statistics ===

    /**
     * Get total number of active channels
     * @return Channel count
     */
    size_t get_channel_count () const;

    /**
     * Get total number of subscriptions across all channels
     * @return Total subscription count
     */
    size_t get_total_subscriptions () const;

  private:
    // Mutex for thread safety
    mutable mutex_t _mutex;

    // Channel name → subscriber count
    std::unordered_map<std::string, size_t> _channel_subscribers;

    // Pattern string → subscriber count
    std::unordered_map<std::string, size_t> _pattern_subscribers;

    SL_NON_COPYABLE_NOR_MOVABLE (pubsub_registry_t)
};

} // namespace slk

#endif

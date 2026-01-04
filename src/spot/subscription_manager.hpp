/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - SPOT Subscription Manager */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>

namespace slk
{

/**
 * @brief Subscription Manager for SPOT PUB/SUB
 *
 * Manages topic-based subscriptions for both local and remote subscribers.
 * Supports exact and pattern-based subscriptions with efficient lookup.
 *
 * Features:
 * - Exact topic subscriptions (LOCAL and REMOTE)
 * - Pattern subscriptions with wildcard matching (LOCAL only)
 * - Thread-safe operations with shared_mutex (read-optimized)
 * - Efficient subscriber lookup per topic
 *
 * Pattern Matching Rules:
 * - '*' wildcard: prefix matching (e.g., "player:*" matches "player:123")
 * - Pattern subscriptions are LOCAL-only (design decision from plan)
 *
 * Thread-safety:
 *   - Read operations use shared_lock (concurrent reads)
 *   - Write operations use unique_lock (exclusive writes)
 */
class subscription_manager_t
{
  public:
    /**
     * @brief Subscriber type (local XSUB socket or remote node)
     */
    enum class subscriber_type_t { LOCAL, REMOTE };

    /**
     * @brief Subscriber information
     */
    struct subscriber_t
    {
        subscriber_type_t type;
        void *socket;         // LOCAL: XSUB socket pointer
        std::string endpoint; // REMOTE: tcp://host:port

        // Equality comparison for finding subscribers
        bool operator== (const subscriber_t &other) const
        {
            if (type != other.type)
                return false;
            if (type == subscriber_type_t::LOCAL)
                return socket == other.socket;
            else
                return endpoint == other.endpoint;
        }

        // Hash support for use in unordered containers
        struct hash
        {
            size_t operator() (const subscriber_t &sub) const
            {
                if (sub.type == subscriber_type_t::LOCAL) {
                    return std::hash<void *>{}(sub.socket);
                } else {
                    return std::hash<std::string>{}(sub.endpoint);
                }
            }
        };
    };

    /**
     * @brief Construct a new subscription manager
     */
    subscription_manager_t () = default;

    /**
     * @brief Destroy the subscription manager
     */
    ~subscription_manager_t () = default;

    // Non-copyable and non-movable
    subscription_manager_t (const subscription_manager_t &) = delete;
    subscription_manager_t &operator= (const subscription_manager_t &) = delete;
    subscription_manager_t (subscription_manager_t &&) = delete;
    subscription_manager_t &operator= (subscription_manager_t &&) = delete;

    /**
     * @brief Add an exact topic subscription
     *
     * @param topic_id Topic identifier
     * @param subscriber Subscriber information
     * @return 0 on success, -1 with errno set on error
     *         errno = EEXIST if subscription already exists
     */
    int add_subscription (const std::string &topic_id, const subscriber_t &subscriber);

    /**
     * @brief Remove an exact topic subscription
     *
     * @param topic_id Topic identifier
     * @param subscriber Subscriber information
     * @return 0 on success, -1 with errno set on error
     *         errno = ENOENT if subscription not found
     */
    int remove_subscription (const std::string &topic_id, const subscriber_t &subscriber);

    /**
     * @brief Remove all subscriptions for a subscriber
     *
     * Removes both exact and pattern subscriptions.
     *
     * @param subscriber Subscriber information
     * @return 0 on success, -1 on error
     */
    int remove_all_subscriptions (const subscriber_t &subscriber);

    /**
     * @brief Add a pattern subscription (LOCAL only)
     *
     * Pattern subscriptions use prefix matching with '*' wildcard.
     * Example: "player:*" matches "player:123", "player:456"
     *
     * @param pattern Pattern string with optional '*' wildcard
     * @param subscriber Subscriber information (must be LOCAL)
     * @return 0 on success, -1 with errno set on error
     *         errno = EEXIST if pattern subscription already exists
     *         errno = EINVAL if subscriber is not LOCAL
     */
    int add_pattern_subscription (const std::string &pattern, const subscriber_t &subscriber);

    /**
     * @brief Remove a pattern subscription
     *
     * @param pattern Pattern string
     * @param subscriber Subscriber information
     * @return 0 on success, -1 with errno set on error
     *         errno = ENOENT if pattern subscription not found
     */
    int remove_pattern_subscription (const std::string &pattern,
                                      const subscriber_t &subscriber);

    /**
     * @brief Get exact subscribers for a topic
     *
     * Returns subscribers with exact topic match only.
     * Does not include pattern-matched subscribers.
     *
     * @param topic_id Topic identifier
     * @return Vector of subscribers
     */
    std::vector<subscriber_t> get_subscribers (const std::string &topic_id) const;

    /**
     * @brief Get exact subscriber count for a topic
     *
     * @param topic_id Topic identifier
     * @return Number of exact subscribers
     */
    size_t get_subscriber_count (const std::string &topic_id) const;

    /**
     * @brief Get pattern-matched subscribers for a topic
     *
     * Returns LOCAL subscribers whose patterns match the given topic_id.
     *
     * @param topic_id Topic identifier
     * @return Vector of pattern-matched LOCAL subscribers
     */
    std::vector<subscriber_t> get_pattern_matched_subscribers (const std::string &topic_id) const;

    /**
     * @brief Get all subscribed topics for a subscriber (exact only)
     *
     * @param subscriber Subscriber information
     * @return Vector of topic IDs
     */
    std::vector<std::string> get_subscribed_topics (const subscriber_t &subscriber) const;

    /**
     * @brief Get total subscription count (exact + pattern)
     *
     * @return Total number of subscriptions
     */
    size_t total_subscription_count () const;

  private:
    // topic_id → subscribers (exact subscriptions)
    std::unordered_map<std::string, std::vector<subscriber_t>> _exact_subscriptions;

    // pattern → subscribers (pattern subscriptions, LOCAL only)
    std::unordered_map<std::string, std::vector<subscriber_t>> _pattern_subscriptions;

    // Thread safety (read-optimized)
    mutable std::shared_mutex _mutex;

    /**
     * @brief Check if pattern matches topic_id (prefix matching)
     *
     * Pattern matching rules:
     * - "player:*" matches "player:123", "player:456"
     * - "*" matches everything
     * - "exact" matches only "exact"
     *
     * @param pattern Pattern string with optional '*' wildcard
     * @param topic_id Topic identifier to match
     * @return true if pattern matches topic_id
     */
    bool matches_pattern (const std::string &pattern, const std::string &topic_id) const;
};

} // namespace slk

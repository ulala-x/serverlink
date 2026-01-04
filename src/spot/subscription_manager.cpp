/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - SPOT Subscription Manager */

#include "subscription_manager.hpp"
#include "../util/err.hpp"

#include <algorithm>

namespace slk
{

int subscription_manager_t::add_subscription (const std::string &topic_id,
                                              const subscriber_t &subscriber)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    auto &subscribers = _exact_subscriptions[topic_id];

    // Check for duplicate subscription
    auto it = std::find (subscribers.begin (), subscribers.end (), subscriber);
    if (it != subscribers.end ()) {
        errno = EEXIST;
        return -1;
    }

    subscribers.push_back (subscriber);
    return 0;
}

int subscription_manager_t::remove_subscription (const std::string &topic_id,
                                                 const subscriber_t &subscriber)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    auto topic_it = _exact_subscriptions.find (topic_id);
    if (topic_it == _exact_subscriptions.end ()) {
        errno = ENOENT;
        return -1;
    }

    auto &subscribers = topic_it->second;
    auto sub_it = std::find (subscribers.begin (), subscribers.end (), subscriber);
    if (sub_it == subscribers.end ()) {
        errno = ENOENT;
        return -1;
    }

    subscribers.erase (sub_it);

    // Clean up empty topic entry
    if (subscribers.empty ()) {
        _exact_subscriptions.erase (topic_it);
    }

    return 0;
}

int subscription_manager_t::remove_all_subscriptions (const subscriber_t &subscriber)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Remove from exact subscriptions
    for (auto it = _exact_subscriptions.begin (); it != _exact_subscriptions.end ();) {
        auto &subscribers = it->second;
        auto sub_it = std::find (subscribers.begin (), subscribers.end (), subscriber);
        if (sub_it != subscribers.end ()) {
            subscribers.erase (sub_it);
        }

        // Clean up empty topic entry
        if (subscribers.empty ()) {
            it = _exact_subscriptions.erase (it);
        } else {
            ++it;
        }
    }

    // Remove from pattern subscriptions
    for (auto it = _pattern_subscriptions.begin (); it != _pattern_subscriptions.end ();) {
        auto &subscribers = it->second;
        auto sub_it = std::find (subscribers.begin (), subscribers.end (), subscriber);
        if (sub_it != subscribers.end ()) {
            subscribers.erase (sub_it);
        }

        // Clean up empty pattern entry
        if (subscribers.empty ()) {
            it = _pattern_subscriptions.erase (it);
        } else {
            ++it;
        }
    }

    return 0;
}

int subscription_manager_t::add_pattern_subscription (const std::string &pattern,
                                                      const subscriber_t &subscriber)
{
    // Pattern subscriptions are LOCAL-only
    if (subscriber.type != subscriber_type_t::LOCAL) {
        errno = EINVAL;
        return -1;
    }

    std::unique_lock<std::shared_mutex> lock (_mutex);

    auto &subscribers = _pattern_subscriptions[pattern];

    // Check for duplicate subscription
    auto it = std::find (subscribers.begin (), subscribers.end (), subscriber);
    if (it != subscribers.end ()) {
        errno = EEXIST;
        return -1;
    }

    subscribers.push_back (subscriber);
    return 0;
}

int subscription_manager_t::remove_pattern_subscription (const std::string &pattern,
                                                         const subscriber_t &subscriber)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    auto pattern_it = _pattern_subscriptions.find (pattern);
    if (pattern_it == _pattern_subscriptions.end ()) {
        errno = ENOENT;
        return -1;
    }

    auto &subscribers = pattern_it->second;
    auto sub_it = std::find (subscribers.begin (), subscribers.end (), subscriber);
    if (sub_it == subscribers.end ()) {
        errno = ENOENT;
        return -1;
    }

    subscribers.erase (sub_it);

    // Clean up empty pattern entry
    if (subscribers.empty ()) {
        _pattern_subscriptions.erase (pattern_it);
    }

    return 0;
}

std::vector<subscription_manager_t::subscriber_t>
subscription_manager_t::get_subscribers (const std::string &topic_id) const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    auto it = _exact_subscriptions.find (topic_id);
    if (it != _exact_subscriptions.end ()) {
        return it->second;
    }

    return {};
}

size_t subscription_manager_t::get_subscriber_count (const std::string &topic_id) const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    auto it = _exact_subscriptions.find (topic_id);
    if (it != _exact_subscriptions.end ()) {
        return it->second.size ();
    }

    return 0;
}

std::vector<subscription_manager_t::subscriber_t>
subscription_manager_t::get_pattern_matched_subscribers (const std::string &topic_id) const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    std::vector<subscriber_t> matched;

    // Check all pattern subscriptions
    for (const auto &[pattern, subscribers] : _pattern_subscriptions) {
        if (matches_pattern (pattern, topic_id)) {
            // Add all subscribers for this pattern
            matched.insert (matched.end (), subscribers.begin (), subscribers.end ());
        }
    }

    return matched;
}

std::vector<std::string>
subscription_manager_t::get_subscribed_topics (const subscriber_t &subscriber) const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    std::vector<std::string> topics;

    // Find all topics where this subscriber is present
    for (const auto &[topic_id, subscribers] : _exact_subscriptions) {
        auto it = std::find (subscribers.begin (), subscribers.end (), subscriber);
        if (it != subscribers.end ()) {
            topics.push_back (topic_id);
        }
    }

    return topics;
}

size_t subscription_manager_t::total_subscription_count () const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    size_t count = 0;

    // Count exact subscriptions
    for (const auto &[topic_id, subscribers] : _exact_subscriptions) {
        (void) topic_id; // Unused
        count += subscribers.size ();
    }

    // Count pattern subscriptions
    for (const auto &[pattern, subscribers] : _pattern_subscriptions) {
        (void) pattern; // Unused
        count += subscribers.size ();
    }

    return count;
}

bool subscription_manager_t::matches_pattern (const std::string &pattern,
                                              const std::string &topic_id) const
{
    // Handle exact match (no wildcard)
    if (pattern.find ('*') == std::string::npos) {
        return pattern == topic_id;
    }

    // Handle universal wildcard
    if (pattern == "*") {
        return true;
    }

    // Handle prefix matching: "player:*" matches "player:123"
    size_t wildcard_pos = pattern.find ('*');
    if (wildcard_pos != std::string::npos) {
        std::string prefix = pattern.substr (0, wildcard_pos);
        return topic_id.compare (0, prefix.size (), prefix) == 0;
    }

    return false;
}

} // namespace slk

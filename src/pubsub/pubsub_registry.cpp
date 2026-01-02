/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Pub/Sub Introspection Registry */

#include "pubsub_registry.hpp"
#include "../pattern/glob_pattern.hpp"
#include <algorithm>

namespace slk
{

pubsub_registry_t::pubsub_registry_t ()
{
}

pubsub_registry_t::~pubsub_registry_t ()
{
}

// === Channel Subscription Management ===

void pubsub_registry_t::register_subscription (const std::string &channel_)
{
    scoped_lock_t lock (_mutex);
    _channel_subscribers[channel_]++;
}

void pubsub_registry_t::unregister_subscription (const std::string &channel_)
{
    scoped_lock_t lock (_mutex);

    auto it = _channel_subscribers.find (channel_);
    if (it != _channel_subscribers.end ()) {
        if (--it->second == 0) {
            // Remove channel if no more subscribers
            _channel_subscribers.erase (it);
        }
    }
}

// === Pattern Subscription Management ===

void pubsub_registry_t::register_pattern (const std::string &pattern_)
{
    scoped_lock_t lock (_mutex);
    _pattern_subscribers[pattern_]++;
}

void pubsub_registry_t::unregister_pattern (const std::string &pattern_)
{
    scoped_lock_t lock (_mutex);

    auto it = _pattern_subscribers.find (pattern_);
    if (it != _pattern_subscribers.end ()) {
        if (--it->second == 0) {
            // Remove pattern if no more subscribers
            _pattern_subscribers.erase (it);
        }
    }
}

// === Introspection API ===

std::vector<std::string>
pubsub_registry_t::get_channels (const std::string &pattern_) const
{
    scoped_lock_t lock (_mutex);

    std::vector<std::string> result;
    result.reserve (_channel_subscribers.size ());

    if (pattern_.empty () || pattern_ == "*") {
        // Return all channels
        for (const auto &entry : _channel_subscribers) {
            result.push_back (entry.first);
        }
    } else {
        // Match channels against pattern
        glob_pattern_t glob (pattern_);
        for (const auto &entry : _channel_subscribers) {
            if (glob.match (
                  reinterpret_cast<const unsigned char *> (entry.first.data ()),
                  entry.first.size ())) {
                result.push_back (entry.first);
            }
        }
    }

    // Sort for consistent ordering
    std::sort (result.begin (), result.end ());
    return result;
}

size_t pubsub_registry_t::get_numsub (const std::string &channel_) const
{
    scoped_lock_t lock (_mutex);

    auto it = _channel_subscribers.find (channel_);
    if (it != _channel_subscribers.end ()) {
        return it->second;
    }
    return 0;
}

size_t pubsub_registry_t::get_numpat () const
{
    scoped_lock_t lock (_mutex);

    // Sum up all pattern subscriber counts
    size_t total = 0;
    for (const auto &entry : _pattern_subscribers) {
        total += entry.second;
    }
    return total;
}

// === Statistics ===

size_t pubsub_registry_t::get_channel_count () const
{
    scoped_lock_t lock (_mutex);
    return _channel_subscribers.size ();
}

size_t pubsub_registry_t::get_total_subscriptions () const
{
    scoped_lock_t lock (_mutex);

    size_t total = 0;
    for (const auto &entry : _channel_subscribers) {
        total += entry.second;
    }
    return total;
}

} // namespace slk

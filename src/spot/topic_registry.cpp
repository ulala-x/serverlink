/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - SPOT Topic Registry */

#include "topic_registry.hpp"
#include "../util/err.hpp"

#include <mutex>
#include <sstream>

namespace slk
{

topic_registry_t::topic_registry_t () : _local_topic_counter (0)
{
}

topic_registry_t::~topic_registry_t ()
{
}

int topic_registry_t::register_local (const std::string &topic_id)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check for duplicate registration
    auto it = _topics.find (topic_id);
    if (it != _topics.end ()) {
        errno = EEXIST;
        return -1;
    }

    // Generate unique inproc endpoint: inproc://spot-{counter}
    std::ostringstream oss;
    oss << "inproc://spot-" << _local_topic_counter++;

    // Create topic entry
    topic_entry_t entry;
    entry.topic_id = topic_id;
    entry.location = topic_location_t::LOCAL;
    entry.endpoint = oss.str ();

    // Insert into map
    _topics[topic_id] = entry;

    return 0;
}

int topic_registry_t::register_local (const std::string &topic_id,
                                       const std::string &endpoint)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check for duplicate registration
    auto it = _topics.find (topic_id);
    if (it != _topics.end ()) {
        errno = EEXIST;
        return -1;
    }

    // Create topic entry with provided endpoint
    topic_entry_t entry;
    entry.topic_id = topic_id;
    entry.location = topic_location_t::LOCAL;
    entry.endpoint = endpoint;

    // Insert into map
    _topics[topic_id] = entry;

    return 0;
}

int topic_registry_t::register_remote (const std::string &topic_id,
                                        const std::string &tcp_endpoint)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    // Check for duplicate registration
    auto it = _topics.find (topic_id);
    if (it != _topics.end ()) {
        errno = EEXIST;
        return -1;
    }

    // Create topic entry
    topic_entry_t entry;
    entry.topic_id = topic_id;
    entry.location = topic_location_t::REMOTE;
    entry.endpoint = tcp_endpoint;

    // Insert into map
    _topics[topic_id] = entry;

    return 0;
}

int topic_registry_t::unregister (const std::string &topic_id)
{
    std::unique_lock<std::shared_mutex> lock (_mutex);

    auto it = _topics.find (topic_id);
    if (it == _topics.end ()) {
        errno = ENOENT;
        return -1;
    }

    _topics.erase (it);
    return 0;
}

std::optional<topic_registry_t::topic_entry_t>
topic_registry_t::lookup (const std::string &topic_id) const
{
    // Use shared_lock for read access (allows multiple concurrent readers)
    std::shared_lock<std::shared_mutex> lock (_mutex);

    auto it = _topics.find (topic_id);
    if (it == _topics.end ())
        return std::nullopt;

    // Return a copy to avoid pointer lifetime issues
    return it->second;
}

bool topic_registry_t::has_topic (const std::string &topic_id) const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);
    return _topics.find (topic_id) != _topics.end ();
}

std::vector<std::string> topic_registry_t::get_all_topics () const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    std::vector<std::string> result;
    result.reserve (_topics.size ());

    for (const auto &[topic_id, entry] : _topics) {
        result.push_back (topic_id);
    }

    return result;
}

std::vector<std::string> topic_registry_t::get_local_topics () const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    std::vector<std::string> result;
    for (const auto &[topic_id, entry] : _topics) {
        if (entry.location == topic_location_t::LOCAL) {
            result.push_back (topic_id);
        }
    }

    return result;
}

std::vector<std::string> topic_registry_t::get_remote_topics () const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);

    std::vector<std::string> result;
    for (const auto &[topic_id, entry] : _topics) {
        if (entry.location == topic_location_t::REMOTE) {
            result.push_back (topic_id);
        }
    }

    return result;
}

size_t topic_registry_t::topic_count () const
{
    std::shared_lock<std::shared_mutex> lock (_mutex);
    return _topics.size ();
}

} // namespace slk

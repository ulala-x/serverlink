/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - SPOT Topic Registry */

#ifndef SL_TOPIC_REGISTRY_HPP_INCLUDED
#define SL_TOPIC_REGISTRY_HPP_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>

namespace slk
{

/**
 * @brief Topic Registry for SPOT PUB/SUB
 *
 * Manages topic_id → endpoint mappings using ROUTER socket's _out_pipes pattern.
 * Provides O(1) hash-based lookup for topic routing decisions.
 *
 * Design:
 *   - LOCAL topics: Mapped to inproc://spot-{counter} endpoints
 *   - REMOTE topics: Mapped to tcp://host:port endpoints
 *   - Thread-safe with shared_mutex (read-heavy workload)
 *   - Similar to ROUTER's routing_id → out_pipe_t mapping
 *
 * Thread-safety:
 *   - lookup operations use shared_lock (multiple concurrent readers)
 *   - register/unregister use unique_lock (exclusive writer)
 */
class topic_registry_t
{
  public:
    /**
     * @brief Topic location type
     */
    enum class topic_location_t
    {
        LOCAL,  // inproc://spot-xxx
        REMOTE  // tcp://host:port
    };

    /**
     * @brief Topic entry (similar to ROUTER's out_pipe_t)
     */
    struct topic_entry_t
    {
        std::string topic_id;
        topic_location_t location;
        std::string endpoint;  // LOCAL: inproc://spot-xxx, REMOTE: tcp://host:port
    };

    /**
     * @brief Construct a new topic registry
     */
    topic_registry_t ();

    /**
     * @brief Destroy the topic registry
     */
    ~topic_registry_t ();

    // Non-copyable and non-movable
    topic_registry_t (const topic_registry_t &) = delete;
    topic_registry_t &operator= (const topic_registry_t &) = delete;
    topic_registry_t (topic_registry_t &&) = delete;
    topic_registry_t &operator= (topic_registry_t &&) = delete;

    /**
     * @brief Register a LOCAL topic
     *
     * Creates an inproc endpoint for the topic.
     *
     * @param topic_id Topic identifier
     * @return 0 on success, -1 on error (sets errno to EEXIST if already registered)
     */
    int register_local (const std::string &topic_id);

    /**
     * @brief Register a REMOTE topic
     *
     * Maps topic to a TCP endpoint.
     *
     * @param topic_id Topic identifier
     * @param tcp_endpoint TCP endpoint (e.g., "tcp://192.168.1.100:5555")
     * @return 0 on success, -1 on error (sets errno to EEXIST if already registered)
     */
    int register_remote (const std::string &topic_id,
                         const std::string &tcp_endpoint);

    /**
     * @brief Unregister a topic
     *
     * @param topic_id Topic identifier
     * @return 0 on success, -1 on error (sets errno to ENOENT if not found)
     */
    int unregister (const std::string &topic_id);

    /**
     * @brief Lookup topic entry (similar to ROUTER's lookup_out_pipe)
     *
     * O(1) hash-based lookup.
     *
     * @param topic_id Topic identifier
     * @return Pointer to topic entry, or nullptr if not found
     */
    topic_entry_t *lookup (const std::string &topic_id);

    /**
     * @brief Lookup topic entry (const version)
     *
     * @param topic_id Topic identifier
     * @return Const pointer to topic entry, or nullptr if not found
     */
    const topic_entry_t *lookup (const std::string &topic_id) const;

    /**
     * @brief Check if topic is registered
     *
     * @param topic_id Topic identifier
     * @return true if registered, false otherwise
     */
    bool has_topic (const std::string &topic_id) const;

    /**
     * @brief Get all topic IDs
     *
     * @return Vector of all topic IDs
     */
    std::vector<std::string> get_all_topics () const;

    /**
     * @brief Get LOCAL topic IDs
     *
     * @return Vector of LOCAL topic IDs
     */
    std::vector<std::string> get_local_topics () const;

    /**
     * @brief Get REMOTE topic IDs
     *
     * @return Vector of REMOTE topic IDs
     */
    std::vector<std::string> get_remote_topics () const;

    /**
     * @brief Get total topic count
     *
     * @return Number of registered topics
     */
    size_t topic_count () const;

  private:
    // Topic map: topic_id → topic_entry_t (similar to ROUTER's _out_pipes)
    std::unordered_map<std::string, topic_entry_t> _topics;

    // Reader-writer lock (read-heavy workload)
    mutable std::shared_mutex _mutex;

    // Counter for generating unique inproc endpoints
    uint64_t _local_topic_counter;
};

} // namespace slk

#endif

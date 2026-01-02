/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Redis-style Sharded Pub/Sub Manager */

#ifndef SL_SHARDED_PUBSUB_HPP_INCLUDED
#define SL_SHARDED_PUBSUB_HPP_INCLUDED

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "../util/config.hpp"
#include "../util/mutex.hpp"

namespace slk
{

class ctx_t;
class socket_base_t;

/**
 * Thread-safe sharded pub/sub manager for single-process distribution
 *
 * This class provides Redis-style sharded pub/sub functionality within a
 * single process. Channels are distributed across multiple shards using
 * CRC16 hash, allowing parallel processing and reduced lock contention.
 *
 * Architecture:
 *   - Each shard has its own XPUB socket (inproc transport)
 *   - Publishers send to the shard determined by channel hash
 *   - Subscribers connect to the appropriate shard(s)
 *   - Fine-grained locking: separate mutex per shard
 *
 * Thread Safety:
 *   - All public methods are thread-safe
 *   - Uses per-shard mutexes for fine-grained locking
 *   - Minimizes contention across different shards
 *
 * Limitations:
 *   - Pattern subscriptions (PSUBSCRIBE) are NOT supported
 *     (would require broadcasting to all shards, defeating the purpose)
 *   - Single process only (use pubsub_cluster_t for multi-process)
 */
class sharded_pubsub_t
{
  public:
    /**
     * Create a sharded pub/sub manager
     *
     * @param ctx Context to use for creating sockets
     * @param shard_count Number of shards (default: 16, max: 1024)
     */
    sharded_pubsub_t (ctx_t *ctx_, int shard_count_);

    /**
     * Destructor - closes all shard sockets
     */
    ~sharded_pubsub_t ();

    // === Publishing API ===

    /**
     * Publish a message to a channel
     *
     * The channel is hashed to determine which shard to publish to.
     * Hash tags are supported: {tag}channel hashes "tag" only.
     *
     * Thread-safe: can be called from multiple threads concurrently.
     *
     * @param channel Channel name (supports {tag} hash tags)
     * @param data Message data pointer
     * @param len Message data length
     * @return Number of bytes published, -1 on error (sets errno)
     */
    int publish (const std::string &channel_,
                 const void *data_,
                 size_t len_);

    // === Subscription API ===

    /**
     * Subscribe a SUB socket to a channel
     *
     * Connects the SUB socket to the appropriate shard and sets up
     * the subscription filter.
     *
     * Thread-safe: can be called from multiple threads concurrently.
     *
     * Note: The SUB socket must be in a state that allows connections.
     *       Typically created by the caller before calling this method.
     *
     * @param sub_socket SUB socket to subscribe (from slk_socket(ctx, SLK_SUB))
     * @param channel Channel name to subscribe to
     * @return 0 on success, -1 on error (sets errno)
     */
    int subscribe (socket_base_t *sub_socket_, const std::string &channel_);

    /**
     * Unsubscribe a SUB socket from a channel
     *
     * Removes the subscription filter from the SUB socket.
     * The socket remains connected to the shard.
     *
     * Thread-safe: can be called from multiple threads concurrently.
     *
     * @param sub_socket SUB socket to unsubscribe
     * @param channel Channel name to unsubscribe from
     * @return 0 on success, -1 on error (sets errno)
     */
    int unsubscribe (socket_base_t *sub_socket_,
                     const std::string &channel_);

    // === Configuration ===

    /**
     * Set high water mark for all shards
     *
     * Controls the maximum number of messages queued per shard.
     * Default is 1000 messages.
     *
     * Thread-safe: can be called from multiple threads concurrently.
     *
     * @param hwm High water mark value (messages)
     * @return 0 on success, -1 on error
     */
    int set_hwm (int hwm_);

    // === Introspection ===

    /**
     * Get the shard index for a channel
     *
     * Returns which shard (0 to shard_count-1) handles this channel.
     *
     * @param channel Channel name
     * @return Shard index (0-based)
     */
    int get_shard_for_channel (const std::string &channel_) const;

    /**
     * Get the number of shards
     *
     * @return Shard count
     */
    int get_shard_count () const { return _shard_count; }

    /**
     * Get the inproc endpoint for a shard
     *
     * @param shard_index Shard index (0 to shard_count-1)
     * @return Endpoint string (e.g., "inproc://shard-0")
     */
    std::string get_shard_endpoint (int shard_index_) const;

  private:
    // Context reference
    ctx_t *_ctx;

    // Number of shards
    int _shard_count;

    // High water mark
    int _hwm;

    // XPUB sockets for each shard (inproc)
    std::vector<socket_base_t *> _shard_pubs;

    // Inproc endpoint for each shard
    std::vector<std::string> _shard_endpoints;

    // Per-shard mutexes for fine-grained locking
    std::vector<std::unique_ptr<mutex_t>> _shard_mutexes;

    // Track which shards each subscriber has connected to
    // sub_socket -> set of shard indices
    std::unordered_map<socket_base_t *, std::vector<int>> _subscriber_shards;
    mutex_t _subscriber_map_mutex;

    // Helper: Create all shard sockets
    bool create_shards ();

    // Helper: Get shard index from channel name
    int hash_channel (const std::string &channel_) const;

    SL_NON_COPYABLE_NOR_MOVABLE (sharded_pubsub_t)
};

} // namespace slk

#endif

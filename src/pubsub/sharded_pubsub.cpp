/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Redis-style Sharded Pub/Sub Manager */

#include "sharded_pubsub.hpp"
#include "shard_hash.hpp"
#include "../core/ctx.hpp"
#include "../core/socket_base.hpp"
#include "../util/err.hpp"
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <atomic>

namespace slk
{

// Static counter for unique instance IDs
static std::atomic<uint64_t> instance_counter (0);

sharded_pubsub_t::sharded_pubsub_t (ctx_t *ctx_, int shard_count_)
  : _ctx (ctx_),
    _shard_count (shard_count_),
    _hwm (1000)  // Default HWM
{
    slk_assert (_ctx);
    slk_assert (shard_count_ > 0 && shard_count_ <= 1024);

    // Allocate space for shards
    _shard_pubs.resize (_shard_count, nullptr);
    _shard_endpoints.resize (_shard_count);
    _shard_mutexes.reserve (_shard_count);

    // Create per-shard mutexes
    for (int i = 0; i < _shard_count; ++i) {
        _shard_mutexes.push_back (std::unique_ptr<mutex_t> (new mutex_t ()));
    }

    // Create all shard sockets
    if (!create_shards ()) {
        // Cleanup on failure
        for (auto *sock : _shard_pubs) {
            if (sock) {
                sock->close ();
            }
        }
        _shard_pubs.clear ();
    }
}

sharded_pubsub_t::~sharded_pubsub_t ()
{
    // Close all shard sockets
    for (auto *sock : _shard_pubs) {
        if (sock) {
            sock->close ();
        }
    }
}

bool sharded_pubsub_t::create_shards ()
{
    // Use atomic counter to make endpoint names unique across instances
    uint64_t instance_id = instance_counter.fetch_add (1);

    for (int i = 0; i < _shard_count; ++i) {
        // Create XPUB socket for this shard
        _shard_pubs[i] = _ctx->create_socket (SL_XPUB);
        if (!_shard_pubs[i]) {
            return false;
        }

        // Set HWM on the shard
        _shard_pubs[i]->setsockopt (SL_SNDHWM, &_hwm, sizeof (_hwm));

        // Generate unique inproc endpoint for this shard
        // Include instance ID to avoid conflicts between multiple sharded_pubsub instances
        std::ostringstream oss;
        oss << "inproc://shard-" << instance_id << "-" << i;
        _shard_endpoints[i] = oss.str ();

        // Bind the shard socket
        if (_shard_pubs[i]->bind (_shard_endpoints[i].c_str ()) != 0) {
            return false;
        }
    }

    return true;
}

int sharded_pubsub_t::hash_channel (const std::string &channel_) const
{
    // Use CRC16 hash to determine shard
    int slot = shard_hash_t::get_slot (channel_);

    // Map slot (0-16383) to shard (0-shard_count-1)
    return slot % _shard_count;
}

int sharded_pubsub_t::publish (const std::string &channel_,
                                const void *data_,
                                size_t len_)
{
    if (channel_.empty ()) {
        errno = EINVAL;
        return -1;
    }

    // Determine which shard this channel belongs to
    int shard_idx = hash_channel (channel_);
    slk_assert (shard_idx >= 0 && shard_idx < _shard_count);

    // Lock this shard (fine-grained locking)
    scoped_lock_t lock (*_shard_mutexes[shard_idx]);

    // Create multipart message: channel, data
    msg_t channel_msg;
    int rc = channel_msg.init_buffer (channel_.data (), channel_.size ());
    if (rc != 0) {
        return -1;
    }

    // Send channel frame with SNDMORE flag
    rc = _shard_pubs[shard_idx]->send (&channel_msg, SL_SNDMORE);
    channel_msg.close ();
    if (rc < 0) {
        return -1;
    }

    // Send data frame
    msg_t data_msg;
    rc = data_msg.init_buffer (data_, len_);
    if (rc != 0) {
        return -1;
    }

    rc = _shard_pubs[shard_idx]->send (&data_msg, 0);
    data_msg.close ();
    if (rc < 0) {
        return -1;
    }

    return static_cast<int> (len_);
}

int sharded_pubsub_t::subscribe (socket_base_t *sub_socket_,
                                  const std::string &channel_)
{
    if (!sub_socket_ || channel_.empty ()) {
        errno = EINVAL;
        return -1;
    }

    // Determine which shard this channel belongs to
    int shard_idx = hash_channel (channel_);
    slk_assert (shard_idx >= 0 && shard_idx < _shard_count);

    // Lock subscriber map to track connections
    {
        scoped_lock_t lock (_subscriber_map_mutex);

        // Check if this subscriber has already connected to this shard
        auto &shards = _subscriber_shards[sub_socket_];
        auto it = std::find (shards.begin (), shards.end (), shard_idx);
        if (it == shards.end ()) {
            // First time connecting to this shard - establish connection
            int rc = sub_socket_->connect (_shard_endpoints[shard_idx].c_str ());
            if (rc != 0) {
                return -1;
            }
            shards.push_back (shard_idx);
        }
    }

    // Set subscription filter on the SUB socket
    int rc = sub_socket_->setsockopt (SL_SUBSCRIBE, channel_.data (),
                                       channel_.size ());
    if (rc != 0) {
        return -1;
    }

    return 0;
}

int sharded_pubsub_t::unsubscribe (socket_base_t *sub_socket_,
                                    const std::string &channel_)
{
    if (!sub_socket_ || channel_.empty ()) {
        errno = EINVAL;
        return -1;
    }

    // Remove subscription filter from the SUB socket
    int rc = sub_socket_->setsockopt (SL_UNSUBSCRIBE, channel_.data (),
                                       channel_.size ());
    if (rc != 0) {
        return -1;
    }

    return 0;
}

int sharded_pubsub_t::set_hwm (int hwm_)
{
    if (hwm_ < 0) {
        errno = EINVAL;
        return -1;
    }

    _hwm = hwm_;

    // Update HWM on all existing shards
    for (int i = 0; i < _shard_count; ++i) {
        scoped_lock_t lock (*_shard_mutexes[i]);

        if (_shard_pubs[i]) {
            int rc = _shard_pubs[i]->setsockopt (SL_SNDHWM, &_hwm,
                                                  sizeof (_hwm));
            if (rc != 0) {
                return -1;
            }
        }
    }

    return 0;
}

int sharded_pubsub_t::get_shard_for_channel (const std::string &channel_) const
{
    return hash_channel (channel_);
}

std::string sharded_pubsub_t::get_shard_endpoint (int shard_index_) const
{
    if (shard_index_ < 0 || shard_index_ >= _shard_count) {
        return "";
    }
    return _shard_endpoints[shard_index_];
}

} // namespace slk

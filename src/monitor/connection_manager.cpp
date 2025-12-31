/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Monitoring System */

#include "../precompiled.hpp"
#include "connection_manager.hpp"
#include <algorithm>
#include <vector>

slk::connection_manager_t::connection_manager_t () : _mutex ()
{
}

slk::connection_manager_t::~connection_manager_t ()
{
    _stats.clear ();
}

void slk::connection_manager_t::peer_connected (const blob_t &routing_id,
                                                 int64_t timestamp_us)
{
    scoped_lock_t lock (_mutex);
    peer_stats_t *stats = get_or_create_stats (routing_id);

    stats->state = SLK_STATE_CONNECTED;
    stats->connection_time = timestamp_us;
    stats->last_recv_time = timestamp_us;
}

void slk::connection_manager_t::peer_disconnected (const blob_t &routing_id,
                                                    int64_t timestamp_us)
{
    scoped_lock_t lock (_mutex);
    peer_stats_t *stats = get_stats_internal (routing_id);

    if (stats) {
        stats->state = SLK_STATE_DISCONNECTED;
        stats->last_heartbeat_time = timestamp_us;
    }
}

void slk::connection_manager_t::peer_reconnecting (const blob_t &routing_id,
                                                    int64_t timestamp_us)
{
    scoped_lock_t lock (_mutex);
    peer_stats_t *stats = get_stats_internal (routing_id);

    if (stats) {
        stats->state = SLK_STATE_RECONNECTING;
        stats->reconnect_count++;
        stats->last_heartbeat_time = timestamp_us;
    }
}

void slk::connection_manager_t::record_send (const blob_t &routing_id,
                                              uint64_t bytes,
                                              int64_t timestamp_us)
{
    scoped_lock_t lock (_mutex);
    peer_stats_t *stats = get_or_create_stats (routing_id);
    stats->record_send (bytes, timestamp_us);
}

void slk::connection_manager_t::record_recv (const blob_t &routing_id,
                                              uint64_t bytes,
                                              int64_t timestamp_us)
{
    scoped_lock_t lock (_mutex);
    peer_stats_t *stats = get_or_create_stats (routing_id);
    stats->record_recv (bytes, timestamp_us);
}

void slk::connection_manager_t::record_heartbeat (const blob_t &routing_id,
                                                   int64_t timestamp_us)
{
    scoped_lock_t lock (_mutex);
    peer_stats_t *stats = get_stats_internal (routing_id);

    if (stats) {
        stats->record_heartbeat (timestamp_us);
    }
}

bool slk::connection_manager_t::is_connected (const blob_t &routing_id) const
{
    scoped_lock_t lock (_mutex);
    const peer_stats_t *stats = get_stats_internal (routing_id);
    return stats && stats->state == SLK_STATE_CONNECTED;
}

slk::peer_state_t
slk::connection_manager_t::get_state (const blob_t &routing_id) const
{
    scoped_lock_t lock (_mutex);
    const peer_stats_t *stats = get_stats_internal (routing_id);
    return stats ? static_cast<peer_state_t> (stats->state)
                 : SLK_STATE_DISCONNECTED;
}

bool slk::connection_manager_t::get_stats (const blob_t &routing_id,
                                            peer_stats_t *stats) const
{
    scoped_lock_t lock (_mutex);
    const peer_stats_t *peer_stats = get_stats_internal (routing_id);

    if (peer_stats && stats) {
        *stats = *peer_stats;
        return true;
    }

    return false;
}

void slk::connection_manager_t::get_connected_peers (
    std::vector<blob_t> *peers) const
{
    scoped_lock_t lock (_mutex);

    if (peers) {
        peers->clear ();
        peers->reserve (_stats.size ());

        for (stats_map_t::const_iterator it = _stats.begin ();
             it != _stats.end (); ++it) {
            if (it->second.state == SLK_STATE_CONNECTED) {
                // Create a copy and move it into the vector
                blob_t id_copy;
                id_copy.set_deep_copy (it->first);
                peers->push_back (SL_MOVE (id_copy));
            }
        }
    }
}

size_t slk::connection_manager_t::get_peer_count () const
{
    scoped_lock_t lock (_mutex);

    size_t count = 0;
    for (stats_map_t::const_iterator it = _stats.begin ();
         it != _stats.end (); ++it) {
        if (it->second.state == SLK_STATE_CONNECTED) {
            count++;
        }
    }

    return count;
}

void slk::connection_manager_t::mark_ping_sent (const blob_t &routing_id,
                                                 int64_t timestamp_us)
{
    scoped_lock_t lock (_mutex);
    peer_stats_t *stats = get_stats_internal (routing_id);

    if (stats) {
        stats->last_ping_sent = timestamp_us;
        stats->ping_timestamp = timestamp_us;
        stats->ping_outstanding = true;
    }
}

void slk::connection_manager_t::mark_pong_received (const blob_t &routing_id,
                                                     int64_t timestamp_us)
{
    scoped_lock_t lock (_mutex);
    peer_stats_t *stats = get_stats_internal (routing_id);

    if (stats) {
        stats->update_rtt (timestamp_us);
        stats->record_heartbeat (timestamp_us);
    }
}

int slk::connection_manager_t::get_rtt (const blob_t &routing_id) const
{
    scoped_lock_t lock (_mutex);
    const peer_stats_t *stats = get_stats_internal (routing_id);
    return stats ? stats->rtt_us : 0;
}

void slk::connection_manager_t::cleanup_stale_peers (int64_t timeout_us)
{
    scoped_lock_t lock (_mutex);

    stats_map_t::iterator it = _stats.begin ();
    while (it != _stats.end ()) {
        if (it->second.state == SLK_STATE_DISCONNECTED &&
            it->second.last_heartbeat_time > 0 &&
            (timeout_us - it->second.last_heartbeat_time) > timeout_us) {
            stats_map_t::iterator to_erase = it;
            ++it;
            _stats.erase (to_erase);
        } else {
            ++it;
        }
    }
}

void slk::connection_manager_t::remove_peer (const blob_t &routing_id)
{
    scoped_lock_t lock (_mutex);
    _stats.erase (routing_id);
}

slk::peer_stats_t *
slk::connection_manager_t::get_or_create_stats (const blob_t &routing_id)
{
    // Note: Assumes lock is already held
    stats_map_t::iterator it = _stats.find (routing_id);

    if (it == _stats.end ()) {
        // Create new entry - need to make a copy of the routing_id
        blob_t id_copy;
        id_copy.set_deep_copy (routing_id);

        std::pair<stats_map_t::iterator, bool> result =
            _stats.emplace (SL_MOVE (id_copy), peer_stats_t ());
        return &result.first->second;
    }

    return &it->second;
}

slk::peer_stats_t *
slk::connection_manager_t::get_stats_internal (const blob_t &routing_id)
{
    // Note: Assumes lock is already held
    stats_map_t::iterator it = _stats.find (routing_id);
    return (it != _stats.end ()) ? &it->second : NULL;
}

const slk::peer_stats_t *
slk::connection_manager_t::get_stats_internal (const blob_t &routing_id) const
{
    // Note: Assumes lock is already held
    stats_map_t::const_iterator it = _stats.find (routing_id);
    return (it != _stats.end ()) ? &it->second : NULL;
}

/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Monitoring System */

#ifndef SL_CONNECTION_MANAGER_HPP_INCLUDED
#define SL_CONNECTION_MANAGER_HPP_INCLUDED

#include <map>
#include <vector>
#include "peer_stats.hpp"
#include "../msg/blob.hpp"
#include "../util/mutex.hpp"
#include "../util/macros.hpp"

namespace slk
{
// Forward declaration
class event_dispatcher_t;

// Manages connection state and statistics for all peers
class connection_manager_t
{
  public:
    connection_manager_t ();
    ~connection_manager_t ();

    // Connection state management
    void peer_connected (const blob_t &routing_id, int64_t timestamp_us);
    void peer_disconnected (const blob_t &routing_id, int64_t timestamp_us);
    void peer_reconnecting (const blob_t &routing_id, int64_t timestamp_us);

    // Statistics tracking
    void record_send (const blob_t &routing_id, uint64_t bytes,
                      int64_t timestamp_us);
    void record_recv (const blob_t &routing_id, uint64_t bytes,
                      int64_t timestamp_us);
    void record_heartbeat (const blob_t &routing_id, int64_t timestamp_us);

    // State queries
    bool is_connected (const blob_t &routing_id) const;
    peer_state_t get_state (const blob_t &routing_id) const;
    bool get_stats (const blob_t &routing_id, peer_stats_t *stats) const;

    // Peer enumeration
    void get_connected_peers (std::vector<blob_t> *peers) const;
    size_t get_peer_count () const;

    // Heartbeat management
    void mark_ping_sent (const blob_t &routing_id, int64_t timestamp_us);
    void mark_pong_received (const blob_t &routing_id, int64_t timestamp_us);

    // RTT calculation
    int get_rtt (const blob_t &routing_id) const;

    // Clean up disconnected peers (optional, for memory management)
    void cleanup_stale_peers (int64_t timeout_us);

    // Remove a peer entirely
    void remove_peer (const blob_t &routing_id);

  private:
    // Thread-safe access to peer statistics
    typedef std::map<blob_t, peer_stats_t> stats_map_t;
    stats_map_t _stats;
    mutable mutex_t _mutex;

    // Helper: get or create stats entry
    peer_stats_t *get_or_create_stats (const blob_t &routing_id);
    peer_stats_t *get_stats_internal (const blob_t &routing_id);
    const peer_stats_t *get_stats_internal (const blob_t &routing_id) const;

    SL_NON_COPYABLE_NOR_MOVABLE (connection_manager_t)
};

}

#endif

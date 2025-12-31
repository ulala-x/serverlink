/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Monitoring System */

#ifndef SL_PEER_STATS_HPP_INCLUDED
#define SL_PEER_STATS_HPP_INCLUDED

#include <cstdint>

namespace slk
{
// Connection states
enum peer_state_t
{
    SLK_STATE_DISCONNECTED = 0,
    SLK_STATE_CONNECTING = 1,
    SLK_STATE_CONNECTED = 2,
    SLK_STATE_RECONNECTING = 3
};

// Internal event types for monitoring
enum event_type_t
{
    EVENT_PEER_CONNECTED = 1,
    EVENT_PEER_DISCONNECTED = 2,
    EVENT_PEER_RECONNECTING = 3,
    EVENT_PEER_RECONNECTED = 4,
    EVENT_PEER_HANDSHAKE_FAILED = 5
};

// Per-peer statistics structure
struct peer_stats_t
{
    // Timestamps (in microseconds)
    int64_t last_send_time;
    int64_t last_recv_time;
    int64_t last_heartbeat_time;
    int64_t connection_time;  // When connection was established

    // Traffic statistics
    uint64_t bytes_sent;
    uint64_t bytes_recv;
    uint64_t messages_sent;
    uint64_t messages_recv;

    // Connection state
    int state;              // peer_state_t
    int reconnect_count;
    int rtt_us;             // Round-trip time in microseconds

    // Heartbeat tracking
    int64_t last_ping_sent;
    int64_t ping_timestamp; // Timestamp in PING message (for RTT calc)
    bool ping_outstanding;  // Waiting for PONG response

    peer_stats_t ()
        : last_send_time (0),
          last_recv_time (0),
          last_heartbeat_time (0),
          connection_time (0),
          bytes_sent (0),
          bytes_recv (0),
          messages_sent (0),
          messages_recv (0),
          state (SLK_STATE_DISCONNECTED),
          reconnect_count (0),
          rtt_us (0),
          last_ping_sent (0),
          ping_timestamp (0),
          ping_outstanding (false)
    {
    }

    // Reset statistics (on reconnection)
    void reset ()
    {
        last_send_time = 0;
        last_recv_time = 0;
        last_heartbeat_time = 0;
        connection_time = 0;
        bytes_sent = 0;
        bytes_recv = 0;
        messages_sent = 0;
        messages_recv = 0;
        rtt_us = 0;
        last_ping_sent = 0;
        ping_timestamp = 0;
        ping_outstanding = false;
    }

    // Update traffic statistics
    void record_send (uint64_t bytes, int64_t timestamp_us)
    {
        bytes_sent += bytes;
        messages_sent++;
        last_send_time = timestamp_us;
    }

    void record_recv (uint64_t bytes, int64_t timestamp_us)
    {
        bytes_recv += bytes;
        messages_recv++;
        last_recv_time = timestamp_us;
    }

    // Update heartbeat
    void record_heartbeat (int64_t timestamp_us)
    {
        last_heartbeat_time = timestamp_us;
        ping_outstanding = false;
    }

    // Calculate and update RTT
    void update_rtt (int64_t current_time_us)
    {
        if (ping_outstanding && ping_timestamp > 0) {
            rtt_us = static_cast<int> (current_time_us - ping_timestamp);
            ping_outstanding = false;
        }
    }
};

}

#endif

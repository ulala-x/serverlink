/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_ROUTER_HPP_INCLUDED
#define SL_ROUTER_HPP_INCLUDED

#include <map>
#include <set>
#include <vector>

#include "socket_base.hpp"
#include "session_base.hpp"
#include "../msg/blob.hpp"
#include "../msg/msg.hpp"
#include "../pipe/fq.hpp"
#include "../monitor/peer_stats.hpp"
#include "../monitor/event_dispatcher.hpp"

namespace slk
{
class ctx_t;
class pipe_t;

// ROUTER socket implementation
// Routes messages to peers based on their identity
class router_t : public routing_socket_base_t
{
  public:
    router_t (ctx_t *parent_, uint32_t tid_, int sid_);
    ~router_t () SL_OVERRIDE;

    // Overrides of functions from socket_base_t
    void xattach_pipe (pipe_t *pipe_, bool subscribe_to_all_,
                       bool locally_initiated_) SL_FINAL;
    int xsetsockopt (int option_, const void *optval_,
                     size_t optvallen_) SL_FINAL;
    int xsend (msg_t *msg_) SL_OVERRIDE;
    int xrecv (msg_t *msg_) SL_OVERRIDE;
    bool xhas_in () SL_OVERRIDE;
    bool xhas_out () SL_OVERRIDE;
    void xread_activated (pipe_t *pipe_) SL_FINAL;
    void xpipe_terminated (pipe_t *pipe_) SL_FINAL;
    int get_peer_state (const void *routing_id_,
                        size_t routing_id_size_) const SL_FINAL;

    // Monitoring API
    bool is_peer_connected (const blob_t &routing_id) const;
    bool get_peer_statistics (const blob_t &routing_id,
                              struct peer_stats_t *stats) const;
    void get_connected_peers (std::vector<blob_t> *peers) const;
    void set_monitor_callback (monitor_callback_fn callback, void *user_data,
                               int event_mask);

    // Heartbeat API
    int send_ping (const blob_t &routing_id);
    void process_heartbeat_message (const blob_t &routing_id, msg_t *msg);

  protected:
    // Rollback any message parts that were sent but not yet flushed
    int rollback ();

  private:
    // Receive peer id and update lookup map
    bool identify_peer (pipe_t *pipe_, bool locally_initiated_);

    // Fair queueing object for inbound pipes
    fq_t _fq;

    // True iff there is a message held in the pre-fetch buffer
    bool _prefetched;

    // If true, the receiver got the message part with
    // the peer's identity
    bool _routing_id_sent;

    // Holds the prefetched identity
    msg_t _prefetched_id;

    // Holds the prefetched message
    msg_t _prefetched_msg;

    // The pipe we are currently reading from
    pipe_t *_current_in;

    // Should current_in should be terminate after all parts received?
    bool _terminate_current_in;

    // If true, more incoming message parts are expected
    bool _more_in;

    // We keep a set of pipes that have not been identified yet
    std::set<pipe_t *> _anonymous_pipes;

    // The pipe we are currently writing to
    pipe_t *_current_out;

    // If true, more outgoing message parts are expected
    bool _more_out;

    // Routing IDs are generated. It's a simple increment and wrap-over
    // algorithm. This value is the next ID to use (if not used already)
    uint32_t _next_integral_routing_id;

    // If true, report EAGAIN to the caller instead of silently dropping
    // the message targeting an unknown peer
    bool _mandatory;
    bool _raw_socket;

    // if true, send an empty message to every connected router peer
    bool _probe_router;

    // If true, the router will reassign an identity upon encountering a
    // name collision. The new pipe will take the identity, the old pipe
    // will be terminated
    bool _handover;

    // Monitoring system components
    class connection_manager_t *_conn_manager;
    class event_dispatcher_t *_event_dispatcher;

    // Helper: dispatch monitoring event
    void dispatch_event (event_type_t type, const blob_t &routing_id,
                         int64_t timestamp_us);

    // Helper: record send/recv statistics
    void record_send_stats (const blob_t &routing_id, size_t size);
    void record_recv_stats (const blob_t &routing_id, size_t size);

    SL_NON_COPYABLE_NOR_MOVABLE (router_t)
};
}

#endif

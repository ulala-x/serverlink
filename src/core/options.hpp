/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_OPTIONS_HPP_INCLUDED
#define SL_OPTIONS_HPP_INCLUDED

#include <string>
#include <vector>
#include <map>

#include "../util/atomic_ptr.hpp"
#include "../util/config.hpp"
#include "../util/constants.hpp"
#include "../transport/tcp_address.hpp"

namespace slk
{
// Default high-water mark (1000 messages)
constexpr int default_hwm = 1000;

// Socket configuration options
// Only ROUTER-relevant options are included
struct options_t
{
    options_t ();

    int setsockopt (int option_, const void *optval_, size_t optvallen_);
    int getsockopt (int option_, void *optval_, size_t *optvallen_) const;

    // High-water marks for message pipes
    int sndhwm;
    int rcvhwm;

    // I/O thread affinity
    uint64_t affinity;

    // Socket routing ID (identity)
    unsigned char routing_id_size;
    unsigned char routing_id[256];

    // SO_SNDBUF and SO_RCVBUF for underlying transport sockets
    int sndbuf;
    int rcvbuf;

    // Type of service (DSCP and ECN socket options)
    int tos;

    // Protocol-defined priority
    int priority;

    // Socket type
    int8_t type;

    // Linger time in milliseconds
    atomic_value_t linger;

    // Maximum interval in milliseconds beyond which userspace will timeout connect()
    int connect_timeout;

    // Maximum interval in milliseconds beyond which TCP will timeout retransmitted packets
    int tcp_maxrt;

    // Disable reconnect under certain conditions
    int reconnect_stop;

    // Minimum interval between attempts to reconnect, in milliseconds
    int reconnect_ivl;

    // Maximum interval between attempts to reconnect, in milliseconds
    int reconnect_ivl_max;

    // Maximum backlog for pending connections
    int backlog;

    // Maximal size of message to handle
    int64_t maxmsgsize;

    // The timeout for send/recv operations for this socket, in milliseconds
    int rcvtimeo;
    int sndtimeo;

    //  If true, send an empty message to the peer when a new pipe is attached.
    bool probe_router;

    // If true, IPv6 is enabled (as well as IPv4)
    bool ipv6;

    // If 1, connecting pipes are not attached immediately, meaning a send()
    // on a socket with only connecting pipes would block
    int immediate;

    // If true, the routing id message is forwarded to the socket
    bool recv_routing_id;

    // If true, router socket accepts non-zmq tcp connections
    bool raw_socket;
    bool raw_notify;  // Provide connect notifications

    // TCP keep-alive settings
    // Defaults to -1 = do not change socket options
    int tcp_keepalive;
    int tcp_keepalive_cnt;
    int tcp_keepalive_idle;
    int tcp_keepalive_intvl;

    // Socket ID
    int socket_id;

    // If connection handshake is not done after this many milliseconds,
    // close socket. Default is 30 secs. 0 means no handshake timeout.
    int handshake_ivl;

    bool connected;

    // Heartbeat settings
    // If remote peer receives a PING message and doesn't receive another
    // message within the ttl value, it should close the connection
    // (measured in tenths of a second)
    uint16_t heartbeat_ttl;
    // Time in milliseconds between sending heartbeat PING messages
    int heartbeat_interval;
    // Time in milliseconds to wait for a PING response before disconnecting
    int heartbeat_timeout;

    // When creating a new socket, if this option is set the value
    // will be used as the File Descriptor instead of allocating a new
    // one via the socket() system call
    int use_fd;

    // Device to bind the underlying socket to, eg. VRF or interface
    std::string bound_device;

    // Use of loopback fastpath
    bool loopback_fastpath;

    // Maximal batching size for engines with receiving functionality
    int in_batch_size;
    // Maximal batching size for engines with sending functionality
    int out_batch_size;

    // Use zero copy strategy for storing message content when decoding
    bool zero_copy;

    // Router socket connect/disconnect notifications
    int router_notify;

    // Application metadata
    std::map<std::string, std::string> app_metadata;

    // Version of monitor events to emit
    int monitor_event_version;

    // Hello msg
    std::vector<unsigned char> hello_msg;
    bool can_send_hello_msg;

    // Disconnect msg
    std::vector<unsigned char> disconnect_msg;
    bool can_recv_disconnect_msg;

    // Hiccup msg
    std::vector<unsigned char> hiccup_msg;
    bool can_recv_hiccup_msg;

    // This option removes several delays caused by scheduling, interrupts and context switching
    int busy_poll;

    // TCP accept() filters
    typedef std::vector<tcp_address_mask_t> tcp_accept_filters_t;
    tcp_accept_filters_t tcp_accept_filters;

    // Pub/Sub options
    bool filter;           // If true, filter messages based on subscriptions (XSUB)
    bool invert_matching;  // If true, invert subscription matching logic
};

// Helper functions for getting/setting socket options
int do_getsockopt (void *optval_,
                   size_t *optvallen_,
                   const void *value_,
                   size_t value_len_);

template <typename T>
int do_getsockopt (void *const optval_, size_t *const optvallen_, T value_)
{
    return do_getsockopt (optval_, optvallen_, &value_, sizeof (T));
}

int do_getsockopt (void *optval_,
                   size_t *optvallen_,
                   const std::string &value_);

int do_setsockopt_int_as_bool_strict (const void *optval_,
                                      size_t optvallen_,
                                      bool *out_value_);

int do_setsockopt_int_as_bool_relaxed (const void *optval_,
                                       size_t optvallen_,
                                       bool *out_value_);
}

#endif

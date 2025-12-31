/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_CONSTANTS_HPP_INCLUDED
#define SL_CONSTANTS_HPP_INCLUDED

namespace slk
{
// Socket option constants (port from ZMQ_* constants)
constexpr int SL_SNDHWM = 23;
constexpr int SL_RCVHWM = 24;
constexpr int SL_AFFINITY = 4;
constexpr int SL_ROUTING_ID = 5;
constexpr int SL_SNDBUF = 11;
constexpr int SL_RCVBUF = 12;
constexpr int SL_TOS = 57;
constexpr int SL_TYPE = 16;
constexpr int SL_LINGER = 17;
constexpr int SL_CONNECT_TIMEOUT = 79;
constexpr int SL_TCP_MAXRT = 80;
constexpr int SL_RECONNECT_STOP = 81;
constexpr int SL_RECONNECT_IVL = 18;
constexpr int SL_RECONNECT_IVL_MAX = 21;
constexpr int SL_BACKLOG = 19;
constexpr int SL_MAXMSGSIZE = 22;
constexpr int SL_RCVTIMEO = 27;
constexpr int SL_SNDTIMEO = 28;
constexpr int SL_IPV6 = 42;
constexpr int SL_IMMEDIATE = 39;
constexpr int SL_TCP_KEEPALIVE = 34;
constexpr int SL_TCP_KEEPALIVE_CNT = 35;
constexpr int SL_TCP_KEEPALIVE_IDLE = 36;
constexpr int SL_TCP_KEEPALIVE_INTVL = 37;
constexpr int SL_HANDSHAKE_IVL = 66;
constexpr int SL_HEARTBEAT_IVL = 75;
constexpr int SL_HEARTBEAT_TTL = 76;
constexpr int SL_HEARTBEAT_TIMEOUT = 77;
constexpr int SL_USE_FD = 89;
constexpr int SL_BINDTODEVICE = 92;
constexpr int SL_LOOPBACK_FASTPATH = 94;
constexpr int SL_METADATA = 95;
constexpr int SL_IN_BATCH_SIZE = 101;
constexpr int SL_OUT_BATCH_SIZE = 102;
constexpr int SL_BUSY_POLL = 103;
constexpr int SL_HELLO_MSG = 110;
constexpr int SL_DISCONNECT_MSG = 111;
constexpr int SL_PRIORITY = 112;
constexpr int SL_HICCUP_MSG = 114;

// Router-specific options
constexpr int SL_ROUTER_MANDATORY = 33;
constexpr int SL_ROUTER_RAW = 41;
constexpr int SL_PROBE_ROUTER = 51;
constexpr int SL_ROUTER_HANDOVER = 56;
constexpr int SL_CONNECT_ROUTING_ID = 61;  // Set peer's routing ID when connecting
constexpr int SL_ROUTER_NOTIFY = 97;

// Socket type
constexpr int SL_ROUTER = 6;

// Socket option flags
constexpr int SL_DONTWAIT = 1;
constexpr int SL_SNDMORE = 2;

// Socket state flags
constexpr int SL_POLLIN = 1;
constexpr int SL_POLLOUT = 2;
constexpr int SL_POLLERR = 4;

// Socket getsockopt options
constexpr int SL_RCVMORE = 13;
constexpr int SL_FD = 14;
constexpr int SL_EVENTS = 15;

// Context options
constexpr int SL_IO_THREADS = 1;
constexpr int SL_MAX_SOCKETS = 2;
constexpr int SL_SOCKET_LIMIT = 3;
constexpr int SL_THREAD_SCHED_POLICY = 6;
constexpr int SL_THREAD_AFFINITY_CPU_ADD = 7;
constexpr int SL_THREAD_AFFINITY_CPU_REMOVE = 8;
constexpr int SL_THREAD_PRIORITY = 9;
constexpr int SL_THREAD_NAME_PREFIX = 10;
constexpr int SL_MAX_MSGSZ = 13;
constexpr int SL_MSG_T_SIZE = 14;
constexpr int SL_ZERO_COPY_RECV = 15;
constexpr int SL_BLOCKY = 70;

// Default values
constexpr int SL_IO_THREADS_DFLT = 1;
constexpr int SL_MAX_SOCKETS_DFLT = 1023;
// SL_THREAD_PRIORITY_DFLT and SL_THREAD_SCHED_POLICY_DFLT are defined in thread.hpp

// Router notify flags
constexpr int SL_NOTIFY_CONNECT = 1;
constexpr int SL_NOTIFY_DISCONNECT = 2;

// Reconnect stop flags
constexpr int SL_RECONNECT_STOP_CONN_REFUSED = 0x1;
constexpr int SL_RECONNECT_STOP_HANDSHAKE_FAILED = 0x2;
constexpr int SL_RECONNECT_STOP_AFTER_DISCONNECT = 0x4;
}

#endif

/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "options.hpp"
#include "../util/err.hpp"
#include "../util/macros.hpp"
#include "../util/config.hpp"
#include "../util/constants.hpp"
#include <string.h>
#include <limits.h>

#ifndef SL_HAVE_WINDOWS
#include <net/if.h>
#endif

#if defined IFNAMSIZ
#define BINDDEVSIZ IFNAMSIZ
#else
#define BINDDEVSIZ 16
#endif

static int sockopt_invalid ()
{
    errno = EINVAL;
    return -1;
}

int slk::do_getsockopt (void *const optval_,
                        size_t *const optvallen_,
                        const std::string &value_)
{
    return do_getsockopt (optval_, optvallen_, value_.c_str (),
                          value_.size () + 1);
}

int slk::do_getsockopt (void *const optval_,
                        size_t *const optvallen_,
                        const void *value_,
                        const size_t value_len_)
{
    if (*optvallen_ < value_len_) {
        return sockopt_invalid ();
    }
    memcpy (optval_, value_, value_len_);
    memset (static_cast<char *> (optval_) + value_len_, 0,
            *optvallen_ - value_len_);
    *optvallen_ = value_len_;
    return 0;
}

template <typename T>
static int do_setsockopt (const void *const optval_,
                          const size_t optvallen_,
                          T *const out_value_)
{
    if (optvallen_ == sizeof (T)) {
        memcpy (out_value_, optval_, sizeof (T));
        return 0;
    }
    return sockopt_invalid ();
}

int slk::do_setsockopt_int_as_bool_strict (const void *const optval_,
                                           const size_t optvallen_,
                                           bool *const out_value_)
{
    int value = -1;
    if (do_setsockopt (optval_, optvallen_, &value) == -1)
        return -1;
    if (value == 0 || value == 1) {
        *out_value_ = (value != 0);
        return 0;
    }
    return sockopt_invalid ();
}

int slk::do_setsockopt_int_as_bool_relaxed (const void *const optval_,
                                            const size_t optvallen_,
                                            bool *const out_value_)
{
    int value = -1;
    if (do_setsockopt (optval_, optvallen_, &value) == -1)
        return -1;
    *out_value_ = (value != 0);
    return 0;
}

static int
do_setsockopt_string_allow_empty_strict (const void *const optval_,
                                         const size_t optvallen_,
                                         std::string *const out_value_,
                                         const size_t max_len_)
{
    if (optval_ == NULL && optvallen_ == 0) {
        out_value_->clear ();
        return 0;
    }
    if (optval_ != NULL && optvallen_ > 0 && optvallen_ <= max_len_) {
        out_value_->assign (static_cast<const char *> (optval_), optvallen_);
        return 0;
    }
    return sockopt_invalid ();
}

slk::options_t::options_t () :
    sndhwm (default_hwm),
    rcvhwm (default_hwm),
    affinity (0),
    routing_id_size (0),
    sndbuf (-1),
    rcvbuf (-1),
    tos (0),
    priority (0),
    type (-1),
    linger (-1),
    connect_timeout (0),
    tcp_maxrt (0),
    reconnect_stop (0),
    reconnect_ivl (100),
    reconnect_ivl_max (0),
    backlog (100),
    maxmsgsize (-1),
    rcvtimeo (-1),
    sndtimeo (-1),
    ipv6 (false),
    immediate (0),
    recv_routing_id (false),
    raw_socket (false),
    raw_notify (true),
    tcp_keepalive (-1),
    tcp_keepalive_cnt (-1),
    tcp_keepalive_idle (-1),
    tcp_keepalive_intvl (-1),
    socket_id (0),
    handshake_ivl (30000),
    connected (false),
    heartbeat_ttl (0),
    heartbeat_interval (0),
    heartbeat_timeout (-1),
    use_fd (-1),
    loopback_fastpath (false),
    in_batch_size (8192),
    out_batch_size (8192),
    zero_copy (true),
    router_notify (0),
    monitor_event_version (1),
    hello_msg (),
    can_send_hello_msg (false),
    disconnect_msg (),
    can_recv_disconnect_msg (false),
    hiccup_msg (),
    can_recv_hiccup_msg (false),
    busy_poll (0),
    filter (false),
    invert_matching (false)
{
}

const int deciseconds_per_millisecond = 100;

int slk::options_t::setsockopt (int option_,
                                const void *optval_,
                                size_t optvallen_)
{
    const bool is_int = (optvallen_ == sizeof (int));
    int value = 0;
    if (is_int)
        memcpy (&value, optval_, sizeof (int));

    switch (option_) {
        case SL_SNDHWM:
            if (is_int && value >= 0) {
                sndhwm = value;
                return 0;
            }
            break;

        case SL_RCVHWM:
            if (is_int && value >= 0) {
                rcvhwm = value;
                return 0;
            }
            break;

        case SL_AFFINITY:
            return do_setsockopt (optval_, optvallen_, &affinity);

        case SL_ROUTING_ID:
            // Routing id is any binary string from 1 to 255 octets
            if (optvallen_ > 0 && optvallen_ <= UCHAR_MAX) {
                routing_id_size = static_cast<unsigned char> (optvallen_);
                memcpy (routing_id, optval_, routing_id_size);
                return 0;
            }
            break;

        case SL_SNDBUF:
            if (is_int && value >= -1) {
                sndbuf = value;
                return 0;
            }
            break;

        case SL_RCVBUF:
            if (is_int && value >= -1) {
                rcvbuf = value;
                return 0;
            }
            break;

        case SL_TOS:
            if (is_int && value >= 0) {
                tos = value;
                return 0;
            }
            break;

        case SL_LINGER:
            if (is_int && value >= -1) {
                linger.store (value);
                return 0;
            }
            break;

        case SL_CONNECT_TIMEOUT:
            if (is_int && value >= 0) {
                connect_timeout = value;
                return 0;
            }
            break;

        case SL_TCP_MAXRT:
            if (is_int && value >= 0) {
                tcp_maxrt = value;
                return 0;
            }
            break;

        case SL_RECONNECT_STOP:
            if (is_int) {
                reconnect_stop = value;
                return 0;
            }
            break;

        case SL_RECONNECT_IVL:
            if (is_int && value >= -1) {
                reconnect_ivl = value;
                return 0;
            }
            break;

        case SL_RECONNECT_IVL_MAX:
            if (is_int && value >= 0) {
                reconnect_ivl_max = value;
                return 0;
            }
            break;

        case SL_BACKLOG:
            if (is_int && value >= 0) {
                backlog = value;
                return 0;
            }
            break;

        case SL_MAXMSGSIZE:
            return do_setsockopt (optval_, optvallen_, &maxmsgsize);

        case SL_RCVTIMEO:
            if (is_int && value >= -1) {
                rcvtimeo = value;
                return 0;
            }
            break;

        case SL_SNDTIMEO:
            if (is_int && value >= -1) {
                sndtimeo = value;
                return 0;
            }
            break;

        case SL_IPV6:
            return do_setsockopt_int_as_bool_strict (optval_, optvallen_, &ipv6);

        case SL_TCP_KEEPALIVE:
            if (is_int && (value == -1 || value == 0 || value == 1)) {
                tcp_keepalive = value;
                return 0;
            }
            break;

        case SL_TCP_KEEPALIVE_CNT:
            if (is_int && (value == -1 || value >= 0)) {
                tcp_keepalive_cnt = value;
                return 0;
            }
            break;

        case SL_TCP_KEEPALIVE_IDLE:
            if (is_int && (value == -1 || value >= 0)) {
                tcp_keepalive_idle = value;
                return 0;
            }
            break;

        case SL_TCP_KEEPALIVE_INTVL:
            if (is_int && (value == -1 || value >= 0)) {
                tcp_keepalive_intvl = value;
                return 0;
            }
            break;

        case SL_IMMEDIATE:
            if (is_int && (value == 0 || value == 1)) {
                immediate = value;
                return 0;
            }
            break;

        case SL_HANDSHAKE_IVL:
            if (is_int && value >= 0) {
                handshake_ivl = value;
                return 0;
            }
            break;

        case SL_HEARTBEAT_IVL:
            if (is_int && value >= 0) {
                heartbeat_interval = value;
                return 0;
            }
            break;

        case SL_HEARTBEAT_TTL:
            // Convert this to deciseconds from milliseconds
            value = value / deciseconds_per_millisecond;
            if (is_int && value >= 0 && value <= UINT16_MAX) {
                heartbeat_ttl = static_cast<uint16_t> (value);
                return 0;
            }
            break;

        case SL_HEARTBEAT_TIMEOUT:
            if (is_int && value >= 0) {
                heartbeat_timeout = value;
                return 0;
            }
            break;

        case SL_USE_FD:
            if (is_int && value >= -1) {
                use_fd = value;
                return 0;
            }
            break;

        case SL_BINDTODEVICE:
            return do_setsockopt_string_allow_empty_strict (
              optval_, optvallen_, &bound_device, BINDDEVSIZ);

        case SL_LOOPBACK_FASTPATH:
            return do_setsockopt_int_as_bool_relaxed (optval_, optvallen_,
                                                      &loopback_fastpath);

        case SL_METADATA:
            if (optvallen_ > 0 && !is_int) {
                const std::string s (static_cast<const char *> (optval_),
                                     optvallen_);
                const size_t pos = s.find (':');
                if (pos != std::string::npos && pos != 0
                    && pos != s.length () - 1) {
                    const std::string key = s.substr (0, pos);
                    if (key.compare (0, 2, "X-") == 0
                        && key.length () <= UCHAR_MAX) {
                        std::string val = s.substr (pos + 1, s.length ());
                        app_metadata.insert (
                          std::pair<std::string, std::string> (key, val));
                        return 0;
                    }
                }
            }
            errno = EINVAL;
            return -1;

        case SL_IN_BATCH_SIZE:
            if (is_int && value > 0) {
                in_batch_size = value;
                return 0;
            }
            break;

        case SL_OUT_BATCH_SIZE:
            if (is_int && value > 0) {
                out_batch_size = value;
                return 0;
            }
            break;

        case SL_BUSY_POLL:
            if (is_int) {
                busy_poll = value;
                return 0;
            }
            break;

        case SL_HELLO_MSG:
            if (optvallen_ > 0) {
                unsigned char *bytes = (unsigned char *) optval_;
                hello_msg =
                  std::vector<unsigned char> (bytes, bytes + optvallen_);
            } else {
                hello_msg = std::vector<unsigned char> ();
            }
            return 0;

        case SL_DISCONNECT_MSG:
            if (optvallen_ > 0) {
                unsigned char *bytes = (unsigned char *) optval_;
                disconnect_msg =
                  std::vector<unsigned char> (bytes, bytes + optvallen_);
            } else {
                disconnect_msg = std::vector<unsigned char> ();
            }
            return 0;

        case SL_PRIORITY:
            if (is_int && value >= 0) {
                priority = value;
                return 0;
            }
            break;

        case SL_HICCUP_MSG:
            if (optvallen_ > 0) {
                unsigned char *bytes = (unsigned char *) optval_;
                hiccup_msg =
                  std::vector<unsigned char> (bytes, bytes + optvallen_);
            } else {
                hiccup_msg = std::vector<unsigned char> ();
            }
            return 0;

        case SL_ROUTER_NOTIFY:
            if (is_int) {
                router_notify = value;
                return 0;
            }
            break;

        default:
            break;
    }

    errno = EINVAL;
    return -1;
}

int slk::options_t::getsockopt (int option_,
                                void *optval_,
                                size_t *optvallen_) const
{
    const bool is_int = (*optvallen_ == sizeof (int));
    int *value = static_cast<int *> (optval_);

    switch (option_) {
        case SL_SNDHWM:
            if (is_int) {
                *value = sndhwm;
                return 0;
            }
            break;

        case SL_RCVHWM:
            if (is_int) {
                *value = rcvhwm;
                return 0;
            }
            break;

        case SL_AFFINITY:
            if (*optvallen_ == sizeof (uint64_t)) {
                *(static_cast<uint64_t *> (optval_)) = affinity;
                return 0;
            }
            break;

        case SL_ROUTING_ID:
            return do_getsockopt (optval_, optvallen_, routing_id,
                                  routing_id_size);

        case SL_SNDBUF:
            if (is_int) {
                *value = sndbuf;
                return 0;
            }
            break;

        case SL_RCVBUF:
            if (is_int) {
                *value = rcvbuf;
                return 0;
            }
            break;

        case SL_TOS:
            if (is_int) {
                *value = tos;
                return 0;
            }
            break;

        case SL_TYPE:
            if (is_int) {
                *value = type;
                return 0;
            }
            break;

        case SL_LINGER:
            if (is_int) {
                *value = linger.load ();
                return 0;
            }
            break;

        case SL_CONNECT_TIMEOUT:
            if (is_int) {
                *value = connect_timeout;
                return 0;
            }
            break;

        case SL_TCP_MAXRT:
            if (is_int) {
                *value = tcp_maxrt;
                return 0;
            }
            break;

        case SL_RECONNECT_STOP:
            if (is_int) {
                *value = reconnect_stop;
                return 0;
            }
            break;

        case SL_RECONNECT_IVL:
            if (is_int) {
                *value = reconnect_ivl;
                return 0;
            }
            break;

        case SL_RECONNECT_IVL_MAX:
            if (is_int) {
                *value = reconnect_ivl_max;
                return 0;
            }
            break;

        case SL_BACKLOG:
            if (is_int) {
                *value = backlog;
                return 0;
            }
            break;

        case SL_MAXMSGSIZE:
            if (*optvallen_ == sizeof (int64_t)) {
                *(static_cast<int64_t *> (optval_)) = maxmsgsize;
                *optvallen_ = sizeof (int64_t);
                return 0;
            }
            break;

        case SL_RCVTIMEO:
            if (is_int) {
                *value = rcvtimeo;
                return 0;
            }
            break;

        case SL_SNDTIMEO:
            if (is_int) {
                *value = sndtimeo;
                return 0;
            }
            break;

        case SL_IPV6:
            if (is_int) {
                *value = ipv6;
                return 0;
            }
            break;

        case SL_IMMEDIATE:
            if (is_int) {
                *value = immediate;
                return 0;
            }
            break;

        case SL_TCP_KEEPALIVE:
            if (is_int) {
                *value = tcp_keepalive;
                return 0;
            }
            break;

        case SL_TCP_KEEPALIVE_CNT:
            if (is_int) {
                *value = tcp_keepalive_cnt;
                return 0;
            }
            break;

        case SL_TCP_KEEPALIVE_IDLE:
            if (is_int) {
                *value = tcp_keepalive_idle;
                return 0;
            }
            break;

        case SL_TCP_KEEPALIVE_INTVL:
            if (is_int) {
                *value = tcp_keepalive_intvl;
                return 0;
            }
            break;

        case SL_HANDSHAKE_IVL:
            if (is_int) {
                *value = handshake_ivl;
                return 0;
            }
            break;

        case SL_HEARTBEAT_IVL:
            if (is_int) {
                *value = heartbeat_interval;
                return 0;
            }
            break;

        case SL_HEARTBEAT_TTL:
            if (is_int) {
                // Convert the internal deciseconds value to milliseconds
                *value = heartbeat_ttl * 100;
                return 0;
            }
            break;

        case SL_HEARTBEAT_TIMEOUT:
            if (is_int) {
                *value = heartbeat_timeout;
                return 0;
            }
            break;

        case SL_USE_FD:
            if (is_int) {
                *value = use_fd;
                return 0;
            }
            break;

        case SL_BINDTODEVICE:
            return do_getsockopt (optval_, optvallen_, bound_device);

        case SL_LOOPBACK_FASTPATH:
            if (is_int) {
                *value = loopback_fastpath;
                return 0;
            }
            break;

        case SL_ROUTER_NOTIFY:
            if (is_int) {
                *value = router_notify;
                return 0;
            }
            break;

        case SL_IN_BATCH_SIZE:
            if (is_int) {
                *value = in_batch_size;
                return 0;
            }
            break;

        case SL_OUT_BATCH_SIZE:
            if (is_int) {
                *value = out_batch_size;
                return 0;
            }
            break;

        case SL_PRIORITY:
            if (is_int) {
                *value = priority;
                return 0;
            }
            break;

        case SL_BUSY_POLL:
            if (is_int) {
                *value = busy_poll;
                return 0;
            }
            break;

        default:
            break;
    }

    errno = EINVAL;
    return -1;
}

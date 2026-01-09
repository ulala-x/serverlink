/* ServerLink - High-performance message routing library */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SERVERLINK_H
#define SERVERLINK_H

#include "serverlink_export.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*  Version Information                                                     */
/****************************************************************************/

#define SLK_VERSION_MAJOR 0
#define SLK_VERSION_MINOR 1
#define SLK_VERSION_PATCH 0

SL_EXPORT void SL_CALL slk_version(int *major, int *minor, int *patch);

/****************************************************************************/
/*  Socket Types                                                            */
/****************************************************************************/

/* Supported Patterns */
#define SLK_PAIR   0  /* Exclusive 1:1 connection */
#define SLK_PUB    1  /* Publisher */
#define SLK_SUB    2  /* Subscriber */
#define SLK_ROUTER 6  /* Routed server */

/* Internal / Legacy Patterns (Do not use in new code) */
#define SLK_DEALER 5
#define SLK_XPUB        9
#define SLK_XSUB        10

/******************************************************************************/
/*  Socket Options                                                          */
/****************************************************************************/

#define SLK_ROUTING_ID          5
#define SLK_CONNECT_ROUTING_ID  61
#define SLK_ROUTER_MANDATORY    33
#define SLK_ROUTER_HANDOVER     56
#define SLK_ROUTER_NOTIFY       97
#define SLK_LAST_ENDPOINT       32
#define SLK_HEARTBEAT_IVL       75
#define SLK_HEARTBEAT_TIMEOUT   77
#define SLK_HEARTBEAT_TTL       76
#define SLK_TCP_KEEPALIVE       34
#define SLK_TCP_KEEPALIVE_IDLE  36
#define SLK_TCP_KEEPALIVE_INTVL 37
#define SLK_TCP_KEEPALIVE_CNT   35
#define SLK_LINGER              17
#define SLK_RECONNECT_IVL       18
#define SLK_RECONNECT_IVL_MAX   21
#define SLK_BACKLOG             19
#define SLK_SNDBUF              11
#define SLK_RCVBUF              12
#define SLK_SNDHWM              23
#define SLK_RCVHWM              24
#define SLK_RCVTIMEO            27
#define SLK_SNDTIMEO            28
#define SLK_SUBSCRIBE           6
#define SLK_UNSUBSCRIBE         7
#define SLK_PSUBSCRIBE          81
#define SLK_PUNSUBSCRIBE        82
#define SLK_XPUB_VERBOSE        40
#define SLK_XPUB_VERBOSER       78
#define SLK_XPUB_NODROP         69
#define SLK_XPUB_MANUAL         71
#define SLK_XPUB_MANUAL_LAST_VALUE  70
#define SLK_XPUB_WELCOME_MSG    72
#define SLK_ONLY_FIRST_SUBSCRIBE    108
#define SLK_TOPICS_COUNT        80
#define SLK_INVERT_MATCHING     60
#define SLK_XSUB_VERBOSE_UNSUBSCRIBE 73

/****************************************************************************/
/*  Message Flags                                                           */
/****************************************************************************/

#define SLK_DONTWAIT    1
#define SLK_SNDMORE     2

/****************************************************************************/
/*  Event Types                                                             */
/****************************************************************************/

#define SLK_EVENT_CONNECTED       1
#define SLK_EVENT_DISCONNECTED    2
#define SLK_EVENT_ACCEPTED        3
#define SLK_EVENT_BIND_FAILED     4
#define SLK_EVENT_LISTENING       5
#define SLK_EVENT_CLOSED          6
#define SLK_EVENT_HANDSHAKE_START 7
#define SLK_EVENT_HANDSHAKE_OK    8
#define SLK_EVENT_HANDSHAKE_FAIL  9
#define SLK_EVENT_HEARTBEAT_OK    10
#define SLK_EVENT_HEARTBEAT_FAIL  11
#define SLK_EVENT_ALL             0xFFFF

/****************************************************************************/
/*  Context Options                                                         */
/****************************************************************************/

#define SLK_IO_THREADS                  1
#define SLK_MAX_SOCKETS                 2
#define SLK_SOCKET_LIMIT                3
#define SLK_THREAD_SCHED_POLICY         6
#define SLK_THREAD_AFFINITY_CPU_ADD     7
#define SLK_THREAD_AFFINITY_CPU_REMOVE  8
#define SLK_THREAD_PRIORITY             9
#define SLK_THREAD_NAME_PREFIX          10
#define SLK_MAX_MSGSZ                   13
#define SLK_MSG_T_SIZE                  14

/****************************************************************************/
/*  Error Codes                                                             */
/****************************************************************************/

#define SLK_EINVAL      1
#define SLK_ENOMEM      2
#define SLK_EAGAIN      3
#define SLK_ENOTSOCK    4
#define SLK_EPROTO      5
#define SLK_ETERM       6
#define SLK_EMTHREAD    7
#define SLK_EHOSTUNREACH    10
#define SLK_ENOTREADY       11
#define SLK_EPEERUNREACH    12
#define SLK_EAUTH           13

SL_EXPORT int SL_CALL slk_errno(void);
SL_EXPORT const char* SL_CALL slk_strerror(int errnum);

/****************************************************************************/
/*  Core API                                                                */
/****************************************************************************/

typedef struct slk_ctx_t slk_ctx_t;
typedef struct slk_socket_t slk_socket_t;
typedef struct slk_msg_t slk_msg_t;

SL_EXPORT slk_ctx_t* SL_CALL slk_ctx_new(void);
SL_EXPORT void SL_CALL slk_ctx_destroy(slk_ctx_t *ctx);
SL_EXPORT int SL_CALL slk_ctx_set(slk_ctx_t *ctx, int option, const void *value, size_t len);
SL_EXPORT int SL_CALL slk_ctx_get(slk_ctx_t *ctx, int option, void *value, size_t *len);

SL_EXPORT slk_socket_t* SL_CALL slk_socket(slk_ctx_t *ctx, int type);
SL_EXPORT int SL_CALL slk_close(slk_socket_t *socket);
SL_EXPORT int SL_CALL slk_bind(slk_socket_t *socket, const char *endpoint);
SL_EXPORT int SL_CALL slk_connect(slk_socket_t *socket, const char *endpoint);
SL_EXPORT int SL_CALL slk_disconnect(slk_socket_t *socket, const char *endpoint);
SL_EXPORT int SL_CALL slk_unbind(slk_socket_t *socket, const char *endpoint);
SL_EXPORT int SL_CALL slk_setsockopt(slk_socket_t *socket, int option, const void *value, size_t len);
SL_EXPORT int SL_CALL slk_getsockopt(slk_socket_t *socket, int option, void *value, size_t *len);

/****************************************************************************/
/*  Message API                                                             */
/****************************************************************************/

SL_EXPORT slk_msg_t* SL_CALL slk_msg_new(void);
SL_EXPORT slk_msg_t* SL_CALL slk_msg_new_data(const void *data, size_t size);
SL_EXPORT void SL_CALL slk_msg_destroy(slk_msg_t *msg);
SL_EXPORT int SL_CALL slk_msg_init(slk_msg_t *msg);
SL_EXPORT int SL_CALL slk_msg_init_data(slk_msg_t *msg, const void *data, size_t size);
SL_EXPORT int SL_CALL slk_msg_close(slk_msg_t *msg);
SL_EXPORT void* SL_CALL slk_msg_data(slk_msg_t *msg);
SL_EXPORT size_t SL_CALL slk_msg_size(slk_msg_t *msg);
SL_EXPORT int SL_CALL slk_msg_copy(slk_msg_t *dest, slk_msg_t *src);
SL_EXPORT int SL_CALL slk_msg_move(slk_msg_t *dest, slk_msg_t *src);
SL_EXPORT int SL_CALL slk_msg_get(slk_msg_t *msg, int property, void *value, size_t *len);
SL_EXPORT int SL_CALL slk_msg_set(slk_msg_t *msg, int property, const void *value, size_t len);
SL_EXPORT int SL_CALL slk_msg_get_routing_id(slk_msg_t *msg, void *id, size_t *size);
SL_EXPORT int SL_CALL slk_msg_set_routing_id(slk_msg_t *msg, const void *id, size_t size);

/****************************************************************************/
/*  Send/Receive API                                                        */
/****************************************************************************/

SL_EXPORT int SL_CALL slk_send(slk_socket_t *socket, const void *data, size_t len, int flags);
SL_EXPORT int SL_CALL slk_recv(slk_socket_t *socket, void *buf, size_t len, int flags);
SL_EXPORT int SL_CALL slk_msg_send(slk_msg_t *msg, slk_socket_t *socket, int flags);
SL_EXPORT int SL_CALL slk_msg_recv(slk_msg_t *msg, slk_socket_t *socket, int flags);
SL_EXPORT int SL_CALL slk_send_to(slk_socket_t *socket, const void *routing_id, size_t id_len,
                                   const void *data, size_t data_len, int flags);

/****************************************************************************/
/*  Polling API                                                             */
/****************************************************************************/

#define SLK_POLLIN  1
#define SLK_POLLOUT 2
#define SLK_POLLERR 4

typedef struct slk_pollitem_t {
    slk_socket_t *socket;
    int fd;
    short events;
    short revents;
} slk_pollitem_t;

SL_EXPORT int SL_CALL slk_poll(slk_pollitem_t *items, int nitems, long timeout);

/****************************************************************************/
/*  Modern Poller API                                                       */
/****************************************************************************/

#if defined(_WIN32)
    typedef uintptr_t slk_fd_t;
#else
    typedef int slk_fd_t;
#endif

typedef struct slk_poller_event_t {
    void *socket;
    slk_fd_t fd;
    void *user_data;
    short events;
} slk_poller_event_t;

SL_EXPORT void* SL_CALL slk_poller_new(void);
SL_EXPORT int SL_CALL slk_poller_destroy(void **poller_p);
SL_EXPORT int SL_CALL slk_poller_size(void *poller);
SL_EXPORT int SL_CALL slk_poller_add(void *poller, void *socket, void *user_data, short events);
SL_EXPORT int SL_CALL slk_poller_modify(void *poller, void *socket, short events);
SL_EXPORT int SL_CALL slk_poller_remove(void *poller, void *socket);
SL_EXPORT int SL_CALL slk_poller_wait(void *poller, slk_poller_event_t *event, long timeout);
SL_EXPORT int SL_CALL slk_poller_wait_all(void *poller, slk_poller_event_t *events, int n_events, long timeout);
SL_EXPORT int SL_CALL slk_poller_fd(void *poller, slk_fd_t *fd);
SL_EXPORT int SL_CALL slk_poller_add_fd(void *poller, slk_fd_t fd, void *user_data, short events);
SL_EXPORT int SL_CALL slk_poller_modify_fd(void *poller, slk_fd_t fd, short events);
SL_EXPORT int SL_CALL slk_poller_remove_fd(void *poller, slk_fd_t fd);

/****************************************************************************/
/*  Monitoring & Router Connection Status API                               */
/****************************************************************************/

typedef struct slk_event_t {
    int event;
    const void *peer_id;
    size_t peer_id_len;
    const char *endpoint;
    int err;
    uint64_t timestamp;
} slk_event_t;

typedef void (*slk_monitor_fn)(slk_socket_t *socket, const slk_event_t *event, void *user_data);

SL_EXPORT int SL_CALL slk_socket_monitor(slk_socket_t *socket, slk_monitor_fn callback, void *user_data, int events);

typedef struct slk_peer_stats_t {
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t msgs_sent;
    uint64_t msgs_received;
    uint64_t connected_time;
    uint64_t last_heartbeat;
    int is_alive;
} slk_peer_stats_t;

SL_EXPORT int SL_CALL slk_is_connected(slk_socket_t *socket, const void *routing_id, size_t id_len);
SL_EXPORT int SL_CALL slk_get_peer_stats(slk_socket_t *socket, const void *routing_id,
                                          size_t id_len, slk_peer_stats_t *stats);
SL_EXPORT int SL_CALL slk_get_peers(slk_socket_t *socket, void **peer_ids, size_t *id_lens, size_t *num_peers);
SL_EXPORT void SL_CALL slk_free_peers(void **peer_ids, size_t *id_lens, size_t num_peers);

/****************************************************************************/
/*  Utility Functions                                                       */
/****************************************************************************/

SL_EXPORT uint64_t SL_CALL slk_clock(void);
SL_EXPORT void SL_CALL slk_sleep(int ms);
SL_EXPORT int SL_CALL slk_has(const char *capability);

/****************************************************************************/
/*  Atomic Counter API                                                      */
/****************************************************************************/

SL_EXPORT void* SL_CALL slk_atomic_counter_new(void);
SL_EXPORT void SL_CALL slk_atomic_counter_set(void *counter, int value);
SL_EXPORT int SL_CALL slk_atomic_counter_inc(void *counter);
SL_EXPORT int SL_CALL slk_atomic_counter_dec(void *counter);
SL_EXPORT int SL_CALL slk_atomic_counter_value(void *counter);
SL_EXPORT void SL_CALL slk_atomic_counter_destroy(void **counter_p);

/****************************************************************************/
/*  Timer API                                                               */
/****************************************************************************/

typedef void (*slk_timer_fn)(int timer_id, void *arg);

SL_EXPORT void* SL_CALL slk_timers_new(void);
SL_EXPORT int SL_CALL slk_timers_destroy(void **timers_p);
SL_EXPORT int SL_CALL slk_timers_add(void *timers, size_t interval, slk_timer_fn handler, void *arg);
SL_EXPORT int SL_CALL slk_timers_cancel(void *timers, int timer_id);
SL_EXPORT int SL_CALL slk_timers_set_interval(void *timers, int timer_id, size_t interval);
SL_EXPORT int SL_CALL slk_timers_reset(void *timers, int timer_id);
SL_EXPORT long SL_CALL slk_timers_timeout(void *timers);
SL_EXPORT int SL_CALL slk_timers_execute(void *timers);

/****************************************************************************/
/*  Stopwatch API                                                           */
/****************************************************************************/

SL_EXPORT void* SL_CALL slk_stopwatch_start(void);
SL_EXPORT unsigned long SL_CALL slk_stopwatch_intermediate(void *watch);
SL_EXPORT unsigned long SL_CALL slk_stopwatch_stop(void *watch);

/****************************************************************************/
/*  SPOT PUB/SUB API                                                        */
/****************************************************************************/

typedef struct slk_spot_s slk_spot_t;

SL_EXPORT slk_spot_t* SL_CALL slk_spot_new(slk_ctx_t *ctx);
SL_EXPORT void SL_CALL slk_spot_destroy(slk_spot_t **spot);
SL_EXPORT int SL_CALL slk_spot_topic_create(slk_spot_t *spot, const char *topic_id);
SL_EXPORT int SL_CALL slk_spot_topic_route(slk_spot_t *spot, const char *topic_id, const char *endpoint);
SL_EXPORT int SL_CALL slk_spot_topic_destroy(slk_spot_t *spot, const char *topic_id);
SL_EXPORT int SL_CALL slk_spot_subscribe(slk_spot_t *spot, const char *topic_id);
SL_EXPORT int SL_CALL slk_spot_subscribe_pattern(slk_spot_t *spot, const char *pattern);
SL_EXPORT int SL_CALL slk_spot_unsubscribe(slk_spot_t *spot, const char *topic_id);
SL_EXPORT int SL_CALL slk_spot_publish(slk_spot_t *spot, const char *topic_id, const void *data, size_t len);
SL_EXPORT int SL_CALL slk_spot_recv(slk_spot_t *spot, char *topic, size_t topic_size,
                                     size_t *topic_len, void *data, size_t data_size,
                                     size_t *data_len, int flags);
SL_EXPORT int SL_CALL slk_spot_bind(slk_spot_t *spot, const char *endpoint);
SL_EXPORT int SL_CALL slk_spot_cluster_add(slk_spot_t *spot, const char *endpoint);
SL_EXPORT int SL_CALL slk_spot_cluster_remove(slk_spot_t *spot, const char *endpoint);
SL_EXPORT int SL_CALL slk_spot_cluster_sync(slk_spot_t *spot, int timeout_ms);
SL_EXPORT int SL_CALL slk_spot_list_topics(slk_spot_t *spot, char ***topics, size_t *count);
SL_EXPORT void SL_CALL slk_spot_list_topics_free(char **topics, size_t count);
SL_EXPORT int SL_CALL slk_spot_topic_exists(slk_spot_t *spot, const char *topic_id);
SL_EXPORT int SL_CALL slk_spot_topic_is_local(slk_spot_t *spot, const char *topic_id);
SL_EXPORT int SL_CALL slk_spot_set_hwm(slk_spot_t *spot, int sndhwm, int rcvhwm);
SL_EXPORT int SL_CALL slk_spot_fd(slk_spot_t *spot, slk_fd_t *fd);
SL_EXPORT int SL_CALL slk_spot_setsockopt(slk_spot_t *spot, int option, const void *value, size_t len);
SL_EXPORT int SL_CALL slk_spot_getsockopt(slk_spot_t *spot, int option, void *value, size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* SERVERLINK_H */
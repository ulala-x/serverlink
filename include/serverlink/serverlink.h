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

#define SLK_ROUTER 6  /* Server-side routing socket */

/****************************************************************************/
/*  Socket Options                                                          */
/****************************************************************************/

/* Core routing options */
#define SLK_ROUTING_ID          5   /* Set/get routing identity */
#define SLK_ROUTER_MANDATORY    33  /* Fail if peer not connected */
#define SLK_ROUTER_HANDOVER     56  /* Transfer messages to new peer with same ID */
#define SLK_ROUTER_NOTIFY       97  /* Enable router event notifications */

/* Heartbeat options */
#define SLK_HEARTBEAT_IVL       75  /* Heartbeat interval in ms */
#define SLK_HEARTBEAT_TIMEOUT   77  /* Heartbeat timeout in ms */
#define SLK_HEARTBEAT_TTL       76  /* Heartbeat time-to-live (hops) */

/* TCP transport options */
#define SLK_TCP_KEEPALIVE       34  /* Enable TCP keepalive (0/1) */
#define SLK_TCP_KEEPALIVE_IDLE  36  /* TCP keepalive idle time (seconds) */
#define SLK_TCP_KEEPALIVE_INTVL 37  /* TCP keepalive interval (seconds) */
#define SLK_TCP_KEEPALIVE_CNT   35  /* TCP keepalive probe count */

/* Connection options */
#define SLK_LINGER              17  /* Linger time on close (ms, -1=infinite) */
#define SLK_RECONNECT_IVL       18  /* Reconnect interval in ms */
#define SLK_RECONNECT_IVL_MAX   21  /* Max reconnect interval in ms */
#define SLK_BACKLOG             19  /* Listen backlog size */

/* Buffer options */
#define SLK_SNDBUF              11  /* Send buffer size (bytes) */
#define SLK_RCVBUF              12  /* Receive buffer size (bytes) */
#define SLK_SNDHWM              23  /* Send high water mark (messages) */
#define SLK_RCVHWM              24  /* Receive high water mark (messages) */

/* Security options (future use) */
#define SLK_AUTH_ENABLED        200 /* Enable authentication */
#define SLK_AUTH_TIMEOUT        201 /* Authentication timeout in ms */

/* Monitoring options (future use) */
#define SLK_MONITOR_EVENTS      202 /* Event mask for monitoring */

/****************************************************************************/
/*  Message Flags                                                           */
/****************************************************************************/

#define SLK_DONTWAIT    1   /* Non-blocking mode */
#define SLK_SNDMORE     2   /* More message parts follow */

/****************************************************************************/
/*  Event Types (for monitoring)                                            */
/****************************************************************************/

#define SLK_EVENT_CONNECTED       1   /* Peer connected */
#define SLK_EVENT_DISCONNECTED    2   /* Peer disconnected */
#define SLK_EVENT_ACCEPTED        3   /* Connection accepted */
#define SLK_EVENT_BIND_FAILED     4   /* Bind failed */
#define SLK_EVENT_LISTENING       5   /* Socket listening */
#define SLK_EVENT_CLOSED          6   /* Socket closed */
#define SLK_EVENT_HANDSHAKE_START 7   /* Handshake started */
#define SLK_EVENT_HANDSHAKE_OK    8   /* Handshake succeeded */
#define SLK_EVENT_HANDSHAKE_FAIL  9   /* Handshake failed */
#define SLK_EVENT_HEARTBEAT_OK    10  /* Heartbeat received */
#define SLK_EVENT_HEARTBEAT_FAIL  11  /* Heartbeat timeout */

/* Event masks (bitwise OR) */
#define SLK_EVENT_ALL 0xFFFF

/****************************************************************************/
/*  Error Codes                                                             */
/****************************************************************************/

/* Standard POSIX-like error codes */
#define SLK_EINVAL      1   /* Invalid argument */
#define SLK_ENOMEM      2   /* Out of memory */
#define SLK_EAGAIN      3   /* Resource temporarily unavailable */
#define SLK_ENOTSOCK    4   /* Not a socket */
#define SLK_EPROTO      5   /* Protocol error */
#define SLK_ETERM       6   /* Context terminated */
#define SLK_EMTHREAD    7   /* No I/O thread available */

/* Library-specific error codes */
#define SLK_EHOSTUNREACH    10  /* Host unreachable */
#define SLK_ENOTREADY       11  /* Socket not ready */
#define SLK_EPEERUNREACH    12  /* Peer unreachable */
#define SLK_EAUTH           13  /* Authentication failed */

SL_EXPORT int SL_CALL slk_errno(void);
SL_EXPORT const char* SL_CALL slk_strerror(int errnum);

/****************************************************************************/
/*  Core Context and Socket API                                             */
/****************************************************************************/

/* Opaque types */
typedef struct slk_ctx_t slk_ctx_t;
typedef struct slk_socket_t slk_socket_t;
typedef struct slk_msg_t slk_msg_t;

/* Context management */
SL_EXPORT slk_ctx_t* SL_CALL slk_ctx_new(void);
SL_EXPORT void SL_CALL slk_ctx_destroy(slk_ctx_t *ctx);
SL_EXPORT int SL_CALL slk_ctx_set(slk_ctx_t *ctx, int option, const void *value, size_t len);
SL_EXPORT int SL_CALL slk_ctx_get(slk_ctx_t *ctx, int option, void *value, size_t *len);

/* Socket management */
SL_EXPORT slk_socket_t* SL_CALL slk_socket(slk_ctx_t *ctx, int type);
SL_EXPORT int SL_CALL slk_close(slk_socket_t *socket);
SL_EXPORT int SL_CALL slk_bind(slk_socket_t *socket, const char *endpoint);
SL_EXPORT int SL_CALL slk_connect(slk_socket_t *socket, const char *endpoint);
SL_EXPORT int SL_CALL slk_disconnect(slk_socket_t *socket, const char *endpoint);
SL_EXPORT int SL_CALL slk_unbind(slk_socket_t *socket, const char *endpoint);

/* Socket options */
SL_EXPORT int SL_CALL slk_setsockopt(slk_socket_t *socket, int option, const void *value, size_t len);
SL_EXPORT int SL_CALL slk_getsockopt(slk_socket_t *socket, int option, void *value, size_t *len);

/****************************************************************************/
/*  Message API                                                             */
/****************************************************************************/

/* Message management */
SL_EXPORT slk_msg_t* SL_CALL slk_msg_new(void);
SL_EXPORT slk_msg_t* SL_CALL slk_msg_new_data(const void *data, size_t size);
SL_EXPORT void SL_CALL slk_msg_destroy(slk_msg_t *msg);
SL_EXPORT int SL_CALL slk_msg_init(slk_msg_t *msg);
SL_EXPORT int SL_CALL slk_msg_init_data(slk_msg_t *msg, const void *data, size_t size);
SL_EXPORT int SL_CALL slk_msg_close(slk_msg_t *msg);

/* Message data access */
SL_EXPORT void* SL_CALL slk_msg_data(slk_msg_t *msg);
SL_EXPORT size_t SL_CALL slk_msg_size(slk_msg_t *msg);
SL_EXPORT int SL_CALL slk_msg_copy(slk_msg_t *dest, slk_msg_t *src);
SL_EXPORT int SL_CALL slk_msg_move(slk_msg_t *dest, slk_msg_t *src);

/* Message properties */
SL_EXPORT int SL_CALL slk_msg_get(slk_msg_t *msg, int property, void *value, size_t *len);
SL_EXPORT int SL_CALL slk_msg_set(slk_msg_t *msg, int property, const void *value, size_t len);

/* Routing information */
SL_EXPORT int SL_CALL slk_msg_get_routing_id(slk_msg_t *msg, void *id, size_t *size);
SL_EXPORT int SL_CALL slk_msg_set_routing_id(slk_msg_t *msg, const void *id, size_t size);

/****************************************************************************/
/*  Send/Receive API                                                        */
/****************************************************************************/

/* Simple send/receive (single-part messages) */
SL_EXPORT int SL_CALL slk_send(slk_socket_t *socket, const void *data, size_t len, int flags);
SL_EXPORT int SL_CALL slk_recv(slk_socket_t *socket, void *buf, size_t len, int flags);

/* Message send/receive (multi-part support) */
SL_EXPORT int SL_CALL slk_msg_send(slk_msg_t *msg, slk_socket_t *socket, int flags);
SL_EXPORT int SL_CALL slk_msg_recv(slk_msg_t *msg, slk_socket_t *socket, int flags);

/* Router-specific API: send to specific peer */
SL_EXPORT int SL_CALL slk_send_to(slk_socket_t *socket, const void *routing_id, size_t id_len,
                                   const void *data, size_t data_len, int flags);

/****************************************************************************/
/*  Polling API                                                             */
/****************************************************************************/

#define SLK_POLLIN  1   /* Ready for reading */
#define SLK_POLLOUT 2   /* Ready for writing */
#define SLK_POLLERR 4   /* Error condition */

typedef struct slk_pollitem_t {
    slk_socket_t *socket;   /* ServerLink socket to poll */
    int fd;                 /* OS file descriptor to poll (or -1) */
    short events;           /* Events to poll for (in) */
    short revents;          /* Events that occurred (out) */
} slk_pollitem_t;

SL_EXPORT int SL_CALL slk_poll(slk_pollitem_t *items, int nitems, long timeout);

/****************************************************************************/
/*  Monitoring API                                                          */
/****************************************************************************/

/* Event structure passed to monitor callback */
typedef struct slk_event_t {
    int event;              /* Event type (SLK_EVENT_*) */
    const void *peer_id;    /* Peer routing ID */
    size_t peer_id_len;     /* Peer routing ID length */
    const char *endpoint;   /* Endpoint address */
    int err;                /* Error code (if applicable) */
    uint64_t timestamp;     /* Event timestamp (ms since epoch) */
} slk_event_t;

/* Monitor callback function type */
typedef void (*slk_monitor_fn)(slk_socket_t *socket, const slk_event_t *event, void *user_data);

/* Socket monitoring */
SL_EXPORT int SL_CALL slk_socket_monitor(slk_socket_t *socket, slk_monitor_fn callback,
                                          void *user_data, int events);

/****************************************************************************/
/*  Router Connection Status API                                            */
/****************************************************************************/

/* Peer statistics */
typedef struct slk_peer_stats_t {
    uint64_t bytes_sent;        /* Total bytes sent to peer */
    uint64_t bytes_received;    /* Total bytes received from peer */
    uint64_t msgs_sent;         /* Total messages sent to peer */
    uint64_t msgs_received;     /* Total messages received from peer */
    uint64_t connected_time;    /* Time connected (ms) */
    uint64_t last_heartbeat;    /* Last heartbeat timestamp */
    int is_alive;               /* Heartbeat status (0/1) */
} slk_peer_stats_t;

/* Check if peer is connected */
SL_EXPORT int SL_CALL slk_is_connected(slk_socket_t *socket, const void *routing_id, size_t id_len);

/* Get peer statistics */
SL_EXPORT int SL_CALL slk_get_peer_stats(slk_socket_t *socket, const void *routing_id,
                                          size_t id_len, slk_peer_stats_t *stats);

/* Get list of connected peers */
SL_EXPORT int SL_CALL slk_get_peers(slk_socket_t *socket, void **peer_ids, size_t *id_lens,
                                     size_t *num_peers);

/* Free peer list returned by slk_get_peers */
SL_EXPORT void SL_CALL slk_free_peers(void **peer_ids, size_t *id_lens, size_t num_peers);

/****************************************************************************/
/*  Utility Functions                                                       */
/****************************************************************************/

/* Get current high-resolution timestamp (microseconds) */
SL_EXPORT uint64_t SL_CALL slk_clock(void);

/* Sleep for specified milliseconds */
SL_EXPORT void SL_CALL slk_sleep(int ms);

#ifdef __cplusplus
}
#endif

#endif /* SERVERLINK_H */

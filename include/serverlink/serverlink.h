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

#define SLK_PUB    1  /* Publisher socket */
#define SLK_SUB    2  /* Subscriber socket */
#define SLK_ROUTER 6  /* Server-side routing socket */
#define SLK_XPUB   9  /* Publisher socket with subscription visibility */
#define SLK_XSUB   10 /* Subscriber socket with manual subscription management */

/****************************************************************************/
/*  Socket Options                                                          */
/****************************************************************************/

/* Core routing options */
#define SLK_ROUTING_ID          5   /* Set/get routing identity */
#define SLK_CONNECT_ROUTING_ID  61  /* Set peer's routing ID when connecting */
#define SLK_ROUTER_MANDATORY    33  /* Fail if peer not connected */
#define SLK_ROUTER_HANDOVER     56  /* Transfer messages to new peer with same ID */
#define SLK_ROUTER_NOTIFY       97  /* Enable router event notifications */

/* Endpoint information */
#define SLK_LAST_ENDPOINT       32  /* Get last bound/connected endpoint */

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

/* Pub/Sub options */
#define SLK_SUBSCRIBE           6   /* Add subscription filter */
#define SLK_UNSUBSCRIBE         7   /* Remove subscription filter */
#define SLK_XPUB_VERBOSE        40  /* Send all subscription messages */
#define SLK_XPUB_VERBOSER       78  /* Send all subscription and unsubscription messages */
#define SLK_XPUB_NODROP         69  /* Block instead of drop when HWM reached */
#define SLK_XPUB_MANUAL         71  /* Manual subscription management mode */
#define SLK_XPUB_MANUAL_LAST_VALUE  70  /* Manual mode with last value caching */
#define SLK_XPUB_WELCOME_MSG    72  /* Welcome message for new subscribers */
#define SLK_ONLY_FIRST_SUBSCRIBE    108 /* Process only first subscribe in multipart */
#define SLK_TOPICS_COUNT        80  /* Get number of active subscriptions */
#define SLK_INVERT_MATCHING     60  /* Invert subscription matching logic */
#define SLK_XSUB_VERBOSE_UNSUBSCRIBE 73  /* Send all unsubscribe messages upstream */

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
/*  Context Options                                                         */
/****************************************************************************/

#define SLK_IO_THREADS                  1   /* Number of I/O threads (int) */
#define SLK_MAX_SOCKETS                 2   /* Maximum number of sockets (int) */
#define SLK_SOCKET_LIMIT                3   /* Maximum socket limit (read-only, int) */
#define SLK_THREAD_SCHED_POLICY         6   /* Thread scheduling policy (int) */
#define SLK_THREAD_AFFINITY_CPU_ADD     7   /* Add CPU to thread affinity (int) */
#define SLK_THREAD_AFFINITY_CPU_REMOVE  8   /* Remove CPU from thread affinity (int) */
#define SLK_THREAD_PRIORITY             9   /* Thread priority (int) */
#define SLK_THREAD_NAME_PREFIX          10  /* Thread name prefix (string/int) */
#define SLK_MAX_MSGSZ                   13  /* Maximum message size (int) */
#define SLK_MSG_T_SIZE                  14  /* Size of slk_msg_t (read-only, size_t) */

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
/*  Modern Poller API (Recommended)                                         */
/****************************************************************************/

/* Platform-specific file descriptor type */
#if defined(_WIN32)
    typedef void* slk_fd_t;
#else
    typedef int slk_fd_t;
#endif

/* Poller event structure */
typedef struct slk_poller_event_t {
    void *socket;           /* ServerLink socket or NULL for fd */
    slk_fd_t fd;            /* File descriptor or -1 for socket */
    void *user_data;        /* User data associated with socket/fd */
    short events;           /* Events that occurred */
} slk_poller_event_t;

/* Create a new poller instance */
SL_EXPORT void* SL_CALL slk_poller_new(void);

/* Destroy a poller instance */
SL_EXPORT int SL_CALL slk_poller_destroy(void **poller_p);

/* Get number of registered items in poller */
SL_EXPORT int SL_CALL slk_poller_size(void *poller);

/* Add a socket to the poller */
SL_EXPORT int SL_CALL slk_poller_add(void *poller, void *socket, void *user_data, short events);

/* Modify events for a registered socket */
SL_EXPORT int SL_CALL slk_poller_modify(void *poller, void *socket, short events);

/* Remove a socket from the poller */
SL_EXPORT int SL_CALL slk_poller_remove(void *poller, void *socket);

/* Wait for events on registered sockets (returns 0 on success, -1 on error) */
SL_EXPORT int SL_CALL slk_poller_wait(void *poller, slk_poller_event_t *event, long timeout);

/* Wait for events on all registered sockets (returns number of events, -1 on error) */
SL_EXPORT int SL_CALL slk_poller_wait_all(void *poller, slk_poller_event_t *events,
                                           int n_events, long timeout);

/* Get the signaler file descriptor (for thread-safe sockets) */
SL_EXPORT int SL_CALL slk_poller_fd(void *poller, slk_fd_t *fd);

/* Add a raw file descriptor to the poller */
SL_EXPORT int SL_CALL slk_poller_add_fd(void *poller, slk_fd_t fd, void *user_data, short events);

/* Modify events for a registered file descriptor */
SL_EXPORT int SL_CALL slk_poller_modify_fd(void *poller, slk_fd_t fd, short events);

/* Remove a file descriptor from the poller */
SL_EXPORT int SL_CALL slk_poller_remove_fd(void *poller, slk_fd_t fd);

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

/* Check if a capability is supported */
SL_EXPORT int SL_CALL slk_has(const char *capability);

/****************************************************************************/
/*  Atomic Counter API                                                      */
/****************************************************************************/

/* Create a new atomic counter */
SL_EXPORT void* SL_CALL slk_atomic_counter_new(void);

/* Set counter value (not thread-safe) */
SL_EXPORT void SL_CALL slk_atomic_counter_set(void *counter, int value);

/* Increment counter and return old value */
SL_EXPORT int SL_CALL slk_atomic_counter_inc(void *counter);

/* Decrement counter and return new value */
SL_EXPORT int SL_CALL slk_atomic_counter_dec(void *counter);

/* Get current counter value */
SL_EXPORT int SL_CALL slk_atomic_counter_value(void *counter);

/* Destroy atomic counter */
SL_EXPORT void SL_CALL slk_atomic_counter_destroy(void **counter_p);

/****************************************************************************/
/*  Timer API                                                               */
/****************************************************************************/

/* Timer callback function type */
typedef void (*slk_timer_fn)(int timer_id, void *arg);

/* Create a new timers object */
SL_EXPORT void* SL_CALL slk_timers_new(void);

/* Destroy timers object */
SL_EXPORT int SL_CALL slk_timers_destroy(void **timers_p);

/* Add a timer with specified interval (ms) and handler */
SL_EXPORT int SL_CALL slk_timers_add(void *timers, size_t interval, slk_timer_fn handler, void *arg);

/* Cancel a timer by ID */
SL_EXPORT int SL_CALL slk_timers_cancel(void *timers, int timer_id);

/* Set interval for an existing timer */
SL_EXPORT int SL_CALL slk_timers_set_interval(void *timers, int timer_id, size_t interval);

/* Reset a timer (restart from current time) */
SL_EXPORT int SL_CALL slk_timers_reset(void *timers, int timer_id);

/* Get timeout until next timer (returns -1 if no timers active) */
SL_EXPORT long SL_CALL slk_timers_timeout(void *timers);

/* Execute ready timers */
SL_EXPORT int SL_CALL slk_timers_execute(void *timers);

/****************************************************************************/
/*  Stopwatch API                                                           */
/****************************************************************************/

/* Start a new stopwatch */
SL_EXPORT void* SL_CALL slk_stopwatch_start(void);

/* Get intermediate time (microseconds since start) */
SL_EXPORT unsigned long SL_CALL slk_stopwatch_intermediate(void *watch);

/* Stop stopwatch and return elapsed time (microseconds) */
SL_EXPORT unsigned long SL_CALL slk_stopwatch_stop(void *watch);

/****************************************************************************/
/*  Proxy API                                                               */
/****************************************************************************/

/* Simple proxy - forwards messages between frontend and backend sockets
 *
 * The proxy connects a frontend socket to a backend socket. Conceptually,
 * data flows from the frontend to the backend. Messages are read from the
 * frontend and written to the backend, and vice versa.
 *
 * If the capture socket is not NULL, the proxy sends all messages received
 * on both frontend and backend to the capture socket, enabling message
 * monitoring and logging.
 *
 * This function blocks until an error occurs or a signal is received.
 *
 * Example usage:
 *   Frontend: ROUTER socket bound to tcp://0.0.0.0:5555
 *   Backend:  DEALER socket bound to tcp://0.0.0.0:5556
 *   Capture:  PUB socket bound to tcp://0.0.0.0:5557 (optional)
 *
 * Parameters:
 *   frontend - Frontend socket (receives from clients)
 *   backend  - Backend socket (sends to workers)
 *   capture  - Optional capture socket for monitoring (can be NULL)
 *
 * Returns:
 *   0 on success (graceful termination)
 *   -1 on error (sets errno)
 */
SL_EXPORT int SL_CALL slk_proxy(void *frontend, void *backend, void *capture);

/* Steerable proxy - adds runtime control via control socket
 *
 * This is an extended version of slk_proxy that accepts a control socket
 * for runtime control. The control socket must be a REQ socket (or compatible).
 *
 * Control commands (single-part messages):
 *   "PAUSE"      - Stop forwarding messages (messages queue up)
 *   "RESUME"     - Resume forwarding messages
 *   "TERMINATE"  - Terminate the proxy and return
 *   "STATISTICS" - Return statistics (8-part message)
 *
 * The STATISTICS reply is an 8-part message containing uint64_t values:
 *   Part 0: Frontend messages received count
 *   Part 1: Frontend bytes received
 *   Part 2: Frontend messages sent count
 *   Part 3: Frontend bytes sent
 *   Part 4: Backend messages received count
 *   Part 5: Backend bytes received
 *   Part 6: Backend messages sent count
 *   Part 7: Backend bytes sent
 *
 * Example usage with control:
 *   void *control = slk_socket(ctx, SLK_REQ);
 *   slk_connect(control, "inproc://proxy-control");
 *
 *   // In proxy thread:
 *   void *control_rep = slk_socket(ctx, SLK_REP);
 *   slk_bind(control_rep, "inproc://proxy-control");
 *   slk_proxy_steerable(frontend, backend, capture, control_rep);
 *
 *   // In main thread:
 *   slk_send(control, "PAUSE", 5, 0);
 *   slk_recv(control, reply, 256, 0); // Wait for acknowledgment
 *
 * Parameters:
 *   frontend - Frontend socket (receives from clients)
 *   backend  - Backend socket (sends to workers)
 *   capture  - Optional capture socket for monitoring (can be NULL)
 *   control  - Optional control socket for runtime control (can be NULL)
 *
 * Returns:
 *   0 on success (graceful termination via TERMINATE command)
 *   -1 on error (sets errno)
 */
SL_EXPORT int SL_CALL slk_proxy_steerable(void *frontend, void *backend,
                                           void *capture, void *control);

#ifdef __cplusplus
}
#endif

#endif /* SERVERLINK_H */

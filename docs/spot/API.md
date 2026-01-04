# SPOT PUB/SUB API Reference

Complete API reference for ServerLink SPOT (Scalable Partitioned Ordered Topics).

## Table of Contents

- [Overview](#overview)
- [Data Types](#data-types)
- [Lifecycle Management](#lifecycle-management)
- [Topic Management](#topic-management)
- [Publishing and Subscribing](#publishing-and-subscribing)
- [Cluster Management](#cluster-management)
- [Introspection](#introspection)
- [Configuration](#configuration)
- [Event Loop Integration](#event-loop-integration)
- [Error Handling](#error-handling)

---

## Overview

SPOT provides location-transparent pub/sub using topic ID-based routing. Topics can be **LOCAL** (hosted on this node) or **REMOTE** (routed to other nodes).

**Key Features:**
- Topic ownership and registration
- Exact and pattern subscriptions
- Position-transparent publish/subscribe (inproc/tcp)
- Cluster synchronization for distributed topics
- Zero-copy message passing for local topics
- Automatic failover and reconnection

---

## Data Types

### slk_spot_t

```c
typedef struct slk_spot_s slk_spot_t;
```

Opaque handle to a SPOT PUB/SUB instance. All SPOT operations require this handle.

---

## Lifecycle Management

### slk_spot_new

```c
slk_spot_t* slk_spot_new(slk_ctx_t *ctx);
```

Create a new SPOT PUB/SUB instance.

**Parameters:**
- `ctx` - ServerLink context (must not be NULL)

**Returns:**
- New SPOT instance on success
- `NULL` on error (sets errno)

**Error Codes:**
- `ENOMEM` - Out of memory
- `EINVAL` - Invalid context

**Example:**
```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);
if (!spot) {
    fprintf(stderr, "Failed to create SPOT: %s\n", slk_strerror(slk_errno()));
    return -1;
}
```

**Notes:**
- Creates internal XSUB socket for receiving messages
- Default HWM: 1000 messages (send and receive)
- Thread-safe

---

### slk_spot_destroy

```c
void slk_spot_destroy(slk_spot_t **spot);
```

Destroy a SPOT PUB/SUB instance and free all resources.

**Parameters:**
- `spot` - Pointer to SPOT instance (set to NULL on return)

**Example:**
```c
slk_spot_destroy(&spot);
// spot is now NULL
```

**Notes:**
- Closes all sockets (XPUB, XSUB, ROUTER)
- Unregisters all topics
- Disconnects from all cluster nodes
- Safe to call with NULL pointer

---

## Topic Management

### slk_spot_topic_create

```c
int slk_spot_topic_create(slk_spot_t *spot, const char *topic_id);
```

Create a **LOCAL** topic (this node is the publisher).

**Parameters:**
- `spot` - SPOT instance
- `topic_id` - Topic identifier (null-terminated string)

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Error Codes:**
- `EEXIST` - Topic already exists
- `ENOMEM` - Out of memory
- `EINVAL` - Invalid parameters

**Example:**
```c
int rc = slk_spot_topic_create(spot, "game:player123");
if (rc != 0) {
    fprintf(stderr, "Failed to create topic: %s\n", slk_strerror(slk_errno()));
}
```

**Notes:**
- Creates XPUB socket bound to unique inproc endpoint
- Topic becomes locally owned
- Can be published to immediately
- Topic ID should be unique across the cluster

**Topic Naming Conventions:**
- Use colon-separated hierarchies: `"game:player:123"`
- Prefix with domain: `"chat:room:lobby"`
- Avoid special characters: `/ \ * ? < > |`

---

### slk_spot_topic_route

```c
int slk_spot_topic_route(slk_spot_t *spot, const char *topic_id,
                          const char *endpoint);
```

Route a topic to a **REMOTE** endpoint.

**Parameters:**
- `spot` - SPOT instance
- `topic_id` - Topic identifier
- `endpoint` - Remote endpoint (e.g., `"tcp://192.168.1.100:5555"`)

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Error Codes:**
- `EEXIST` - Topic already exists
- `EHOSTUNREACH` - Connection to endpoint failed
- `EINVAL` - Invalid endpoint format

**Example:**
```c
int rc = slk_spot_topic_route(spot, "remote:sensor", "tcp://192.168.1.100:5555");
if (rc != 0) {
    fprintf(stderr, "Failed to route topic: %s\n", slk_strerror(slk_errno()));
}
```

**Notes:**
- Establishes connection to remote node if needed
- Registers topic as REMOTE in registry
- Multiple topics can share the same endpoint
- Connection is persistent with automatic reconnection

---

### slk_spot_topic_destroy

```c
int slk_spot_topic_destroy(slk_spot_t *spot, const char *topic_id);
```

Destroy a topic and unregister it.

**Parameters:**
- `spot` - SPOT instance
- `topic_id` - Topic identifier

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Error Codes:**
- `ENOENT` - Topic not found

**Example:**
```c
int rc = slk_spot_topic_destroy(spot, "game:player123");
```

**Notes:**
- Closes topic's XPUB socket (if LOCAL)
- Removes from registry
- Active subscriptions are unaffected (will fail on next use)

---

## Publishing and Subscribing

### slk_spot_subscribe

```c
int slk_spot_subscribe(slk_spot_t *spot, const char *topic_id);
```

Subscribe to a topic (LOCAL or REMOTE).

**Parameters:**
- `spot` - SPOT instance
- `topic_id` - Topic identifier

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Error Codes:**
- `ENOENT` - Topic not found in registry
- `EEXIST` - Already subscribed (idempotent, not an error)

**Example:**
```c
// Subscribe to LOCAL topic
int rc = slk_spot_subscribe(spot, "game:player123");

// Subscribe to REMOTE topic
rc = slk_spot_subscribe(spot, "remote:sensor");
```

**Notes:**
- **LOCAL topics:** Connects XSUB to topic's inproc endpoint
- **REMOTE topics:** Sends SUBSCRIBE command to remote node
- Idempotent: calling twice is safe
- Subscription persists until explicit unsubscribe or destruction

---

### slk_spot_subscribe_pattern

```c
int slk_spot_subscribe_pattern(slk_spot_t *spot, const char *pattern);
```

Subscribe to a pattern (**LOCAL topics only**).

**Parameters:**
- `spot` - SPOT instance
- `pattern` - Pattern string with optional `*` wildcard

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**
```c
// Subscribe to all player topics
slk_spot_subscribe_pattern(spot, "game:player:*");

// Subscribe to all chat messages
slk_spot_subscribe_pattern(spot, "chat:*");
```

**Pattern Matching Rules:**
- `*` matches zero or more characters
- Prefix matching: `"game:*"` matches `"game:player"`, `"game:score"`
- Only works with LOCAL topics
- Multiple patterns can be active simultaneously

**Notes:**
- Pattern subscriptions are LOCAL only
- No pattern matching for REMOTE topics
- Filters during `slk_spot_recv()`

---

### slk_spot_unsubscribe

```c
int slk_spot_unsubscribe(slk_spot_t *spot, const char *topic_id);
```

Unsubscribe from a topic.

**Parameters:**
- `spot` - SPOT instance
- `topic_id` - Topic identifier

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Error Codes:**
- `ENOENT` - Not subscribed or topic not found

**Example:**
```c
int rc = slk_spot_unsubscribe(spot, "game:player123");
```

**Notes:**
- **LOCAL topics:** Sends unsubscription message to XPUB
- **REMOTE topics:** Sends UNSUBSCRIBE command to remote node
- Idempotent: safe to call multiple times

---

### slk_spot_publish

```c
int slk_spot_publish(slk_spot_t *spot, const char *topic_id,
                      const void *data, size_t len);
```

Publish a message to a topic.

**Parameters:**
- `spot` - SPOT instance
- `topic_id` - Topic identifier
- `data` - Message data pointer
- `len` - Message data length (bytes)

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Error Codes:**
- `ENOENT` - Topic not found
- `EAGAIN` - HWM reached (non-blocking mode)
- `EINVAL` - Invalid parameters

**Message Format:**
```
Frame 0: topic_id (variable length)
Frame 1: data (variable length)
```

**Example:**
```c
const char *msg = "Player joined!";
int rc = slk_spot_publish(spot, "game:player123", msg, strlen(msg));
if (rc != 0) {
    if (slk_errno() == SLK_EAGAIN) {
        fprintf(stderr, "Send buffer full\n");
    }
}
```

**Notes:**
- **LOCAL topics:** Sends to XPUB socket
- **REMOTE topics:** Sends PUBLISH command to remote node
- Zero-copy for LOCAL topics (inproc)
- Subject to HWM limits (default: 1000 messages)

---

### slk_spot_recv

```c
int slk_spot_recv(slk_spot_t *spot, char *topic, size_t topic_size,
                   size_t *topic_len, void *data, size_t data_size,
                   size_t *data_len, int flags);
```

Receive a message (topic and data separated).

**Parameters:**
- `spot` - SPOT instance
- `topic` - Output buffer for topic ID
- `topic_size` - Size of topic buffer
- `topic_len` - Output: actual topic length
- `data` - Output buffer for message data
- `data_size` - Size of data buffer
- `data_len` - Output: actual data length
- `flags` - Receive flags (`SLK_DONTWAIT` for non-blocking)

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Error Codes:**
- `EAGAIN` - No message available (non-blocking)
- `EMSGSIZE` - Buffer too small for message
- `EINVAL` - Invalid parameters

**Example:**
```c
char topic[256], data[4096];
size_t topic_len, data_len;

int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, SLK_DONTWAIT);
if (rc == 0) {
    topic[topic_len] = '\0';
    printf("Received on topic '%s': %zu bytes\n", topic, data_len);
} else if (slk_errno() == SLK_EAGAIN) {
    // No message available
}
```

**Notes:**
- Checks LOCAL topics first (XSUB socket)
- Then checks REMOTE topics (all nodes)
- Processes QUERY requests from cluster nodes
- Blocking mode waits on LOCAL socket only
- Pattern filtering applied during receive

---

## Cluster Management

### slk_spot_bind

```c
int slk_spot_bind(slk_spot_t *spot, const char *endpoint);
```

Bind to an endpoint for server mode (accepting cluster connections).

**Parameters:**
- `spot` - SPOT instance
- `endpoint` - Bind endpoint (e.g., `"tcp://*:5555"`)

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Error Codes:**
- `EEXIST` - Already bound
- `EADDRINUSE` - Address already in use

**Example:**
```c
int rc = slk_spot_bind(spot, "tcp://*:5555");
if (rc != 0) {
    fprintf(stderr, "Failed to bind: %s\n", slk_strerror(slk_errno()));
}
```

**Notes:**
- Creates ROUTER socket for accepting connections
- Required for cluster synchronization
- Must be called before `slk_spot_cluster_sync()`
- Endpoint format: `tcp://interface:port` or `tcp://*:port`

---

### slk_spot_cluster_add

```c
int slk_spot_cluster_add(slk_spot_t *spot, const char *endpoint);
```

Add a cluster node (establish connection to remote SPOT node).

**Parameters:**
- `spot` - SPOT instance
- `endpoint` - Remote node endpoint (e.g., `"tcp://192.168.1.100:5555"`)

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Error Codes:**
- `EEXIST` - Node already added
- `EHOSTUNREACH` - Connection failed

**Example:**
```c
int rc = slk_spot_cluster_add(spot, "tcp://192.168.1.100:5555");
```

**Notes:**
- Establishes persistent connection
- Automatic reconnection on failure
- Use with `slk_spot_cluster_sync()` to discover topics

---

### slk_spot_cluster_remove

```c
int slk_spot_cluster_remove(slk_spot_t *spot, const char *endpoint);
```

Remove a cluster node (disconnect from remote SPOT node).

**Parameters:**
- `spot` - SPOT instance
- `endpoint` - Remote node endpoint

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Error Codes:**
- `ENOENT` - Node not found

**Example:**
```c
int rc = slk_spot_cluster_remove(spot, "tcp://192.168.1.100:5555");
```

**Notes:**
- Closes connection to remote node
- Removes all REMOTE topics associated with this node
- Active subscriptions to removed topics will fail

---

### slk_spot_cluster_sync

```c
int slk_spot_cluster_sync(slk_spot_t *spot, int timeout_ms);
```

Synchronize topics with cluster nodes.

**Parameters:**
- `spot` - SPOT instance
- `timeout_ms` - Timeout in milliseconds for sync operation

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Protocol:**
1. Sends QUERY command to all cluster nodes
2. Receives QUERY_RESP with topic lists
3. Registers discovered topics as REMOTE

**Example:**
```c
// After adding cluster nodes
int rc = slk_spot_cluster_sync(spot, 1000); // 1 second timeout
if (rc == 0) {
    printf("Cluster synchronized\n");
}
```

**Notes:**
- Must call `slk_spot_bind()` first
- Updates local registry with remote topics
- Non-blocking for individual nodes
- Returns success even if some nodes timeout

---

## Introspection

### slk_spot_list_topics

```c
int slk_spot_list_topics(slk_spot_t *spot, char ***topics, size_t *count);
```

List all registered topics (LOCAL + REMOTE).

**Parameters:**
- `spot` - SPOT instance
- `topics` - Output: array of topic ID strings
- `count` - Output: number of topics

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**
```c
char **topics;
size_t count;

if (slk_spot_list_topics(spot, &topics, &count) == 0) {
    for (size_t i = 0; i < count; i++) {
        printf("Topic %zu: %s\n", i, topics[i]);
    }
    slk_spot_list_topics_free(topics, count);
}
```

**Notes:**
- Returns dynamically allocated array
- Caller must free using `slk_spot_list_topics_free()`

---

### slk_spot_list_topics_free

```c
void slk_spot_list_topics_free(char **topics, size_t count);
```

Free topic list returned by `slk_spot_list_topics`.

**Parameters:**
- `topics` - Array of topic ID strings
- `count` - Number of topics

---

### slk_spot_topic_exists

```c
int slk_spot_topic_exists(slk_spot_t *spot, const char *topic_id);
```

Check if a topic exists.

**Parameters:**
- `spot` - SPOT instance
- `topic_id` - Topic identifier

**Returns:**
- `1` if topic exists
- `0` if not found
- `-1` on error

**Example:**
```c
if (slk_spot_topic_exists(spot, "game:player123")) {
    printf("Topic exists\n");
}
```

---

### slk_spot_topic_is_local

```c
int slk_spot_topic_is_local(slk_spot_t *spot, const char *topic_id);
```

Check if a topic is LOCAL.

**Parameters:**
- `spot` - SPOT instance
- `topic_id` - Topic identifier

**Returns:**
- `1` if topic is LOCAL
- `0` if REMOTE or not found
- `-1` on error

**Example:**
```c
if (slk_spot_topic_is_local(spot, "game:player123")) {
    printf("Topic is LOCAL\n");
} else {
    printf("Topic is REMOTE or not found\n");
}
```

---

## Configuration

### slk_spot_set_hwm

```c
int slk_spot_set_hwm(slk_spot_t *spot, int sndhwm, int rcvhwm);
```

Set high water marks (message queue limits).

**Parameters:**
- `spot` - SPOT instance
- `sndhwm` - Send high water mark (messages)
- `rcvhwm` - Receive high water mark (messages)

**Returns:**
- `0` on success
- `-1` on error

**Example:**
```c
// Set limits to 10,000 messages
int rc = slk_spot_set_hwm(spot, 10000, 10000);
```

**Notes:**
- Default: 1000 messages for both send and receive
- Applies to all existing and future sockets
- When HWM reached:
  - Blocking mode: blocks until space available
  - Non-blocking mode: returns `EAGAIN`

---

## Event Loop Integration

### slk_spot_fd

```c
int slk_spot_fd(slk_spot_t *spot, slk_fd_t *fd);
```

Get pollable file descriptor for the receive socket.

**Parameters:**
- `spot` - SPOT instance
- `fd` - Output: file descriptor

**Returns:**
- `0` on success
- `-1` on error

**Example:**
```c
slk_fd_t fd;
if (slk_spot_fd(spot, &fd) == 0) {
    // Use with poll/epoll/select
}
```

**Notes:**
- Returns FD for XSUB socket (LOCAL topics)
- Use with `poll()`, `epoll()`, or `select()`
- For REMOTE topics, poll individual node FDs

---

## Error Handling

### Error Codes

All functions return `-1` on error and set `errno` via `slk_errno()`.

**Common Error Codes:**
- `EINVAL` - Invalid argument
- `ENOMEM` - Out of memory
- `ENOENT` - Topic or node not found
- `EEXIST` - Topic or node already exists
- `EAGAIN` - Resource temporarily unavailable (non-blocking)
- `EHOSTUNREACH` - Remote host unreachable
- `EMSGSIZE` - Message too large for buffer
- `EPROTO` - Protocol error (invalid message format)

### Error Handling Example

```c
int rc = slk_spot_publish(spot, "topic", data, len);
if (rc != 0) {
    int err = slk_errno();
    switch (err) {
    case SLK_ENOENT:
        fprintf(stderr, "Topic not found\n");
        break;
    case SLK_EAGAIN:
        fprintf(stderr, "Send buffer full, retry later\n");
        break;
    default:
        fprintf(stderr, "Error: %s\n", slk_strerror(err));
    }
}
```

---

## Thread Safety

**SPOT Instance:**
- Thread-safe for all operations
- Uses internal `std::shared_mutex` for read/write locking
- Multiple threads can call `slk_spot_recv()` concurrently
- Publish operations are serialized per topic

**Best Practices:**
- Use one SPOT instance per thread for best performance
- Share SPOT instance across threads if necessary (thread-safe)
- Avoid destroying SPOT while other threads are using it

---

## Performance Considerations

**LOCAL Topics:**
- Zero-copy inproc transport
- Nanosecond latency
- Limited only by HWM

**REMOTE Topics:**
- TCP network overhead
- Automatic batching for small messages
- Persistent connections with reconnection

**Optimization Tips:**
- Use LOCAL topics for same-process communication
- Batch multiple publishes when possible
- Increase HWM for high-throughput scenarios
- Use pattern subscriptions sparingly (CPU overhead)

---

## See Also

- [Quick Start Guide](QUICK_START.md)
- [Architecture Overview](ARCHITECTURE.md)
- [Protocol Specification](PROTOCOL.md)
- [Clustering Guide](CLUSTERING.md)

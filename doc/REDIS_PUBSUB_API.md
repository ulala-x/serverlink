# ServerLink Redis-Style Pub/Sub API Reference

> Version: 1.0
> Last Updated: 2026-01-02

---

## Table of Contents

1. [Overview](#1-overview)
2. [Pattern Subscription API](#2-pattern-subscription-api)
3. [Introspection API](#3-introspection-api)
4. [Sharded Pub/Sub API](#4-sharded-pubsub-api)
5. [Broker API](#5-broker-api)
6. [Cluster API](#6-cluster-api)
7. [Error Codes](#7-error-codes)
8. [Thread Safety](#8-thread-safety)
9. [Performance Considerations](#9-performance-considerations)
10. [Migration Guide](#10-migration-guide)

---

## 1. Overview

ServerLink provides a Redis-compatible Pub/Sub API built on top of its native ZeroMQ-style PUB/SUB sockets. This high-level API offers:

- **Pattern-based subscriptions** using glob patterns (`*`, `?`, `[...]`)
- **Introspection** of active channels and subscriber counts
- **Sharded Pub/Sub** for horizontal scalability within a single process
- **Broker** for centralized message routing
- **Cluster** for distributed pub/sub across multiple servers

### Design Principles

| Principle | Description |
|-----------|-------------|
| **Type Safety** | Opaque struct pointers instead of `void*` |
| **Thread Safety** | All APIs are thread-safe with internal synchronization |
| **Zero-Copy** | Efficient message passing using ZeroMQ's zero-copy mechanism |
| **Backpressure** | Configurable HWM (High Water Mark) for flow control |

### Comparison with Redis

| Feature | Redis | ServerLink |
|---------|-------|------------|
| Pattern Subscription | `PSUBSCRIBE` | `SLK_PSUBSCRIBE` sockopt |
| Introspection | `PUBSUB CHANNELS` | `slk_pubsub_channels()` |
| Sharded Pub/Sub | `SPUBLISH` (Redis 7.0+) | `slk_spublish()` |
| Broker | N/A | `slk_pubsub_broker_*()` |
| Cluster | Redis Cluster | `slk_pubsub_cluster_*()` |

---

## 2. Pattern Subscription API

### 2.1 SLK_PSUBSCRIBE

Add a glob pattern subscription filter to a SUB socket.

```c
#define SLK_PSUBSCRIBE 81

int slk_setsockopt(slk_socket_t *sub, int option, const void *optval, size_t optvallen);
```

**Parameters:**
- `sub`: SUB socket to add pattern filter
- `option`: Must be `SLK_PSUBSCRIBE`
- `optval`: Null-terminated glob pattern string
- `optvallen`: Length of pattern string (excluding null terminator)

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Pattern Syntax:**

| Pattern | Description | Example |
|---------|-------------|---------|
| `*` | Matches any sequence of characters | `news.*` matches `news.sports`, `news.weather` |
| `?` | Matches exactly one character | `user.?` matches `user.1`, `user.a` |
| `[abc]` | Matches one character from set | `[abc]def` matches `adef`, `bdef`, `cdef` |
| `[a-z]` | Matches one character from range | `id.[0-9]` matches `id.0` to `id.9` |

**Example:**

```c
slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_connect(sub, "tcp://localhost:5555");

// Subscribe to all news channels
slk_setsockopt(sub, SLK_PSUBSCRIBE, "news.*", 6);

// Subscribe to user channels with single digit
slk_setsockopt(sub, SLK_PSUBSCRIBE, "user.?", 6);

// Receive messages
char channel[256], msg[1024];
slk_recv(sub, channel, sizeof(channel), 0);
slk_recv(sub, msg, sizeof(msg), 0);
```

**Thread Safety:** Thread-safe (can be called from multiple threads on different sockets)

**Notes:**
- Pattern subscriptions work alongside exact subscriptions (via `SLK_SUBSCRIBE`)
- If a message matches both an exact and pattern subscription, it will be delivered twice
- Patterns are compiled once and cached for efficiency

### 2.2 SLK_PUNSUBSCRIBE

Remove a glob pattern subscription filter.

```c
#define SLK_PUNSUBSCRIBE 82

int slk_setsockopt(slk_socket_t *sub, int option, const void *optval, size_t optvallen);
```

**Parameters:** Same as `SLK_PSUBSCRIBE`

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**

```c
// Remove pattern subscription
slk_setsockopt(sub, SLK_PUNSUBSCRIBE, "news.*", 6);
```

---

## 3. Introspection API

### 3.1 slk_pubsub_channels

Retrieve list of active channels matching a pattern.

```c
int slk_pubsub_channels(slk_ctx_t *ctx,
                        const char *pattern,
                        char ***channels,
                        size_t *count);
```

**Parameters:**
- `ctx`: ServerLink context
- `pattern`: Glob pattern to filter channels (or `NULL` for all channels)
- `channels`: Output pointer to array of channel name strings
- `count`: Output pointer to number of channels

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**

```c
char **channels;
size_t count;

// Get all active channels
if (slk_pubsub_channels(ctx, NULL, &channels, &count) == 0) {
    for (size_t i = 0; i < count; i++) {
        printf("Channel: %s\n", channels[i]);
    }
    slk_pubsub_channels_free(channels, count);
}

// Get channels matching pattern
if (slk_pubsub_channels(ctx, "news.*", &channels, &count) == 0) {
    printf("Found %zu news channels\n", count);
    slk_pubsub_channels_free(channels, count);
}
```

**Thread Safety:** Thread-safe (uses internal reader-writer lock)

**Memory Management:** Caller must free the result using `slk_pubsub_channels_free()`

### 3.2 slk_pubsub_channels_free

Free memory allocated by `slk_pubsub_channels()`.

```c
void slk_pubsub_channels_free(char **channels, size_t count);
```

**Parameters:**
- `channels`: Array returned by `slk_pubsub_channels()`
- `count`: Number of channels in array

**Example:** See `slk_pubsub_channels()` above.

### 3.3 slk_pubsub_numsub

Get subscriber count for specific channels.

```c
int slk_pubsub_numsub(slk_ctx_t *ctx,
                      const char **channels,
                      size_t count,
                      size_t *numsub);
```

**Parameters:**
- `ctx`: ServerLink context
- `channels`: Array of channel names to query
- `count`: Number of channels in array
- `numsub`: Output array of subscriber counts (must have space for `count` elements)

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**

```c
const char *channels[] = {"news.sports", "news.weather"};
size_t numsub[2];

if (slk_pubsub_numsub(ctx, channels, 2, numsub) == 0) {
    printf("news.sports: %zu subscribers\n", numsub[0]);
    printf("news.weather: %zu subscribers\n", numsub[1]);
}
```

**Thread Safety:** Thread-safe

**Notes:** Channels with no subscribers return `0`

### 3.4 slk_pubsub_numpat

Get total number of active pattern subscriptions.

```c
int slk_pubsub_numpat(slk_ctx_t *ctx);
```

**Parameters:**
- `ctx`: ServerLink context

**Returns:**
- Number of active pattern subscriptions (≥ 0)
- `-1` on error (sets errno)

**Example:**

```c
int numpat = slk_pubsub_numpat(ctx);
printf("Active pattern subscriptions: %d\n", numpat);
```

**Thread Safety:** Thread-safe

---

## 4. Sharded Pub/Sub API

Sharded Pub/Sub distributes channels across multiple internal shards for horizontal scalability within a single process. This is inspired by Redis 7.0+ Sharded Pub/Sub.

### 4.1 slk_sharded_pubsub_new

Create a sharded pub/sub context.

```c
typedef struct slk_sharded_pubsub_s slk_sharded_pubsub_t;

slk_sharded_pubsub_t* slk_sharded_pubsub_new(slk_ctx_t *ctx, int shard_count);
```

**Parameters:**
- `ctx`: ServerLink context
- `shard_count`: Number of shards (typically 8-64, must be power of 2 for optimal performance)

**Returns:**
- Pointer to sharded context on success
- `NULL` on error (sets errno)

**Example:**

```c
// Create 16 shards
slk_sharded_pubsub_t *shard_ctx = slk_sharded_pubsub_new(ctx, 16);
if (!shard_ctx) {
    perror("Failed to create sharded context");
    return -1;
}
```

**Thread Safety:** Thread-safe (the returned context can be used from multiple threads)

**Notes:**
- Higher shard count increases parallelism but also memory overhead
- Recommended: CPU core count × 2 for compute-bound workloads

### 4.2 slk_sharded_pubsub_destroy

Destroy a sharded pub/sub context.

```c
int slk_sharded_pubsub_destroy(slk_sharded_pubsub_t **shard_ctx);
```

**Parameters:**
- `shard_ctx`: Pointer to sharded context pointer (set to `NULL` after destruction)

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**

```c
slk_sharded_pubsub_destroy(&shard_ctx);
// shard_ctx is now NULL
```

**Thread Safety:** Must not be called concurrently with other operations on the same context

### 4.3 slk_spublish

Publish a message to a sharded channel.

```c
int slk_spublish(slk_sharded_pubsub_t *shard_ctx,
                 const char *channel,
                 const void *data,
                 size_t len);
```

**Parameters:**
- `shard_ctx`: Sharded pub/sub context
- `channel`: Channel name (supports hash tags `{tag}`)
- `data`: Message data
- `len`: Message length in bytes

**Returns:**
- Number of subscribers that received the message (≥ 0)
- `-1` on error (sets errno)

**Example:**

```c
// Simple publish
slk_spublish(shard_ctx, "room:lobby", "Hello!", 6);

// Hash tag ensures channels route to same shard
slk_spublish(shard_ctx, "{room:1}chat", "Message 1", 9);
slk_spublish(shard_ctx, "{room:1}events", "Event 1", 7);  // Same shard as above
```

**Thread Safety:** Thread-safe

**Hash Tags:** Content between `{}` determines shard assignment. Channels with the same hash tag go to the same shard.

### 4.4 slk_ssubscribe

Subscribe to a sharded channel.

```c
int slk_ssubscribe(slk_sharded_pubsub_t *shard_ctx,
                   slk_socket_t *sub,
                   const char *channel);
```

**Parameters:**
- `shard_ctx`: Sharded pub/sub context
- `sub`: SUB socket to receive messages
- `channel`: Channel name to subscribe

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**

```c
slk_socket_t *sub = slk_socket(ctx, SLK_SUB);

// Subscribe to sharded channels
slk_ssubscribe(shard_ctx, sub, "{room:1}chat");
slk_ssubscribe(shard_ctx, sub, "{room:1}events");

// Receive messages
char msg[1024];
while (slk_recv(sub, msg, sizeof(msg), 0) > 0) {
    printf("Received: %s\n", msg);
}
```

**Thread Safety:** Thread-safe (but same socket must not be used from multiple threads)

**Notes:** Pattern subscriptions (`SLK_PSUBSCRIBE`) are **not supported** in sharded pub/sub

### 4.5 slk_sunsubscribe

Unsubscribe from a sharded channel.

```c
int slk_sunsubscribe(slk_sharded_pubsub_t *shard_ctx,
                     slk_socket_t *sub,
                     const char *channel);
```

**Parameters:** Same as `slk_ssubscribe()`

**Returns:**
- `0` on success
- `-1` on error (sets errno)

### 4.6 slk_sharded_pubsub_set_hwm

Set high water mark for all shards.

```c
int slk_sharded_pubsub_set_hwm(slk_sharded_pubsub_t *shard_ctx, int hwm);
```

**Parameters:**
- `shard_ctx`: Sharded pub/sub context
- `hwm`: High water mark (message queue size per shard)

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**

```c
// Set HWM to 10,000 messages per shard
slk_sharded_pubsub_set_hwm(shard_ctx, 10000);
```

**Thread Safety:** Thread-safe

**Backpressure:** When HWM is reached, `slk_spublish()` will block (or return `EAGAIN` if `SLK_DONTWAIT` is set)

---

## 5. Broker API

The Broker provides a centralized message routing proxy, wrapping ServerLink's native XPUB/XSUB proxy pattern.

### 5.1 slk_pubsub_broker_new

Create a pub/sub broker.

```c
typedef struct slk_pubsub_broker_s slk_pubsub_broker_t;

slk_pubsub_broker_t* slk_pubsub_broker_new(slk_ctx_t *ctx,
                                            const char *frontend,
                                            const char *backend);
```

**Parameters:**
- `ctx`: ServerLink context
- `frontend`: Endpoint for publishers (e.g., `"tcp://*:5555"`)
- `backend`: Endpoint for subscribers (e.g., `"tcp://*:5556"`)

**Returns:**
- Pointer to broker on success
- `NULL` on error (sets errno)

**Example:**

```c
slk_pubsub_broker_t *broker = slk_pubsub_broker_new(ctx,
    "tcp://*:5555",  // Publishers connect here
    "tcp://*:5556"   // Subscribers connect here
);
```

**Thread Safety:** Thread-safe

**Architecture:**
```
Publishers → tcp://*:5555 → [XSUB ← Proxy → XPUB] → tcp://*:5556 → Subscribers
```

### 5.2 slk_pubsub_broker_destroy

Destroy a broker.

```c
int slk_pubsub_broker_destroy(slk_pubsub_broker_t **broker);
```

**Parameters:**
- `broker`: Pointer to broker pointer (set to `NULL` after destruction)

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Thread Safety:** Must stop broker first with `slk_pubsub_broker_stop()`

### 5.3 slk_pubsub_broker_run

Run broker in blocking mode (current thread).

```c
int slk_pubsub_broker_run(slk_pubsub_broker_t *broker);
```

**Parameters:**
- `broker`: Broker instance

**Returns:**
- `0` when stopped normally
- `-1` on error (sets errno)

**Example:**

```c
// Blocks until slk_pubsub_broker_stop() is called
slk_pubsub_broker_run(broker);
```

**Thread Safety:** Blocking - must call `slk_pubsub_broker_stop()` from another thread to terminate

### 5.4 slk_pubsub_broker_start

Start broker in background thread.

```c
int slk_pubsub_broker_start(slk_pubsub_broker_t *broker);
```

**Parameters:**
- `broker`: Broker instance

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**

```c
slk_pubsub_broker_start(broker);  // Returns immediately

// Broker now running in background...

slk_pubsub_broker_stop(broker);   // Stop when done
```

**Thread Safety:** Thread-safe

### 5.5 slk_pubsub_broker_stop

Stop a running broker.

```c
int slk_pubsub_broker_stop(slk_pubsub_broker_t *broker);
```

**Parameters:**
- `broker`: Broker instance

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Thread Safety:** Thread-safe (can be called from any thread)

### 5.6 slk_pubsub_broker_stats

Get broker statistics.

```c
int slk_pubsub_broker_stats(slk_pubsub_broker_t *broker,
                             size_t *messages_relayed);
```

**Parameters:**
- `broker`: Broker instance
- `messages_relayed`: Output pointer for total message count

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**

```c
size_t msg_count;
if (slk_pubsub_broker_stats(broker, &msg_count) == 0) {
    printf("Messages relayed: %zu\n", msg_count);
}
```

**Thread Safety:** Thread-safe

---

## 6. Cluster API

The Cluster API enables distributed pub/sub across multiple ServerLink instances.

### 6.1 slk_pubsub_cluster_new

Create a pub/sub cluster.

```c
typedef struct slk_pubsub_cluster_s slk_pubsub_cluster_t;

slk_pubsub_cluster_t* slk_pubsub_cluster_new(slk_ctx_t *ctx);
```

**Parameters:**
- `ctx`: ServerLink context

**Returns:**
- Pointer to cluster on success
- `NULL` on error (sets errno)

**Example:**

```c
slk_pubsub_cluster_t *cluster = slk_pubsub_cluster_new(ctx);
```

**Thread Safety:** Thread-safe

### 6.2 slk_pubsub_cluster_destroy

Destroy a cluster.

```c
void slk_pubsub_cluster_destroy(slk_pubsub_cluster_t **cluster);
```

**Parameters:**
- `cluster`: Pointer to cluster pointer (set to `NULL` after destruction)

**Thread Safety:** Must not be called concurrently with other cluster operations

### 6.3 slk_pubsub_cluster_add_node

Add a node to the cluster.

```c
int slk_pubsub_cluster_add_node(slk_pubsub_cluster_t *cluster,
                                 const char *endpoint);
```

**Parameters:**
- `cluster`: Cluster instance
- `endpoint`: Node endpoint (e.g., `"tcp://node1.example.com:5555"`)

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**

```c
slk_pubsub_cluster_add_node(cluster, "tcp://10.0.0.1:5555");
slk_pubsub_cluster_add_node(cluster, "tcp://10.0.0.2:5555");
slk_pubsub_cluster_add_node(cluster, "tcp://10.0.0.3:5555");
```

**Thread Safety:** Thread-safe

**Fault Tolerance:** If a node goes down, the cluster automatically reconnects with exponential backoff

### 6.4 slk_pubsub_cluster_remove_node

Remove a node from the cluster.

```c
int slk_pubsub_cluster_remove_node(slk_pubsub_cluster_t *cluster,
                                    const char *endpoint);
```

**Parameters:** Same as `slk_pubsub_cluster_add_node()`

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Thread Safety:** Thread-safe

### 6.5 slk_pubsub_cluster_publish

Publish a message to a cluster channel.

```c
int slk_pubsub_cluster_publish(slk_pubsub_cluster_t *cluster,
                                const char *channel,
                                const void *data,
                                size_t len);
```

**Parameters:**
- `cluster`: Cluster instance
- `channel`: Channel name (supports hash tags `{tag}`)
- `data`: Message data
- `len`: Message length

**Returns:**
- Number of nodes that received the message (≥ 0)
- `-1` on error (sets errno)

**Example:**

```c
slk_pubsub_cluster_publish(cluster, "global.events", "Server started", 14);
```

**Thread Safety:** Thread-safe

**Routing:** Channel is hashed to determine target node (similar to Redis Cluster)

### 6.6 slk_pubsub_cluster_subscribe

Subscribe to a cluster channel.

```c
int slk_pubsub_cluster_subscribe(slk_pubsub_cluster_t *cluster,
                                  const char *channel);
```

**Parameters:**
- `cluster`: Cluster instance
- `channel`: Channel name

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Thread Safety:** Thread-safe

### 6.7 slk_pubsub_cluster_psubscribe

Subscribe to cluster channels matching a pattern.

```c
int slk_pubsub_cluster_psubscribe(slk_pubsub_cluster_t *cluster,
                                   const char *pattern);
```

**Parameters:**
- `cluster`: Cluster instance
- `pattern`: Glob pattern

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**

```c
slk_pubsub_cluster_psubscribe(cluster, "events.*");
```

**Thread Safety:** Thread-safe

**Notes:** Pattern subscriptions are propagated to all cluster nodes

### 6.8 slk_pubsub_cluster_recv

Receive a message from the cluster.

```c
int slk_pubsub_cluster_recv(slk_pubsub_cluster_t *cluster,
                             char *channel, size_t *channel_len,
                             void *data, size_t *data_len,
                             int flags);
```

**Parameters:**
- `cluster`: Cluster instance
- `channel`: Buffer for channel name
- `channel_len`: In: buffer size, Out: actual channel name length
- `data`: Buffer for message data
- `data_len`: In: buffer size, Out: actual message length
- `flags`: `0` (blocking) or `SLK_DONTWAIT` (non-blocking)

**Returns:**
- `0` on success
- `-1` on error (sets errno to `EAGAIN` if non-blocking and no message available)

**Example:**

```c
char channel[256];
char data[4096];
size_t channel_len = sizeof(channel);
size_t data_len = sizeof(data);

while (1) {
    if (slk_pubsub_cluster_recv(cluster,
                                 channel, &channel_len,
                                 data, &data_len, 0) == 0) {
        printf("[%.*s] %.*s\n",
               (int)channel_len, channel,
               (int)data_len, data);
    }
    channel_len = sizeof(channel);
    data_len = sizeof(data);
}
```

**Thread Safety:** Thread-safe

### 6.9 slk_pubsub_cluster_nodes

Get list of cluster nodes.

```c
int slk_pubsub_cluster_nodes(slk_pubsub_cluster_t *cluster,
                              char ***nodes,
                              size_t *count);
```

**Parameters:**
- `cluster`: Cluster instance
- `nodes`: Output pointer to array of endpoint strings
- `count`: Output pointer to number of nodes

**Returns:**
- `0` on success
- `-1` on error (sets errno)

**Example:**

```c
char **nodes;
size_t count;

if (slk_pubsub_cluster_nodes(cluster, &nodes, &count) == 0) {
    for (size_t i = 0; i < count; i++) {
        printf("Node %zu: %s\n", i + 1, nodes[i]);
    }
    // Free nodes...
}
```

**Thread Safety:** Thread-safe

---

## 7. Error Codes

All functions return `-1` on error and set `errno` to one of the following:

| Error Code | Description |
|------------|-------------|
| `EINVAL` | Invalid argument (e.g., NULL pointer, invalid pattern) |
| `ENOMEM` | Out of memory |
| `ENOTSUP` | Operation not supported (e.g., pattern subscription on sharded pub/sub) |
| `ETERM` | Context was terminated |
| `EAGAIN` | Resource temporarily unavailable (HWM reached with `SLK_DONTWAIT`) |
| `ENOENT` | Channel or node not found |
| `ECONNREFUSED` | Connection refused (cluster node unreachable) |

---

## 8. Thread Safety

### 8.1 Thread Safety Guarantees

| Component | Thread Safety | Synchronization |
|-----------|---------------|-----------------|
| `slk_pubsub_channels()` | Thread-safe | Internal reader-writer lock |
| `slk_pubsub_numsub()` | Thread-safe | Internal reader-writer lock |
| `slk_pubsub_numpat()` | Thread-safe | Internal reader-writer lock |
| `slk_sharded_pubsub_*()` | Thread-safe | Per-shard locks (fine-grained) |
| `slk_pubsub_broker_*()` | Thread-safe | Internal thread isolation |
| `slk_pubsub_cluster_*()` | Thread-safe | Internal reader-writer lock + per-node locks |
| `SLK_PSUBSCRIBE` | Socket-safe | Not thread-safe per socket (ServerLink standard) |

### 8.2 Concurrent Usage Example

```c
// Thread 1: Publisher
void *publisher_thread(void *arg) {
    slk_sharded_pubsub_t *shard = arg;

    while (running) {
        slk_spublish(shard, "events", "data", 4);  // Thread-safe
    }
    return NULL;
}

// Thread 2: Subscriber
void *subscriber_thread(void *arg) {
    slk_sharded_pubsub_t *shard = arg;
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);

    slk_ssubscribe(shard, sub, "events");  // Thread-safe

    char buf[1024];
    while (slk_recv(sub, buf, sizeof(buf), 0) > 0) {
        // Process message
    }
    return NULL;
}

// Thread 3: Introspection
void *monitor_thread(void *arg) {
    slk_ctx_t *ctx = arg;

    while (running) {
        size_t numsub;
        const char *channels[] = {"events"};
        slk_pubsub_numsub(ctx, channels, 1, &numsub);  // Thread-safe
        printf("Subscribers: %zu\n", numsub);
        sleep(5);
    }
    return NULL;
}
```

---

## 9. Performance Considerations

### 9.1 Pattern Matching Performance

- Patterns are compiled once and cached
- Matching complexity: O(N) where N is pattern length
- Recommendation: Use exact subscriptions when possible

### 9.2 Sharded Pub/Sub Scalability

| Shard Count | Throughput | Latency | Memory |
|-------------|------------|---------|--------|
| 1 | Baseline | Baseline | Baseline |
| 8 | 6-7x | ~same | 2x |
| 16 | 12-14x | ~same | 3x |
| 32 | 20-25x | +5-10% | 5x |

**Recommendation:** Start with `shard_count = num_cpus * 2`

### 9.3 Memory Usage

| Component | Memory per Channel |
|-----------|-------------------|
| Registry Entry | ~128 bytes |
| Pattern Subscription | ~256 bytes |
| Shard | ~16 KB baseline + HWM × message_size |

### 9.4 Backpressure Tuning

```c
// High-throughput, low-latency
slk_sharded_pubsub_set_hwm(shard, 100000);  // Large queue

// Memory-constrained
slk_sharded_pubsub_set_hwm(shard, 1000);    // Small queue
```

---

## 10. Migration Guide

### 10.1 From Native PUB/SUB to Sharded

**Before:**
```c
slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
slk_bind(pub, "inproc://events");

slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_connect(sub, "inproc://events");
slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);

slk_send(pub, "message", 7, 0);
```

**After:**
```c
slk_sharded_pubsub_t *shard = slk_sharded_pubsub_new(ctx, 16);

slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_ssubscribe(shard, sub, "events");

slk_spublish(shard, "events", "message", 7);
```

### 10.2 From Redis PSUBSCRIBE to ServerLink

**Redis:**
```python
pubsub = redis.pubsub()
pubsub.psubscribe('news.*')
for msg in pubsub.listen():
    print(msg['data'])
```

**ServerLink:**
```c
slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_connect(sub, "tcp://localhost:5555");
slk_setsockopt(sub, SLK_PSUBSCRIBE, "news.*", 6);

char buf[1024];
while (slk_recv(sub, buf, sizeof(buf), 0) > 0) {
    printf("%s\n", buf);
}
```

---

## Appendix: Complete Example

```c
#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>

int main() {
    // Create context
    slk_ctx_t *ctx = slk_init();

    // Create sharded pub/sub
    slk_sharded_pubsub_t *shard = slk_sharded_pubsub_new(ctx, 16);
    slk_sharded_pubsub_set_hwm(shard, 10000);

    // Create subscriber
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    slk_ssubscribe(shard, sub, "events.*");

    // Pattern subscription on regular SUB socket
    slk_socket_t *pattern_sub = slk_socket(ctx, SLK_SUB);
    slk_connect(pattern_sub, "tcp://localhost:5555");
    slk_setsockopt(pattern_sub, SLK_PSUBSCRIBE, "alerts.*", 8);

    // Publish messages
    slk_spublish(shard, "events.login", "user123", 7);
    slk_spublish(shard, "events.logout", "user456", 7);

    // Introspection
    char **channels;
    size_t count;
    if (slk_pubsub_channels(ctx, "events.*", &channels, &count) == 0) {
        printf("Found %zu event channels\n", count);
        slk_pubsub_channels_free(channels, count);
    }

    // Cleanup
    slk_sharded_pubsub_destroy(&shard);
    slk_close(sub);
    slk_close(pattern_sub);
    slk_term(ctx);

    return 0;
}
```

---

**End of API Reference**

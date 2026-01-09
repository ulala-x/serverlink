[![English](https://img.shields.io/badge/lang:en-red.svg)](MIGRATION.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](MIGRATION.ko.md)

# Migration from Traditional PUB/SUB to SPOT

Guide for migrating from ServerLink's traditional PUB/SUB sockets to SPOT.

## Table of Contents

1. [Overview](#overview)
2. [API Mapping Table](#api-mapping-table)
3. [Step-by-Step Migration](#step-by-step-migration)
4. [Migration Considerations](#migration-considerations)
5. [Compatibility Notes](#compatibility-notes)
6. [Example Migrations](#example-migrations)

---

## Overview

**Traditional PUB/SUB:**
- Direct XPUB/XSUB socket usage
- Manual endpoint management
- Explicit bind/connect operations
- No built-in cluster support

**SPOT:**
- Higher-level abstraction over XPUB/XSUB
- Automatic topic registry
- Location-transparent routing
- Built-in cluster synchronization

**Migration Benefits:**
- Simpler API surface
- Automatic topic discovery
- Better scalability (cluster support)
- Transparent LOCAL/REMOTE routing

**Migration Challenges:**
- Different API semantics
- Pattern subscriptions (LOCAL only)
- No backward compatibility with raw PUB/SUB

---

## API Mapping Table

### Socket Creation

| Traditional PUB/SUB | SPOT Equivalent | Notes |
|---------------------|-----------------|-------|
| `slk_socket(ctx, SLK_XPUB)` | `slk_spot_new(ctx)` + `slk_spot_topic_create()` | SPOT creates XPUB internally |
| `slk_socket(ctx, SLK_XSUB)` | `slk_spot_new(ctx)` | SPOT creates XSUB internally |
| `slk_bind(xpub, "inproc://topic")` | `slk_spot_topic_create(spot, "topic")` | SPOT generates inproc endpoint |
| `slk_connect(xsub, "inproc://topic")` | `slk_spot_subscribe(spot, "topic")` | SPOT connects automatically |

### Publishing

| Traditional PUB/SUB | SPOT Equivalent | Notes |
|---------------------|-----------------|-------|
| `slk_send(xpub, data, len, 0)` | `slk_spot_publish(spot, topic, data, len)` | SPOT adds topic frame |
| Multi-frame: `[topic][data]` | Same format used internally | SPOT handles framing |

### Subscribing

| Traditional PUB/SUB | SPOT Equivalent | Notes |
|---------------------|-----------------|-------|
| `slk_setsockopt(xsub, SLK_SUBSCRIBE, topic, len)` | `slk_spot_subscribe(spot, topic)` | SPOT manages subscription |
| `slk_setsockopt(xsub, SLK_UNSUBSCRIBE, topic, len)` | `slk_spot_unsubscribe(spot, topic)` | SPOT removes subscription |
| `slk_setsockopt(xsub, SLK_PSUBSCRIBE, pattern, len)` | `slk_spot_subscribe_pattern(spot, pattern)` | LOCAL topics only |

### Receiving

| Traditional PUB/SUB | SPOT Equivalent | Notes |
|---------------------|-----------------|-------|
| `slk_recv(xsub, buf, len, flags)` (Frame 1) | `slk_spot_recv(spot, topic, ...)` | SPOT separates topic/data |
| `slk_recv(xsub, buf, len, flags)` (Frame 2) | Same call | Single API for both frames |

### Socket Options

| Traditional PUB/SUB | SPOT Equivalent | Notes |
|---------------------|-----------------|-------|
| `slk_setsockopt(socket, SLK_SNDHWM, ...)` | `slk_spot_set_hwm(spot, sndhwm, rcvhwm)` | Sets for all sockets |
| `slk_setsockopt(socket, SLK_RCVHWM, ...)` | Same | |
| `slk_getsockopt(socket, SLK_FD, ...)` | `slk_spot_fd(spot, &fd)` | Returns XSUB FD |

### Cleanup

| Traditional PUB/SUB | SPOT Equivalent | Notes |
|---------------------|-----------------|-------|
| `slk_close(xpub)` | `slk_spot_topic_destroy(spot, topic)` | Destroys topic's XPUB |
| `slk_close(xsub)` | `slk_spot_destroy(&spot)` | Destroys all sockets |

---

## Step-by-Step Migration

### Step 1: Identify PUB/SUB Usage

**Find all PUB/SUB socket creation:**
```bash
grep -r "SLK_XPUB\|SLK_XSUB\|SLK_PUB\|SLK_SUB" src/
```

**Catalog topics:**
- List all topics being published/subscribed
- Identify topic naming patterns
- Note multi-frame message usage

### Step 2: Create Topic Mapping

**Before (Traditional):**
```c
// Publisher
slk_socket_t *xpub1 = slk_socket(ctx, SLK_XPUB);
slk_bind(xpub1, "inproc://game:player");

slk_socket_t *xpub2 = slk_socket(ctx, SLK_XPUB);
slk_bind(xpub2, "inproc://game:score");

// Subscriber
slk_socket_t *xsub = slk_socket(ctx, SLK_XSUB);
slk_connect(xsub, "inproc://game:player");
slk_connect(xsub, "inproc://game:score");
```

**After (SPOT):**
```c
// Single SPOT instance
slk_spot_t *spot = slk_spot_new(ctx);

// Create topics
slk_spot_topic_create(spot, "game:player");
slk_spot_topic_create(spot, "game:score");

// Subscribe
slk_spot_subscribe(spot, "game:player");
slk_spot_subscribe(spot, "game:score");
```

### Step 3: Update Publish Code

**Before:**
```c
// Multi-frame publish
msg_t topic_msg, data_msg;
msg_init_buffer(&topic_msg, "game:player", 11);
msg_init_buffer(&data_msg, "Player1", 7);

slk_msg_send(&topic_msg, xpub, SLK_SNDMORE);
slk_msg_send(&data_msg, xpub, 0);

msg_close(&topic_msg);
msg_close(&data_msg);
```

**After:**
```c
// Single call
slk_spot_publish(spot, "game:player", "Player1", 7);
```

### Step 4: Update Subscribe Code

**Before:**
```c
// Set subscription option
const char *topic = "game:player";
slk_setsockopt(xsub, SLK_SUBSCRIBE, topic, strlen(topic));
```

**After:**
```c
// Subscribe API
slk_spot_subscribe(spot, "game:player");
```

### Step 5: Update Receive Code

**Before:**
```c
// Receive multi-frame message
msg_t topic_msg, data_msg;

msg_init(&topic_msg);
slk_msg_recv(&topic_msg, xsub, 0);

msg_init(&data_msg);
slk_msg_recv(&data_msg, xsub, 0);

// Extract topic and data
const char *topic = (const char *)msg_data(&topic_msg);
size_t topic_len = msg_size(&topic_msg);

const char *data = (const char *)msg_data(&data_msg);
size_t data_len = msg_size(&data_msg);

msg_close(&topic_msg);
msg_close(&data_msg);
```

**After:**
```c
// Single call with separated buffers
char topic[256], data[4096];
size_t topic_len, data_len;

slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, 0);
```

### Step 6: Update Cleanup Code

**Before:**
```c
slk_close(xpub1);
slk_close(xpub2);
slk_close(xsub);
```

**After:**
```c
slk_spot_destroy(&spot); // Closes all sockets
```

### Step 7: Test

**Unit Tests:**
```c
void test_spot_migration()
{
    slk_ctx_t *ctx = slk_ctx_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    // Create and subscribe
    slk_spot_topic_create(spot, "test:topic");
    slk_spot_subscribe(spot, "test:topic");

    // Publish
    slk_spot_publish(spot, "test:topic", "hello", 5);
    slk_sleep(10);

    // Receive
    char topic[256], data[256];
    size_t topic_len, data_len;
    int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, SLK_DONTWAIT);

    assert(rc == 0);
    assert(memcmp(data, "hello", 5) == 0);

    slk_spot_destroy(&spot);
    slk_ctx_destroy(ctx);
}
```

---

## Migration Considerations

### Pattern Subscriptions

**Traditional PUB/SUB:**
- Pattern subscriptions work for both inproc and TCP

**SPOT:**
- Pattern subscriptions **LOCAL only**
- REMOTE topics require explicit subscription

**Migration:**
```c
// Before: Pattern subscription over TCP
slk_setsockopt(xsub, SLK_PSUBSCRIBE, "game:*", 6);

// After: SPOT pattern subscription (LOCAL only)
slk_spot_subscribe_pattern(spot, "game:*");

// For REMOTE topics, use explicit subscriptions
slk_spot_subscribe(spot, "game:player");
slk_spot_subscribe(spot, "game:score");
```

### Multi-Frame Messages

**Traditional PUB/SUB:**
- Direct control over frame boundaries
- Can send arbitrary multi-frame messages

**SPOT:**
- Fixed two-frame format: `[topic][data]`
- No support for additional frames

**Migration:**
If you need additional frames, encode in data:
```c
// Before: [topic][metadata][data]
slk_send(xpub, "topic", 5, SLK_SNDMORE);
slk_send(xpub, "metadata", 8, SLK_SNDMORE);
slk_send(xpub, "data", 4, 0);

// After: Encode metadata in data
typedef struct {
    char metadata[8];
    char data[4];
} message_t;

message_t msg;
memcpy(msg.metadata, "metadata", 8);
memcpy(msg.data, "data", 4);

slk_spot_publish(spot, "topic", &msg, sizeof(msg));
```

### Socket Options

**Traditional PUB/SUB:**
- Fine-grained control per socket

**SPOT:**
- Global HWM setting for all topics

**Migration:**
```c
// Before: Per-socket HWM
int hwm = 10000;
slk_setsockopt(xpub1, SLK_SNDHWM, &hwm, sizeof(hwm));
slk_setsockopt(xpub2, SLK_SNDHWM, &hwm, sizeof(hwm));

// After: Global HWM
slk_spot_set_hwm(spot, 10000, 10000);
```

If you need per-topic HWM, use separate SPOT instances.

### TCP Transport

**Traditional PUB/SUB:**
```c
// Publisher
slk_socket_t *xpub = slk_socket(ctx, SLK_XPUB);
slk_bind(xpub, "tcp://*:5555");

// Subscriber
slk_socket_t *xsub = slk_socket(ctx, SLK_XSUB);
slk_connect(xsub, "tcp://server:5555");
```

**SPOT:**
```c
// Publisher
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "topic");
slk_spot_bind(spot, "tcp://*:5555");

// Subscriber
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_route(spot, "topic", "tcp://server:5555");
slk_spot_subscribe(spot, "topic");
```

**Or use cluster sync:**
```c
// Subscriber (automatic discovery)
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_cluster_add(spot, "tcp://server:5555");
slk_spot_cluster_sync(spot, 1000);
slk_spot_subscribe(spot, "topic");
```

---

## Compatibility Notes

### Cannot Mix SPOT and Traditional PUB/SUB

**SPOT uses ROUTER for cluster protocol**, which is incompatible with raw XPUB/XSUB connections.

**Incompatible:**
```c
// Traditional XSUB
slk_socket_t *xsub = slk_socket(ctx, SLK_XSUB);
slk_connect(xsub, "tcp://spot-node:5555"); // Won't work!
```

**Workaround:**
- Migrate all nodes to SPOT
- Or use separate endpoints for SPOT and traditional PUB/SUB

### Subscription Visibility

**Traditional XPUB:**
- Subscription messages visible via `SLK_XPUB_VERBOSE`

**SPOT:**
- Subscription tracking internal to subscription_manager_t
- No external visibility (future enhancement)

### Message Ordering

**Both:**
- Maintain FIFO order per topic
- No ordering guarantees across topics

---

## Example Migrations

### Example 1: Simple Pub/Sub

**Before:**
```c
// Publisher
slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
slk_bind(pub, "inproc://events");
slk_send(pub, "event1", 6, 0);

// Subscriber
slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_connect(sub, "inproc://events");
const char *filter = "";
slk_setsockopt(sub, SLK_SUBSCRIBE, filter, 0);
char buf[256];
slk_recv(sub, buf, sizeof(buf), 0);
```

**After:**
```c
// Combined pub/sub
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "events");
slk_spot_subscribe(spot, "events");
slk_spot_publish(spot, "events", "event1", 6);

slk_sleep(10);

char topic[256], data[256];
size_t topic_len, data_len;
slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, SLK_DONTWAIT);
```

### Example 2: Multiple Publishers

**Before:**
```c
// Publisher 1
slk_socket_t *pub1 = slk_socket(ctx, SLK_XPUB);
slk_bind(pub1, "inproc://topic1");

// Publisher 2
slk_socket_t *pub2 = slk_socket(ctx, SLK_XPUB);
slk_bind(pub2, "inproc://topic2");

// Subscriber
slk_socket_t *sub = slk_socket(ctx, SLK_XSUB);
slk_connect(sub, "inproc://topic1");
slk_connect(sub, "inproc://topic2");
```

**After:**
```c
// Single SPOT instance
slk_spot_t *spot = slk_spot_new(ctx);

// Create both topics
slk_spot_topic_create(spot, "topic1");
slk_spot_topic_create(spot, "topic2");

// Subscribe to both
slk_spot_subscribe(spot, "topic1");
slk_spot_subscribe(spot, "topic2");
```

### Example 3: Pattern Subscription

**Before:**
```c
slk_socket_t *xsub = slk_socket(ctx, SLK_XSUB);
slk_connect(xsub, "inproc://game");

const char *pattern = "player:";
slk_setsockopt(xsub, SLK_PSUBSCRIBE, pattern, strlen(pattern));
```

**After:**
```c
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_subscribe_pattern(spot, "player:*");
```

**Note:** Pattern matching syntax differs:
- Traditional: Prefix match ("player:" matches "player:123")
- SPOT: Wildcard match ("player:*" matches "player:123")

### Example 4: TCP Pub/Sub

**Before:**
```c
// Server
slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
slk_bind(pub, "tcp://*:5555");
slk_send(pub, "data", 4, 0);

// Client
slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_connect(sub, "tcp://server:5555");
slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
char buf[256];
slk_recv(sub, buf, sizeof(buf), 0);
```

**After:**
```c
// Server
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "data");
slk_spot_bind(spot, "tcp://*:5555");
slk_spot_publish(spot, "data", "data", 4);

// Client
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_route(spot, "data", "tcp://server:5555");
slk_spot_subscribe(spot, "data");

char topic[256], data[256];
size_t topic_len, data_len;
slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, 0);
```

---

## Testing Migration

### Parallel Run Strategy

Run traditional and SPOT systems in parallel:

**Dual Publishing:**
```c
// Publish to both traditional and SPOT
slk_send(xpub, data, len, 0);
slk_spot_publish(spot, topic, data, len);

// Compare results
```

**Shadow Receiving:**
```c
// Receive from both and compare
char traditional_data[256];
slk_recv(xsub, traditional_data, sizeof(traditional_data), 0);

char spot_topic[256], spot_data[256];
size_t topic_len, data_len;
slk_spot_recv(spot, spot_topic, sizeof(spot_topic), &topic_len,
              spot_data, sizeof(spot_data), &data_len, 0);

// Assert equality
assert(memcmp(traditional_data, spot_data, data_len) == 0);
```

### Gradual Migration

Migrate one topic at a time:

1. **Phase 1**: Migrate publisher (dual-publish)
2. **Phase 2**: Migrate subscribers (dual-subscribe)
3. **Phase 3**: Remove traditional code

---

## Performance Comparison

| Metric | Traditional PUB/SUB | SPOT | Notes |
|--------|---------------------|------|-------|
| Latency (inproc) | 0.01-0.1 µs | 0.01-0.1 µs | Equivalent |
| Latency (TCP) | 10-50 µs | 10-50 µs | Equivalent |
| Throughput (inproc) | 10M msg/s | 10M msg/s | Equivalent |
| Throughput (TCP) | 1M msg/s | 1M msg/s | Equivalent |
| Memory overhead | Low | Medium | SPOT has registry |
| API complexity | High | Low | SPOT simplifies |

---

## See Also

- [API Reference](API.md)
- [Quick Start Guide](QUICK_START.md)
- [Architecture Overview](ARCHITECTURE.md)
- [Usage Patterns](PATTERNS.md)

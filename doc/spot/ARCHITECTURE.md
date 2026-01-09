[![English](https://img.shields.io/badge/lang:en-red.svg)](ARCHITECTURE.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](ARCHITECTURE.ko.md)

# SPOT PUB/SUB Architecture

Internal architecture and design of ServerLink SPOT (Scalable Partitioned Ordered Topics).

## Table of Contents

1. [Overview](#overview)
2. [Architecture Evolution](#architecture-evolution)
3. [Component Architecture](#component-architecture)
4. [Class Diagrams](#class-diagrams)
5. [Data Flow](#data-flow)
6. [Threading Model](#threading-model)
7. [Memory Management](#memory-management)
8. [Performance Characteristics](#performance-characteristics)
9. [Design Decisions](#design-decisions)

---

## Overview

SPOT provides location-transparent pub/sub using a simplified **direct XPUB/XSUB** architecture:

- **One shared XPUB** socket per SPOT instance (publishes all topics)
- **One shared XSUB** socket per SPOT instance (receives from all connected publishers)
- **Topic Registry** for topic metadata and routing
- **Subscription Manager** for subscription tracking

**Key Design Principles:**
1. **Simplicity** - Direct XPUB/XSUB connections without intermediate routing
2. **Zero-copy LOCAL topics** using inproc transport
3. **Transparent remote topics** via TCP connections
4. **Thread-safe** operations with read/write locking

---

## Component Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                          slk_spot_t                                  │
│                      (SPOT PUB/SUB Instance)                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌────────────────────┐    ┌────────────────────┐                  │
│  │  topic_registry_t  │    │ subscription_      │                  │
│  │                    │    │ manager_t          │                  │
│  ├────────────────────┤    ├────────────────────┤                  │
│  │ - LOCAL topics     │    │ - Topic subs       │                  │
│  │ - REMOTE topics    │    │ - Pattern subs     │                  │
│  │ - Endpoint mapping │    │ - Subscriber types │                  │
│  └────────────────────┘    └────────────────────┘                  │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                     Socket Layer                              │  │
│  ├──────────────────────────────────────────────────────────────┤  │
│  │                                                               │  │
│  │   _pub_socket (XPUB)         _recv_socket (XSUB)             │  │
│  │   ┌─────────────────┐        ┌─────────────────┐             │  │
│  │   │ Bound to:       │        │ Connected to:   │             │  │
│  │   │ - inproc://spot-N│       │ - Local XPUB    │             │  │
│  │   │ - tcp://*:port  │        │ - Remote XPUBs  │             │  │
│  │   └─────────────────┘        └─────────────────┘             │  │
│  │          │                          │                         │  │
│  │          ▼                          ▼                         │  │
│  │   publish() sends here       recv() reads here               │  │
│  │                                                               │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                     Endpoint Tracking                         │  │
│  ├──────────────────────────────────────────────────────────────┤  │
│  │ _inproc_endpoint: "inproc://spot-0"  (always set)            │  │
│  │ _bind_endpoint: "tcp://...:5555"      (after bind())         │  │
│  │ _bind_endpoints: set of all bound endpoints                  │  │
│  │ _connected_endpoints: set of cluster connections             │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Class Diagrams

### Core Classes

```
┌───────────────────────────────────────┐
│           topic_registry_t            │
├───────────────────────────────────────┤
│ - _topics: map<string, topic_entry_t> │
│ - _mutex: shared_mutex                │
├───────────────────────────────────────┤
│ + register_local(topic_id)            │
│ + register_local(topic_id, endpoint)  │
│ + register_remote(topic_id, endpoint) │
│ + unregister(topic_id)                │
│ + lookup(topic_id) → optional<entry>  │
│ + has_topic(topic_id) → bool          │
│ + get_local_topics() → vector         │
│ + get_all_topics() → vector           │
└───────────────────────────────────────┘

┌───────────────────────────────────────┐
│           topic_entry_t               │
├───────────────────────────────────────┤
│ + location: LOCAL | REMOTE            │
│ + endpoint: string                    │
│ + created_at: timestamp               │
└───────────────────────────────────────┘

┌───────────────────────────────────────┐
│       subscription_manager_t          │
├───────────────────────────────────────┤
│ - _subscriptions: map<string, set>    │
│ - _patterns: map<string, set>         │
│ - _mutex: shared_mutex                │
├───────────────────────────────────────┤
│ + add_subscription(topic, sub)        │
│ + add_pattern_subscription(pattern)   │
│ + remove_subscription(topic, sub)     │
│ + is_subscribed(topic, sub) → bool    │
│ + match_pattern(topic) → bool         │
└───────────────────────────────────────┘

┌───────────────────────────────────────┐
│           spot_pubsub_t               │
├───────────────────────────────────────┤
│ - _ctx: ctx_t*                        │
│ - _registry: unique_ptr<registry>     │
│ - _sub_manager: unique_ptr<sub_mgr>   │
│ - _pub_socket: socket_base_t* (XPUB)  │
│ - _recv_socket: socket_base_t* (XSUB) │
│ - _inproc_endpoint: string            │
│ - _bind_endpoint: string              │
│ - _bind_endpoints: set<string>        │
│ - _connected_endpoints: set<string>   │
│ - _sndhwm, _rcvhwm: int               │
│ - _rcvtimeo: int                      │
│ - _mutex: shared_mutex                │
├───────────────────────────────────────┤
│ + topic_create(topic_id)              │
│ + topic_destroy(topic_id)             │
│ + topic_route(topic_id, endpoint)     │
│ + subscribe(topic_id)                 │
│ + subscribe_pattern(pattern)          │
│ + unsubscribe(topic_id)               │
│ + publish(topic_id, data, len)        │
│ + recv(topic, data, flags)            │
│ + bind(endpoint)                      │
│ + cluster_add(endpoint)               │
│ + cluster_remove(endpoint)            │
│ + cluster_sync(timeout)               │
│ + list_topics() → vector              │
│ + topic_exists(topic_id) → bool       │
│ + topic_is_local(topic_id) → bool     │
│ + set_hwm(sndhwm, rcvhwm)             │
│ + setsockopt(option, value, len)      │
│ + getsockopt(option, value, len)      │
│ + fd() → int                          │
└───────────────────────────────────────┘
```

---

## Data Flow

### LOCAL Topic Publish/Subscribe (Same SPOT Instance)

```
┌──────────────────────────────────────────────────────────────────┐
│                         SPOT Instance                             │
│                                                                   │
│   Publisher Thread              Subscriber Thread                 │
│         │                             │                           │
│         │ slk_spot_publish()          │                           │
│         │                             │                           │
│         ▼                             │                           │
│   ┌───────────┐                       │                           │
│   │   XPUB    │◄── bound to inproc    │                           │
│   └─────┬─────┘                       │                           │
│         │                             │                           │
│         │ [topic][data]               │                           │
│         │                             │                           │
│         └──────────── inproc ─────────┤                           │
│                                       ▼                           │
│                                 ┌───────────┐                     │
│                                 │   XSUB    │◄── connected to XPUB│
│                                 └─────┬─────┘                     │
│                                       │                           │
│                                       │ slk_spot_recv()           │
│                                       ▼                           │
│                              Return [topic][data]                 │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘

Performance:
- Zero-copy via inproc (ypipe)
- Sub-microsecond latency
- No serialization overhead
```

### REMOTE Topic Publish/Subscribe (Cross-Node)

```
┌────────────────────────┐              ┌────────────────────────┐
│        Node A          │              │        Node B          │
│                        │              │                        │
│  ┌─────────────────┐   │              │   ┌─────────────────┐  │
│  │ slk_spot_publish│   │              │   │                 │  │
│  └────────┬────────┘   │              │   │  slk_spot_recv  │  │
│           │            │              │   │                 │  │
│           ▼            │              │   └────────▲────────┘  │
│      ┌─────────┐       │              │            │           │
│      │  XPUB   │       │              │       ┌────┴────┐      │
│      │ (bound) │       │              │       │  XSUB   │      │
│      └────┬────┘       │              │       │(connect)│      │
│           │            │              │       └────┬────┘      │
│           │ TCP        │              │            │           │
│           └────────────┼──────────────┼────────────┘           │
│                        │  [topic]     │                        │
│                        │  [data]      │                        │
│                        │              │                        │
└────────────────────────┘              └────────────────────────┘

Setup:
1. Node A: bind("tcp://*:5555")
2. Node B: cluster_add("tcp://nodeA:5555")
3. Node B: subscribe("topic")  → XSUB sends subscription to XPUB
4. Node A: publish("topic", data) → XPUB forwards to matching XSUBs

Performance:
- TCP network overhead (~10-100 µs)
- Persistent connections with auto-reconnection
- ZeroMQ handles subscription filtering
```

### Multi-Publisher Scenario

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Publisher A   │    │   Publisher B   │    │   Subscriber    │
│                 │    │                 │    │                 │
│ ┌─────────────┐ │    │ ┌─────────────┐ │    │ ┌─────────────┐ │
│ │    XPUB     │ │    │ │    XPUB     │ │    │ │    XSUB     │ │
│ │ tcp://:5555 │ │    │ │ tcp://:5556 │ │    │ │ (connects)  │ │
│ └──────┬──────┘ │    │ └──────┬──────┘ │    │ └──────┬──────┘ │
│        │        │    │        │        │    │        │        │
└────────┼────────┘    └────────┼────────┘    └────────┼────────┘
         │                      │                      │
         │     TCP              │     TCP              │
         └──────────────────────┴──────────────────────┘
                                │
                   ┌────────────┴────────────┐
                   │  Subscriber receives    │
                   │  from BOTH publishers   │
                   └─────────────────────────┘

Setup:
1. Publisher A: bind("tcp://*:5555")
2. Publisher B: bind("tcp://*:5556")
3. Subscriber: cluster_add("tcp://pubA:5555")
4. Subscriber: cluster_add("tcp://pubB:5556")
5. Subscriber: subscribe("topic")
```

### Pattern Subscription

```
Subscriber                                    Publisher
    │                                            │
    │ subscribe_pattern("events:*")              │
    │                                            │
    │ ┌─────────────────────────────────────┐   │
    │ │ Convert to XPUB prefix:             │   │
    │ │ "events:*" → "events:"              │   │
    │ │                                     │   │
    │ │ Send subscription message:          │   │
    │ │ [0x01]["events:"]                   │   │
    │ └─────────────────────────────────────┘   │
    │                                            │
    │           subscription message             │
    │ ─────────────────────────────────────────► │
    │                                            │
    │                      publish("events:login", data)
    │                      publish("events:logout", data)
    │                      publish("metrics:cpu", data)
    │                                            │
    │ ◄──────── "events:login" matches ──────── │
    │ ◄──────── "events:logout" matches ─────── │
    │           "metrics:cpu" filtered out       │
    │                                            │

Note: XPUB uses prefix matching, not glob patterns.
"events:*" pattern is converted to "events:" prefix.
```

---

## Threading Model

### Concurrency Control

```cpp
// All public methods use read/write locking
class spot_pubsub_t {
    mutable std::shared_mutex _mutex;

    // Read operations (multiple concurrent readers)
    bool topic_exists(const std::string& topic_id) const {
        std::shared_lock<std::shared_mutex> lock(_mutex);
        return _registry->has_topic(topic_id);
    }

    // Write operations (exclusive access)
    int topic_create(const std::string& topic_id) {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        return _registry->register_local(topic_id, endpoint);
    }
};
```

### Thread Safety Guarantees

| Operation | Thread Safety | Lock Type |
|-----------|---------------|-----------|
| topic_create | Safe | Exclusive |
| topic_destroy | Safe | Exclusive |
| subscribe | Safe | Exclusive |
| unsubscribe | Safe | Exclusive |
| publish | Safe | Shared |
| recv | Safe | Shared |
| list_topics | Safe | Shared |
| cluster_add | Safe | Exclusive |
| cluster_remove | Safe | Exclusive |

### Best Practices

```c
// Good: Shared SPOT instance with internal locking
slk_spot_t *spot = slk_spot_new(ctx);

// Thread 1: Publisher
void publisher_thread() {
    while (running) {
        slk_spot_publish(spot, "topic", data, len);  // Thread-safe
    }
}

// Thread 2: Subscriber
void subscriber_thread() {
    while (running) {
        slk_spot_recv(spot, topic, &topic_len, data, &data_len, 0);  // Thread-safe
    }
}

// Good: Separate SPOT instances per thread (no contention)
void worker_thread() {
    slk_spot_t *local_spot = slk_spot_new(ctx);
    // Use exclusively in this thread
}
```

---

## Memory Management

### Object Lifetime

```cpp
// Constructor: Creates shared XPUB/XSUB sockets
spot_pubsub_t::spot_pubsub_t(ctx_t *ctx_)
{
    // 1. Generate unique inproc endpoint
    _inproc_endpoint = "inproc://" + generate_instance_id();

    // 2. Create XPUB (for publishing)
    _pub_socket = _ctx->create_socket(SL_XPUB);
    _pub_socket->bind(_inproc_endpoint.c_str());

    // 3. Create XSUB (for receiving)
    _recv_socket = _ctx->create_socket(SL_XSUB);
    _recv_socket->connect(_inproc_endpoint.c_str());
}

// Destructor: Proper cleanup order
spot_pubsub_t::~spot_pubsub_t()
{
    // 1. Close receive socket first
    if (_recv_socket) {
        _recv_socket->close();
        _recv_socket = nullptr;
    }

    // 2. Close publish socket
    if (_pub_socket) {
        _pub_socket->close();
        _pub_socket = nullptr;
    }

    // 3. Registry and subscription manager cleaned up via unique_ptr
}
```

### Message Format

```
XPUB/XSUB Message Format (multipart):
┌──────────────────┐
│ Frame 1: Topic   │  (variable length string)
├──────────────────┤
│ Frame 2: Data    │  (variable length binary)
└──────────────────┘

Subscription Message (sent by XSUB to XPUB):
┌──────────────────┐
│ 0x01 + prefix    │  Subscribe to prefix
├──────────────────┤
│ 0x00 + prefix    │  Unsubscribe from prefix
└──────────────────┘
```

---

## Performance Characteristics

### Latency

| Operation | LOCAL (inproc) | REMOTE (LAN) | REMOTE (WAN) |
|-----------|----------------|--------------|--------------|
| Publish | 0.1-1 µs | 10-100 µs | 1-100 ms |
| Subscribe | 1-10 µs | 50-200 µs | 50-500 ms |
| Disconnect | 1-10 µs | 10-50 µs | 10-100 ms |

### Throughput (Messages/Second)

| Message Size | LOCAL (inproc) | REMOTE (1Gbps) |
|--------------|----------------|----------------|
| 64B | 10M msg/s | 1M msg/s |
| 1KB | 5M msg/s | 500K msg/s |
| 8KB | 2M msg/s | 100K msg/s |
| 64KB | 200K msg/s | 15K msg/s |

### Memory Usage

| Component | Memory |
|-----------|--------|
| SPOT instance base | ~8 KB |
| Per topic (registry) | ~200 bytes |
| Per subscription | ~200 bytes |
| Per cluster connection | ~4 KB |
| Message buffer (default HWM=1000) | ~1 MB |

---

## Design Decisions

### Shared XPUB/XSUB Per Instance

SPOT uses a single shared XPUB/XSUB socket pair per instance:

**Advantages:**
- Constant socket count regardless of topic count
- Simple resource management
- Efficient topic filtering using ZeroMQ's trie-based matching

**Considerations:**
- All messages pass through the same socket (serialization point)
- Consider multiple instances for high-throughput scenarios

### Pattern Subscription (Prefix Matching)

XPUB/XSUB uses prefix matching:

```
"events:*" pattern → converted to "events:" prefix
```

**Matching Examples:**
- `events:` prefix matches `events:login`, `events:logout`, `events:user:created`
- `game:player:` prefix matches `game:player:spawn`, `game:player:death`

### Cluster Connection Management

`cluster_add()` connects a new endpoint to the XSUB socket, and `cluster_remove()` terminates the connection via `term_endpoint()`:

```cpp
// cluster_add(): Connect new endpoint to XSUB
_recv_socket->connect(endpoint.c_str());
_connected_endpoints.insert(endpoint);

// cluster_remove(): Disconnect endpoint
_recv_socket->term_endpoint(endpoint.c_str());
_connected_endpoints.erase(endpoint);
```

---

## See Also

- [API Reference](API.md) - Complete API documentation
- [Quick Start](QUICK_START.md) - Getting started guide
- [Clustering Guide](CLUSTERING.md) - Multi-node setup
- [Usage Patterns](PATTERNS.md) - Common patterns and best practices

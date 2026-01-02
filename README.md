# ServerLink

High-performance messaging library with ZeroMQ-compatible API and Redis-style Pub/Sub features.

## Overview

ServerLink is a C/C++ messaging library that provides:

- **ZeroMQ-compatible Socket Patterns**: ROUTER, PUB/SUB, XPUB/XSUB, PAIR
- **Redis-style Pub/Sub**: Pattern subscriptions, introspection, sharding, clustering
- **High Performance**: Optimized I/O with epoll/kqueue, zero-copy messaging
- **Cross-Platform**: Linux, macOS, BSD (Windows planned)
- **Simple C API**: Clean interface with C++ compatibility

## Socket Types

| Type | Description |
|------|-------------|
| `SLK_ROUTER` | Server-side routing socket with client identification |
| `SLK_PUB` | Publisher socket (fan-out) |
| `SLK_SUB` | Subscriber socket with topic filtering |
| `SLK_XPUB` | Extended publisher with subscription visibility |
| `SLK_XSUB` | Extended subscriber with manual subscription control |
| `SLK_PAIR` | Exclusive 1:1 bidirectional socket |

## Transport Protocols

| Protocol | Description |
|----------|-------------|
| `tcp://` | TCP/IP networking |
| `inproc://` | In-process (thread-to-thread) |

## Features

### 1. ROUTER Socket Pattern

Server-side routing with automatic client identification:

```c
slk_socket_t *router = slk_socket(ctx, SLK_ROUTER);
slk_bind(router, "tcp://*:5555");

// Enable connection notifications
int notify = 1;
slk_setsockopt(router, SLK_ROUTER_NOTIFY, &notify, sizeof(notify));

// Receive: [routing_id][empty][payload]
char id[256], empty[1], msg[1024];
slk_recv(router, id, sizeof(id), 0);
slk_recv(router, empty, sizeof(empty), 0);
slk_recv(router, msg, sizeof(msg), 0);

// Reply to specific client
slk_send(router, id, id_len, SLK_SNDMORE);
slk_send(router, "", 0, SLK_SNDMORE);
slk_send(router, "response", 8, 0);
```

**Options:**
- `SLK_ROUTER_MANDATORY` - Fail if peer not connected
- `SLK_ROUTER_HANDOVER` - Transfer to new peer with same ID
- `SLK_ROUTER_NOTIFY` - Enable connect/disconnect events
- `SLK_CONNECT_ROUTING_ID` - Set custom routing ID when connecting

### 2. Basic Pub/Sub

Simple publish-subscribe messaging:

```c
// Publisher
slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
slk_bind(pub, "tcp://*:5556");
slk_send(pub, "news.sports Hello!", 18, 0);

// Subscriber
slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_connect(sub, "tcp://localhost:5556");
slk_setsockopt(sub, SLK_SUBSCRIBE, "news.", 5);  // Topic filter

char buf[256];
int len = slk_recv(sub, buf, sizeof(buf), 0);
```

### 3. Pattern Subscriptions (Redis PSUBSCRIBE)

Glob pattern matching for subscriptions:

```c
slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_connect(sub, "tcp://localhost:5556");

// Glob patterns
slk_setsockopt(sub, SLK_PSUBSCRIBE, "news.*", 6);      // news.sports, news.tech
slk_setsockopt(sub, SLK_PSUBSCRIBE, "user.?", 6);      // user.1, user.a
slk_setsockopt(sub, SLK_PSUBSCRIBE, "alert.[0-9]", 11); // alert.0 ~ alert.9

// Unsubscribe
slk_setsockopt(sub, SLK_PUNSUBSCRIBE, "news.*", 6);
```

**Supported Patterns:**
| Pattern | Description | Example |
|---------|-------------|---------|
| `*` | Any string | `news.*` → `news.sports` |
| `?` | Single character | `user.?` → `user.1` |
| `[abc]` | Character set | `[abc]def` → `adef` |
| `[a-z]` | Character range | `id.[0-9]` → `id.5` |

### 4. Pub/Sub Introspection (Redis PUBSUB)

Query active channels and subscriptions:

```c
// Get active channels matching pattern
char **channels;
size_t count;
slk_pubsub_channels(ctx, "news.*", &channels, &count);
for (size_t i = 0; i < count; i++) {
    printf("Channel: %s\n", channels[i]);
}
slk_pubsub_channels_free(channels, count);

// Get subscriber count per channel
const char *names[] = {"news.sports", "news.tech"};
size_t numsub[2];
slk_pubsub_numsub(ctx, names, 2, numsub);
// numsub[0] = subscribers for "news.sports"

// Get total pattern subscriptions
int numpat = slk_pubsub_numpat(ctx);
```

### 5. Sharded Pub/Sub (Redis Cluster Compatible)

Horizontal scaling with CRC16 hash slots:

```c
// Create sharded context (16384 slots, Redis-compatible)
slk_sharded_pubsub_t *shard = slk_sharded_pubsub_new(ctx, 16);
slk_sharded_pubsub_set_hwm(shard, 10000);

// Publish to sharded channel
slk_spublish(shard, "events.login", data, len);

// Hash tags for co-location
slk_spublish(shard, "{room:1}chat", msg1, len1);     // Same shard
slk_spublish(shard, "{room:1}members", msg2, len2);  // Same shard

// Subscribe
slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_ssubscribe(shard, sub, "events.login");

char buf[1024];
slk_recv(sub, buf, sizeof(buf), 0);

slk_sharded_pubsub_destroy(&shard);
```

**Features:**
- CRC16 hashing with 16384 slots (Redis Cluster compatible)
- Hash tag `{tag}channel` for related channel co-location
- Per-shard HWM configuration
- Thread-safe with fine-grained locking

### 6. Broker Pub/Sub (XSUB/XPUB Proxy)

Centralized message broker:

```c
// Create broker
slk_pubsub_broker_t *broker = slk_pubsub_broker_new(ctx,
    "tcp://*:5555",   // Publishers connect here
    "tcp://*:5556");  // Subscribers connect here

// Start in background
slk_pubsub_broker_start(broker);

// Publishers connect to frontend
slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
slk_connect(pub, "tcp://localhost:5555");

// Subscribers connect to backend
slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_connect(sub, "tcp://localhost:5556");
slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);

// Get statistics
size_t msg_count;
slk_pubsub_broker_stats(broker, &msg_count);

// Graceful shutdown
slk_pubsub_broker_stop(broker);
slk_pubsub_broker_destroy(&broker);
```

### 7. Cluster Pub/Sub (Distributed)

Multi-node distributed pub/sub:

```c
// Create cluster
slk_pubsub_cluster_t *cluster = slk_pubsub_cluster_new(ctx);

// Add nodes
slk_pubsub_cluster_add_node(cluster, "tcp://node1:5555");
slk_pubsub_cluster_add_node(cluster, "tcp://node2:5555");
slk_pubsub_cluster_add_node(cluster, "tcp://node3:5555");

// Subscribe (routes to appropriate node)
slk_pubsub_cluster_subscribe(cluster, "news.sports");

// Pattern subscribe (broadcasts to all nodes)
slk_pubsub_cluster_psubscribe(cluster, "alerts.*");

// Publish (auto-routes by channel hash)
slk_pubsub_cluster_publish(cluster, "news.sports", data, len);

// Hash tags for routing
slk_pubsub_cluster_publish(cluster, "{user:123}messages", data, len);

// Receive from any node
char channel[256];
size_t channel_len = sizeof(channel);
char data[4096];
size_t data_len = sizeof(data);
slk_pubsub_cluster_recv(cluster, channel, &channel_len, data, &data_len, 0);

// Node management
slk_pubsub_cluster_remove_node(cluster, "tcp://node2:5555");

slk_pubsub_cluster_destroy(&cluster);
```

### 8. Extended Pub/Sub (XPUB/XSUB)

Fine-grained subscription control:

```c
// XPUB - see subscription messages
slk_socket_t *xpub = slk_socket(ctx, SLK_XPUB);
int verbose = 1;
slk_setsockopt(xpub, SLK_XPUB_VERBOSE, &verbose, sizeof(verbose));
slk_bind(xpub, "tcp://*:5557");

// Receive subscription notifications
char sub_msg[256];
int len = slk_recv(xpub, sub_msg, sizeof(sub_msg), 0);
// sub_msg[0] = 1 (subscribe) or 0 (unsubscribe)
// sub_msg[1:] = topic

// XSUB - manual subscription management
slk_socket_t *xsub = slk_socket(ctx, SLK_XSUB);
slk_connect(xsub, "tcp://localhost:5557");

// Send subscription upstream
char subscribe[] = "\x01news.";  // 0x01 + topic
slk_send(xsub, subscribe, sizeof(subscribe) - 1, 0);
```

**Options:**
- `SLK_XPUB_VERBOSE` - Forward all subscription messages
- `SLK_XPUB_VERBOSER` - Include unsubscribe messages
- `SLK_XPUB_NODROP` - Block instead of drop at HWM
- `SLK_XPUB_MANUAL` - Manual subscription handling
- `SLK_XPUB_WELCOME_MSG` - Welcome message for new subscribers

### 9. Modern Poller API

Efficient event-driven I/O:

```c
void *poller = slk_poller_new();

// Add sockets
slk_poller_add(poller, socket1, user_data1, SLK_POLLIN);
slk_poller_add(poller, socket2, user_data2, SLK_POLLIN | SLK_POLLOUT);

// Wait for events
slk_poller_event_t event;
while (slk_poller_wait(poller, &event, 1000) == 0) {
    if (event.events & SLK_POLLIN) {
        // Socket ready for reading
    }
}

slk_poller_destroy(&poller);
```

### 10. Proxy API

Message forwarding between sockets:

```c
// Simple proxy
slk_proxy(frontend, backend, capture);

// Steerable proxy with control
slk_socket_t *control = slk_socket(ctx, SLK_PAIR);
slk_bind(control, "inproc://proxy-ctrl");

// In proxy thread
slk_socket_t *ctrl_peer = slk_socket(ctx, SLK_PAIR);
slk_connect(ctrl_peer, "inproc://proxy-ctrl");
slk_proxy_steerable(frontend, backend, capture, ctrl_peer);

// Control commands: "PAUSE", "RESUME", "TERMINATE", "STATISTICS"
slk_send(control, "TERMINATE", 9, 0);
```

### 11. Socket Monitoring

Real-time connection events:

```c
void monitor_callback(slk_socket_t *socket, const slk_event_t *event, void *user_data) {
    switch (event->event) {
        case SLK_EVENT_CONNECTED:
            printf("Connected to %s\n", event->endpoint);
            break;
        case SLK_EVENT_DISCONNECTED:
            printf("Disconnected\n");
            break;
    }
}

slk_socket_monitor(socket, monitor_callback, user_data, SLK_EVENT_ALL);
```

### 12. Utility APIs

```c
// High-resolution clock (microseconds)
uint64_t now = slk_clock();

// Sleep
slk_sleep(100);  // 100ms

// Atomic counters
void *counter = slk_atomic_counter_new();
slk_atomic_counter_inc(counter);
int value = slk_atomic_counter_value(counter);
slk_atomic_counter_destroy(&counter);

// Timers
void *timers = slk_timers_new();
int id = slk_timers_add(timers, 1000, timer_callback, arg);  // 1 second
slk_timers_execute(timers);
slk_timers_destroy(&timers);

// Stopwatch
void *watch = slk_stopwatch_start();
// ... work ...
unsigned long elapsed = slk_stopwatch_stop(watch);  // microseconds
```

## Build

### Requirements

- CMake 3.14+
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- POSIX threads

### Building

```bash
git clone https://github.com/ulala-x/serverlink.git
cd serverlink

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run tests
cd build && ctest --output-on-failure

# Install
sudo cmake --install build
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | ON | Build shared library |
| `BUILD_TESTS` | ON | Build test suite |
| `BUILD_EXAMPLES` | ON | Build example programs |
| `CMAKE_BUILD_TYPE` | Debug | Build type (Debug/Release) |

## Examples

Example programs are in `examples/pubsub/`:

| Example | Description |
|---------|-------------|
| `psubscribe_example` | Pattern subscription demo |
| `broker_example` | Broker pub/sub with multiple publishers/subscribers |
| `sharded_example` | Sharded pub/sub with hash tags |
| `cluster_example` | Distributed cluster pub/sub |
| `mesh_topology_example` | MMORPG cell pattern (mesh network) |

## Socket Options Reference

### ROUTER Options
| Option | Type | Description |
|--------|------|-------------|
| `SLK_ROUTING_ID` | bytes | Socket identity |
| `SLK_CONNECT_ROUTING_ID` | bytes | Peer identity on connect |
| `SLK_ROUTER_MANDATORY` | int | Fail on unroutable message |
| `SLK_ROUTER_HANDOVER` | int | Enable peer handover |
| `SLK_ROUTER_NOTIFY` | int | Enable connection events |

### Pub/Sub Options
| Option | Type | Description |
|--------|------|-------------|
| `SLK_SUBSCRIBE` | bytes | Add topic filter |
| `SLK_UNSUBSCRIBE` | bytes | Remove topic filter |
| `SLK_PSUBSCRIBE` | bytes | Add glob pattern filter |
| `SLK_PUNSUBSCRIBE` | bytes | Remove glob pattern filter |
| `SLK_XPUB_VERBOSE` | int | Forward all subscriptions |
| `SLK_XPUB_NODROP` | int | Block at HWM instead of drop |
| `SLK_TOPICS_COUNT` | int | Get active topic count (read-only) |

### Buffer Options
| Option | Type | Description |
|--------|------|-------------|
| `SLK_SNDHWM` | int | Send high water mark (messages) |
| `SLK_RCVHWM` | int | Receive high water mark (messages) |
| `SLK_SNDBUF` | int | Send buffer size (bytes) |
| `SLK_RCVBUF` | int | Receive buffer size (bytes) |

### Connection Options
| Option | Type | Description |
|--------|------|-------------|
| `SLK_LINGER` | int | Linger time on close (ms) |
| `SLK_RECONNECT_IVL` | int | Reconnect interval (ms) |
| `SLK_RECONNECT_IVL_MAX` | int | Max reconnect interval (ms) |
| `SLK_BACKLOG` | int | Listen backlog size |

### Heartbeat Options
| Option | Type | Description |
|--------|------|-------------|
| `SLK_HEARTBEAT_IVL` | int | Heartbeat interval (ms) |
| `SLK_HEARTBEAT_TIMEOUT` | int | Heartbeat timeout (ms) |
| `SLK_HEARTBEAT_TTL` | int | Heartbeat TTL (hops) |

### TCP Options
| Option | Type | Description |
|--------|------|-------------|
| `SLK_TCP_KEEPALIVE` | int | Enable TCP keepalive |
| `SLK_TCP_KEEPALIVE_IDLE` | int | Keepalive idle time (s) |
| `SLK_TCP_KEEPALIVE_INTVL` | int | Keepalive interval (s) |
| `SLK_TCP_KEEPALIVE_CNT` | int | Keepalive probe count |

## Test Results

```
100% tests passed, 0 tests failed out of 41

Test Categories:
- ROUTER socket: 8 tests
- PUB/SUB socket: 12 tests
- Transport (tcp/inproc): 3 tests
- Pattern matching: 2 tests
- Unit tests: 7 tests
- Utilities: 4 tests
- Integration: 2 tests
- Poller/Proxy/Monitor: 3 tests
```

## Platform Support

| Platform | I/O Backend | Status |
|----------|-------------|--------|
| Linux | epoll | ✅ Supported |
| macOS | kqueue | ✅ Supported |
| BSD | kqueue | ✅ Supported |
| Windows | select/IOCP | ⏳ Planned |

## License

Mozilla Public License 2.0 (MPL-2.0). See [LICENSE](LICENSE).

## Acknowledgments

- [ZeroMQ](https://zeromq.org/) - Socket pattern inspiration
- [Redis](https://redis.io/) - Pub/Sub feature inspiration

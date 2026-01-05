[![English](https://img.shields.io/badge/lang:en-red.svg)](README.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](README.ko.md)

# ServerLink

[![CI](https://github.com/ulala-x/serverlink/actions/workflows/ci.yml/badge.svg)](https://github.com/ulala-x/serverlink/actions/workflows/ci.yml)
[![Release](https://github.com/ulala-x/serverlink/actions/workflows/release.yml/badge.svg)](https://github.com/ulala-x/serverlink/actions/workflows/release.yml)
[![License: MPL 2.0](https://img.shields.io/badge/License-MPL%202.0-brightgreen.svg)](https://opensource.org/licenses/MPL-2.0)

High-performance messaging library with ZeroMQ-compatible API and location-transparent Pub/Sub system.

## Features

- **ZeroMQ-compatible Socket Patterns**: ROUTER, PUB/SUB, XPUB/XSUB, PAIR
- **SPOT PUB/SUB**: Location-transparent topic-based messaging system
- **High Performance**: Optimized I/O with epoll/kqueue/select, zero-copy messaging
- **Cross-platform**: Linux, macOS, Windows, BSD support
- **C/C++ API**: Clean C API with C++ compatibility

## Quick Start

### Build

```bash
git clone https://github.com/ulalax/serverlink.git
cd serverlink

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run tests
ctest --test-dir build -C Release --output-on-failure
```

### Basic Example

```c
#include <serverlink/serverlink.h>

int main() {
    // Create context
    slk_ctx_t *ctx = slk_ctx_new();

    // Publisher
    slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
    slk_bind(pub, "tcp://*:5555");
    slk_send(pub, "hello world", 11, 0);

    // Subscriber
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    slk_connect(sub, "tcp://localhost:5555");
    slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);  // Subscribe to all messages

    char buf[256];
    slk_recv(sub, buf, sizeof(buf), 0);

    // Cleanup
    slk_close(pub);
    slk_close(sub);
    slk_ctx_destroy(ctx);
    return 0;
}
```

---

## Socket Types

| Type | Description |
|------|-------------|
| `SLK_ROUTER` | Server-side routing socket with client identity |
| `SLK_PUB` | Publisher socket (fan-out) |
| `SLK_SUB` | Subscriber socket with topic filtering |
| `SLK_XPUB` | Extended publisher with subscription visibility |
| `SLK_XSUB` | Extended subscriber with manual subscription control |
| `SLK_PAIR` | Exclusive 1:1 bidirectional socket |

## Transport Protocols

| Protocol | Description | Latency |
|----------|-------------|---------|
| `tcp://` | TCP/IP networking | 10-100 µs |
| `inproc://` | In-process (inter-thread) | < 1 µs |

---

## SPOT PUB/SUB

**SPOT** (Scalable Partitioned Ordered Topics) - Location-transparent topic-based messaging system

### Features

- **Location Transparency**: Subscribe/publish regardless of topic location
- **LOCAL Topics**: Zero-copy via inproc (nanosecond latency)
- **REMOTE Topics**: Automatic routing via TCP
- **Pattern Subscription**: Prefix matching (`events:*` → `events:`)
- **Cluster Sync**: Automatic topic discovery across nodes

### Basic Usage

```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Create LOCAL topic
slk_spot_topic_create(spot, "game:player");

// Subscribe
slk_spot_subscribe(spot, "game:player");

// Publish
slk_spot_publish(spot, "game:player", "hello", 5);

// Receive
char topic[256], data[4096];
size_t topic_len, data_len;
slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, 0);

slk_spot_destroy(&spot);
slk_ctx_destroy(ctx);
```

### Cluster Configuration

```c
// Server Node
slk_spot_topic_create(spot, "sensor:temp");
slk_spot_bind(spot, "tcp://*:5555");

// Client Node
slk_spot_cluster_add(spot, "tcp://server:5555");
slk_spot_subscribe(spot, "sensor:temp");
```

### Pattern Subscription

```c
// Subscribe to all topics starting with "events:" prefix
slk_spot_subscribe_pattern(spot, "events:*");

// Matches: events:login, events:logout, events:user:created
```

**Detailed Documentation**: [docs/spot/](docs/spot/)

---

## ROUTER Socket

Server-side routing with client identity:

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
- `SLK_ROUTER_MANDATORY` - Fail on send to disconnected peer
- `SLK_ROUTER_HANDOVER` - Handover to new peer with same ID
- `SLK_ROUTER_NOTIFY` - Enable connect/disconnect events
- `SLK_CONNECT_ROUTING_ID` - Set routing ID on connect

---

## PUB/SUB

### Basic Pub/Sub

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
slk_recv(sub, buf, sizeof(buf), 0);
```

### Pattern Subscription

```c
slk_setsockopt(sub, SLK_PSUBSCRIBE, "news.*", 6);       // news.sports, news.tech
slk_setsockopt(sub, SLK_PSUBSCRIBE, "user.?", 6);       // user.1, user.a
slk_setsockopt(sub, SLK_PSUBSCRIBE, "alert.[0-9]", 11); // alert.0 ~ alert.9
```

| Pattern | Description | Example |
|---------|-------------|---------|
| `*` | Any string | `news.*` → `news.sports` |
| `?` | Single character | `user.?` → `user.1` |
| `[abc]` | Character set | `[abc]def` → `adef` |
| `[a-z]` | Character range | `id.[0-9]` → `id.5` |

### XPUB/XSUB (Extended Pub/Sub)

```c
// XPUB - Receive subscription messages
slk_socket_t *xpub = slk_socket(ctx, SLK_XPUB);
int verbose = 1;
slk_setsockopt(xpub, SLK_XPUB_VERBOSE, &verbose, sizeof(verbose));
slk_bind(xpub, "tcp://*:5557");

// Receive subscription notification
char sub_msg[256];
slk_recv(xpub, sub_msg, sizeof(sub_msg), 0);
// sub_msg[0] = 1 (subscribe) or 0 (unsubscribe)
// sub_msg[1:] = topic

// XSUB - Manual subscription management
slk_socket_t *xsub = slk_socket(ctx, SLK_XSUB);
slk_connect(xsub, "tcp://localhost:5557");
char subscribe[] = "\x01news.";  // 0x01 + topic
slk_send(xsub, subscribe, sizeof(subscribe) - 1, 0);
```

---

## Poller API

Event-driven I/O:

```c
void *poller = slk_poller_new();

slk_poller_add(poller, socket1, user_data1, SLK_POLLIN);
slk_poller_add(poller, socket2, user_data2, SLK_POLLIN | SLK_POLLOUT);

slk_poller_event_t event;
while (slk_poller_wait(poller, &event, 1000) == 0) {
    if (event.events & SLK_POLLIN) {
        // Ready to read
    }
}

slk_poller_destroy(&poller);
```

---

## Socket Options Reference

### Buffer Options
| Option | Type | Description |
|--------|------|-------------|
| `SLK_SNDHWM` | int | Send high water mark (message count) |
| `SLK_RCVHWM` | int | Receive high water mark (message count) |
| `SLK_SNDBUF` | int | Send buffer size (bytes) |
| `SLK_RCVBUF` | int | Receive buffer size (bytes) |

### Connection Options
| Option | Type | Description |
|--------|------|-------------|
| `SLK_LINGER` | int | Linger time on close (ms) |
| `SLK_RECONNECT_IVL` | int | Reconnection interval (ms) |
| `SLK_RECONNECT_IVL_MAX` | int | Maximum reconnection interval (ms) |
| `SLK_BACKLOG` | int | Listen backlog size |

### TCP Options
| Option | Type | Description |
|--------|------|-------------|
| `SLK_TCP_KEEPALIVE` | int | Enable TCP keepalive |
| `SLK_TCP_KEEPALIVE_IDLE` | int | Keepalive idle time (seconds) |
| `SLK_TCP_KEEPALIVE_INTVL` | int | Keepalive interval (seconds) |
| `SLK_TCP_KEEPALIVE_CNT` | int | Keepalive probe count |

---

## Platform Support

| Platform | Architecture | I/O Backend | Status |
|----------|--------------|-------------|--------|
| Linux | x64, ARM64 | epoll | ✅ |
| macOS | x64, ARM64 | kqueue | ✅ |
| Windows | x64, ARM64 | select | ✅ |
| BSD | x64 | kqueue | ✅ |

---

## Test Results

```
78 tests passed

Core (47):
├── ROUTER: 8
├── PUB/SUB: 12
├── Transport: 4
├── Unit: 11
├── Utilities: 4
├── Integration: 1
├── Poller/Proxy/Monitor: 4
└── Windows: 1

SPOT (31):
├── Basic: 11
├── Local: 6
├── Remote: 5
├── Cluster: 4
└── Mixed: 5
```

---

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | ON | Build shared library |
| `BUILD_TESTS` | ON | Build test suite |
| `BUILD_EXAMPLES` | ON | Build example programs |
| `CMAKE_BUILD_TYPE` | Debug | Build type (Debug/Release) |

## Requirements

- CMake 3.14+
- C++20 compiler (GCC 10+, Clang 10+, MSVC 2019+)
- POSIX threads (Linux/macOS) or Win32 threads (Windows)

---

## Documentation

- **SPOT PUB/SUB**: [docs/spot/](docs/spot/)
  - [API Reference](docs/spot/API.md)
  - [Architecture](docs/spot/ARCHITECTURE.md)
  - [Quick Start](docs/spot/QUICK_START.md)
  - [Clustering Guide](docs/spot/CLUSTERING.md)

---

## License

Mozilla Public License 2.0 (MPL-2.0). See [LICENSE](LICENSE).

## Acknowledgments

- [ZeroMQ](https://zeromq.org/) - Socket pattern inspiration
- [Redis](https://redis.io/) - Pub/Sub feature inspiration

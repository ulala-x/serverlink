[![English](https://img.shields.io/badge/lang:en-red.svg)](README.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](README.ko.md)

# SPOT PUB/SUB Documentation

Complete documentation for ServerLink SPOT (Scalable Partitioned Ordered Topics).

## What is SPOT?

SPOT is a **location-transparent pub/sub system** built on ServerLink that provides:

- **LOCAL Topics**: Zero-copy inproc messaging (nanosecond latency)
- **REMOTE Topics**: TCP networking with automatic routing
- **Cluster Sync**: Automatic topic discovery across nodes
- **Pattern Matching**: Subscribe to multiple topics with wildcards

## Documentation Index

### Getting Started

1. **[Quick Start Guide](QUICK_START.md)** - Get started in 5 minutes
   - What is SPOT?
   - Installation
   - Your first SPOT application
   - Local and remote topics
   - Event loop integration

### Core Documentation

2. **[API Reference](API.md)** - Complete function reference
   - All `slk_spot_*` functions
   - Data types and constants
   - Error codes
   - Thread safety

3. **[Architecture](ARCHITECTURE.md)** - Internal design
   - Component architecture
   - Class diagrams
   - Data flow
   - Threading model
   - Performance characteristics

4. **[Protocol Specification](PROTOCOL.md)** - Wire format
   - Message formats
   - Command codes (PUBLISH, SUBSCRIBE, QUERY, etc.)
   - Protocol flows
   - Wire format examples

### Advanced Topics

5. **[Clustering Guide](CLUSTERING.md)** - Multi-node deployment
   - Single node setup
   - Two-node cluster
   - N-node mesh topology
   - Hub-spoke topology
   - Failure handling
   - Production deployment

6. **[Usage Patterns](PATTERNS.md)** - Design patterns
   - Explicit routing (game servers)
   - Central registry (microservices)
   - Hybrid approach
   - Producer-consumer
   - Fan-out (1:N)
   - Fan-in (N:1)
   - Event sourcing
   - Stream processing

7. **[Migration Guide](MIGRATION.md)** - From traditional PUB/SUB
   - API mapping table
   - Step-by-step migration
   - Compatibility notes
   - Example migrations

## Quick Reference

### Create and Use SPOT

```c
// Create SPOT instance
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Create LOCAL topic
slk_spot_topic_create(spot, "game:player");

// Subscribe
slk_spot_subscribe(spot, "game:player");

// Publish
slk_spot_publish(spot, "game:player", "data", 4);

// Receive
char topic[256], data[4096];
size_t topic_len, data_len;
slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, 0);

// Cleanup
slk_spot_destroy(&spot);
slk_ctx_destroy(ctx);
```

### Cluster Setup

```c
// Node A (Server)
slk_spot_topic_create(spot, "sensor:temp");
slk_spot_bind(spot, "tcp://*:5555");

// Node B (Client)
slk_spot_cluster_add(spot, "tcp://nodeA:5555");
slk_spot_cluster_sync(spot, 1000);
slk_spot_subscribe(spot, "sensor:temp");
```

## Use Cases

- **Game Servers**: Player events, chat rooms, world state
- **Microservices**: Event distribution, service discovery
- **IoT Systems**: Sensor data routing, device management
- **Analytics**: Real-time metrics, log aggregation
- **Financial**: Market data, trade events

## Key Features

### Location Transparency

Subscribe to topics without knowing their location:
```c
// Works for both LOCAL and REMOTE topics
slk_spot_subscribe(spot, "any:topic");
```

### Pattern Subscriptions

Subscribe to multiple topics with wildcards:
```c
slk_spot_subscribe_pattern(spot, "game:player:*");
// Receives: game:player:spawn, game:player:death, etc.
```

### Automatic Discovery

Discover topics across cluster nodes:
```c
slk_spot_cluster_sync(spot, 1000);
// Now knows about all remote topics
```

## Performance

| Transport | Latency | Throughput |
|-----------|---------|------------|
| **inproc** (LOCAL) | 0.01-0.1 µs | 10M msg/s |
| **TCP** (REMOTE) | 10-50 µs | 1M msg/s |

## Examples

### Basic Pub/Sub
```c
slk_spot_topic_create(spot, "events");
slk_spot_subscribe(spot, "events");
slk_spot_publish(spot, "events", "hello", 5);
```

### Remote Topics
```c
// Server
slk_spot_topic_create(spot, "sensor:temp");
slk_spot_bind(spot, "tcp://*:5555");

// Client
slk_spot_topic_route(spot, "sensor:temp", "tcp://server:5555");
slk_spot_subscribe(spot, "sensor:temp");
```

### Cluster Mesh
```c
// All nodes connect to all others
slk_spot_bind(nodeA, "tcp://*:5555");
slk_spot_bind(nodeB, "tcp://*:5556");

slk_spot_cluster_add(nodeA, "tcp://nodeB:5556");
slk_spot_cluster_add(nodeB, "tcp://nodeA:5555");

slk_spot_cluster_sync(nodeA, 1000);
slk_spot_cluster_sync(nodeB, 1000);
```

## API Overview

### Lifecycle
- `slk_spot_new()` - Create instance
- `slk_spot_destroy()` - Destroy instance

### Topic Management
- `slk_spot_topic_create()` - Create LOCAL topic
- `slk_spot_topic_route()` - Route to REMOTE topic
- `slk_spot_topic_destroy()` - Destroy topic

### Pub/Sub
- `slk_spot_subscribe()` - Subscribe to topic
- `slk_spot_subscribe_pattern()` - Pattern subscription (LOCAL only)
- `slk_spot_unsubscribe()` - Unsubscribe
- `slk_spot_publish()` - Publish message
- `slk_spot_recv()` - Receive message

### Clustering
- `slk_spot_bind()` - Bind for server mode
- `slk_spot_cluster_add()` - Add cluster node
- `slk_spot_cluster_remove()` - Remove cluster node
- `slk_spot_cluster_sync()` - Synchronize topics

### Introspection
- `slk_spot_list_topics()` - List all topics
- `slk_spot_topic_exists()` - Check topic exists
- `slk_spot_topic_is_local()` - Check if LOCAL

### Configuration
- `slk_spot_set_hwm()` - Set high water marks
- `slk_spot_fd()` - Get pollable FD

## Error Handling

All functions return `-1` on error and set `errno`:

```c
int rc = slk_spot_publish(spot, "topic", data, len);
if (rc != 0) {
    int err = slk_errno();
    fprintf(stderr, "Error: %s\n", slk_strerror(err));
}
```

**Common Error Codes:**
- `SLK_EINVAL` - Invalid argument
- `SLK_ENOMEM` - Out of memory
- `SLK_ENOENT` - Topic not found
- `SLK_EEXIST` - Topic already exists
- `SLK_EAGAIN` - Resource temporarily unavailable
- `SLK_EHOSTUNREACH` - Remote host unreachable

## Thread Safety

SPOT instances are **thread-safe**:
- Multiple threads can call `slk_spot_recv()` concurrently
- Publish operations are serialized per topic
- Internal locking uses `std::shared_mutex`

## Best Practices

1. **Use LOCAL topics** for same-process communication
2. **Pattern subscriptions** for flexible routing (LOCAL only)
3. **Cluster sync** for automatic topic discovery
4. **Error handling** - always check return values
5. **Resource cleanup** - use `slk_spot_destroy()`

## Platform Support

- **Linux**: epoll (tested)
- **Windows**: select (tested)
- **macOS**: kqueue (tested)
- **BSD**: kqueue (should work)

## Building

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Testing

```bash
cd build
ctest -R spot --output-on-failure
```

**Test Coverage:**
- `test_spot_basic` - Basic pub/sub operations
- `test_spot_local` - LOCAL topic tests
- `test_spot_remote` - REMOTE topic tests
- `test_spot_cluster` - Cluster synchronization
- `test_spot_mixed` - Mixed LOCAL/REMOTE

## Contributing

See `docs/CONTRIBUTING.md` for contribution guidelines.

## License

ServerLink is licensed under the Mozilla Public License 2.0 (MPL-2.0).

## Getting Help

- **Documentation**: `docs/spot/`
- **Examples**: `examples/spot_cluster_sync_example.cpp`
- **Tests**: `tests/spot/`
- **GitHub Issues**: https://github.com/ulala-x/serverlink/issues

---

**Version**: 1.0
**Last Updated**: 2026-01-04
**Status**: Production Ready

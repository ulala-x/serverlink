[![English](https://img.shields.io/badge/lang:en-red.svg)](QUICK_START.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](QUICK_START.ko.md)

# SPOT PUB/SUB Quick Start Guide

Get started with ServerLink SPOT (Scalable Partitioned Ordered Topics) in 5 minutes.

## Table of Contents

1. [What is SPOT?](#what-is-spot)
2. [Installation](#installation)
3. [Your First SPOT Application](#your-first-spot-application)
4. [Local Topics](#local-topics)
5. [Remote Topics](#remote-topics)
6. [Next Steps](#next-steps)

---

## What is SPOT?

**SPOT** (Scalable Partitioned Ordered Topics) is a location-transparent pub/sub system built on ServerLink.

**Key Features:**
- **Location Transparency**: Subscribe to topics without knowing their location
- **LOCAL Topics**: Zero-copy inproc messaging (nanosecond latency)
- **REMOTE Topics**: TCP networking with automatic routing
- **Cluster Sync**: Discover topics across multiple nodes
- **Pattern Matching**: Subscribe to multiple topics with wildcards

**Use Cases:**
- Game servers (player events, chat rooms)
- Microservices (event distribution)
- IoT systems (sensor data routing)
- Real-time analytics (data aggregation)

---

## Installation

### Build ServerLink

```bash
# Clone repository
git clone https://github.com/ulalax/serverlink.git
cd serverlink

# Build with CMake
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build --config Release

# Run tests
cd build && ctest -C Release
```

### Link to Your Project

**CMake:**
```cmake
find_package(ServerLink REQUIRED)
target_link_libraries(your_app ServerLink::serverlink)
```

**Manual:**
```bash
gcc your_app.c -lserverlink -o your_app
```

---

## Your First SPOT Application

### Simple Pub/Sub (Single Process)

```c
#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>

int main()
{
    // Create context and SPOT instance
    slk_ctx_t *ctx = slk_ctx_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    // Create a LOCAL topic
    slk_spot_topic_create(spot, "chat:lobby");

    // Subscribe to the topic
    slk_spot_subscribe(spot, "chat:lobby");

    // Publish a message
    const char *msg = "Hello, SPOT!";
    slk_spot_publish(spot, "chat:lobby", msg, strlen(msg));

    // Wait for message to arrive (inproc is fast!)
    slk_sleep(10);

    // Receive the message
    char topic[256], data[4096];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, SLK_DONTWAIT);

    if (rc == 0) {
        topic[topic_len] = '\0';
        data[data_len] = '\0';
        printf("Received on '%s': %s\n", topic, data);
    }

    // Cleanup
    slk_spot_destroy(&spot);
    slk_ctx_destroy(ctx);

    return 0;
}
```

**Expected Output:**
```
Received on 'chat:lobby': Hello, SPOT!
```

---

## Local Topics

LOCAL topics use inproc transport for zero-copy, same-process messaging.

### Creating Multiple Topics

```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Create game-related topics
slk_spot_topic_create(spot, "game:player:spawn");
slk_spot_topic_create(spot, "game:player:death");
slk_spot_topic_create(spot, "game:score:update");

// Subscribe to specific topics
slk_spot_subscribe(spot, "game:player:spawn");
slk_spot_subscribe(spot, "game:player:death");
```

### Pattern Subscriptions

```c
// Subscribe to all player events
slk_spot_subscribe_pattern(spot, "game:player:*");

// Now receives messages from:
// - game:player:spawn
// - game:player:death
// - game:player:move
// - etc.
```

### Publishing and Receiving

```c
// Publish to different topics
slk_spot_publish(spot, "game:player:spawn", "Player1", 7);
slk_spot_publish(spot, "game:player:death", "Player2", 7);
slk_spot_publish(spot, "game:score:update", "1000", 4);

slk_sleep(10); // Allow messages to propagate

// Receive messages
for (int i = 0; i < 2; i++) { // Expecting 2 messages (pattern match)
    char topic[256], data[256];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, 100);

    if (rc == 0) {
        topic[topic_len] = '\0';
        data[data_len] = '\0';
        printf("Topic: %s, Data: %s\n", topic, data);
    }
}
```

---

## Remote Topics

REMOTE topics route messages across TCP connections to other SPOT nodes.

### Two-Node Setup

**Node 1 (Server):**
```c
#include <serverlink/serverlink.h>

void run_server()
{
    slk_ctx_t *ctx = slk_ctx_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    // Create LOCAL topic
    slk_spot_topic_create(spot, "sensor:temperature");

    // Bind to accept connections
    slk_spot_bind(spot, "tcp://*:5555");

    // Publish sensor data
    while (1) {
        slk_spot_publish(spot, "sensor:temperature", "25.5", 4);
        slk_sleep(1000); // 1 second
    }
}
```

**Node 2 (Client):**
```c
#include <serverlink/serverlink.h>
#include <stdio.h>

void run_client()
{
    slk_ctx_t *ctx = slk_ctx_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    // Route topic to remote server
    slk_spot_topic_route(spot, "sensor:temperature", "tcp://localhost:5555");

    // Subscribe to REMOTE topic
    slk_spot_subscribe(spot, "sensor:temperature");

    // Receive sensor data
    while (1) {
        char topic[256], data[256];
        size_t topic_len, data_len;

        int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                               data, sizeof(data), &data_len, 1000);

        if (rc == 0) {
            data[data_len] = '\0';
            printf("Temperature: %s°C\n", data);
        }
    }
}
```

---

## Cluster Synchronization

Automatically discover topics across multiple nodes.

### Three-Node Mesh Cluster

**Node A:**
```c
slk_spot_t *spot = slk_spot_new(ctx);

// Create LOCAL topics
slk_spot_topic_create(spot, "nodeA:data");
slk_spot_bind(spot, "tcp://*:5555");

// Connect to other nodes
slk_spot_cluster_add(spot, "tcp://nodeB:5556");
slk_spot_cluster_add(spot, "tcp://nodeC:5557");

// Synchronize (discover topics from nodeB and nodeC)
slk_spot_cluster_sync(spot, 1000);

// Now can subscribe to topics from nodeB and nodeC
slk_spot_subscribe(spot, "nodeB:data");
slk_spot_subscribe(spot, "nodeC:data");
```

**Node B and C:** Similar setup with different ports.

### Listing All Topics

```c
char **topics;
size_t count;

slk_spot_cluster_sync(spot, 1000);
slk_spot_list_topics(spot, &topics, &count);

printf("All topics in cluster:\n");
for (size_t i = 0; i < count; i++) {
    int is_local = slk_spot_topic_is_local(spot, topics[i]);
    printf("  - %s (%s)\n", topics[i], is_local ? "LOCAL" : "REMOTE");
}

slk_spot_list_topics_free(topics, count);
```

**Output:**
```
All topics in cluster:
  - nodeA:data (LOCAL)
  - nodeB:data (REMOTE)
  - nodeC:data (REMOTE)
```

---

## Event Loop Integration

Use with `poll()` or `epoll()` for non-blocking I/O.

```c
#include <poll.h>

slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "events");
slk_spot_subscribe(spot, "events");

// Get pollable file descriptor
slk_fd_t spot_fd;
slk_spot_fd(spot, &spot_fd);

struct pollfd pfd = {
    .fd = spot_fd,
    .events = POLLIN
};

while (1) {
    int rc = poll(&pfd, 1, 1000);

    if (rc > 0 && (pfd.revents & POLLIN)) {
        char topic[256], data[4096];
        size_t topic_len, data_len;

        slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                      data, sizeof(data), &data_len, SLK_DONTWAIT);

        // Process message
    }
}
```

---

## Error Handling

Always check return values and use `slk_errno()`.

```c
int rc = slk_spot_publish(spot, "topic", data, len);
if (rc != 0) {
    int err = slk_errno();

    if (err == SLK_ENOENT) {
        fprintf(stderr, "Topic not found\n");
    } else if (err == SLK_EAGAIN) {
        fprintf(stderr, "Send buffer full, retry later\n");
    } else {
        fprintf(stderr, "Error: %s\n", slk_strerror(err));
    }
}
```

---

## Common Patterns

### Producer-Consumer

```c
// Producer
slk_spot_topic_create(spot, "jobs:queue");
for (int i = 0; i < 100; i++) {
    char job[64];
    snprintf(job, sizeof(job), "job_%d", i);
    slk_spot_publish(spot, "jobs:queue", job, strlen(job));
}

// Consumer
slk_spot_subscribe(spot, "jobs:queue");
while (1) {
    char topic[256], data[256];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, -1); // Block

    if (rc == 0) {
        // Process job
        data[data_len] = '\0';
        printf("Processing: %s\n", data);
    }
}
```

### Fan-Out (1:N)

```c
// Publisher
slk_spot_topic_create(spot, "broadcast");

// Multiple subscribers
slk_spot_subscribe(spot_sub1, "broadcast");
slk_spot_subscribe(spot_sub2, "broadcast");
slk_spot_subscribe(spot_sub3, "broadcast");

// All subscribers receive the message
slk_spot_publish(spot, "broadcast", "announcement", 12);
```

### Fan-In (N:1)

```c
// Multiple publishers to different topics
slk_spot_topic_create(spot1, "source1");
slk_spot_topic_create(spot2, "source2");
slk_spot_topic_create(spot3, "source3");

// Single subscriber with pattern
slk_spot_subscribe_pattern(aggregator, "source*");

// Receives messages from all sources
```

---

## Performance Tips

1. **Use LOCAL topics** for same-process communication (zero-copy)
2. **Increase HWM** for high-throughput scenarios:
   ```c
   slk_spot_set_hwm(spot, 100000, 100000);
   ```
3. **Batch publishes** when possible
4. **Use non-blocking recv** with event loop integration
5. **Pattern subscriptions** have CPU overhead, use sparingly

---

## Next Steps

Now that you understand the basics, explore:

1. **[Architecture Guide](ARCHITECTURE.md)** - Internal design and data flow
2. **[Protocol Specification](PROTOCOL.md)** - Message formats and commands
3. **[Clustering Guide](CLUSTERING.md)** - Multi-node deployment patterns
4. **[API Reference](API.md)** - Complete function documentation
5. **[Usage Patterns](PATTERNS.md)** - Common design patterns

---

## Troubleshooting

### No messages received

```c
// Ensure topic exists
if (!slk_spot_topic_exists(spot, "topic")) {
    fprintf(stderr, "Topic not found\n");
}

// Check subscription
slk_spot_subscribe(spot, "topic"); // Idempotent

// Add delay for inproc propagation
slk_sleep(10);
```

### Connection refused (REMOTE topics)

```c
// Ensure server is bound first
slk_spot_bind(server_spot, "tcp://*:5555");

// Then connect client
int rc = slk_spot_topic_route(client_spot, "topic", "tcp://server:5555");
if (rc != 0 && slk_errno() == SLK_EHOSTUNREACH) {
    fprintf(stderr, "Server not reachable\n");
}
```

### HWM errors (EAGAIN)

```c
// Increase HWM
slk_spot_set_hwm(spot, 100000, 100000);

// Or handle backpressure
if (slk_errno() == SLK_EAGAIN) {
    slk_sleep(10);
    // Retry publish
}
```

---

## Complete Example

See `examples/spot_cluster_sync_example.cpp` for a full working example of cluster synchronization.

**Run the example:**
```bash
cd build
./examples/spot_cluster_sync_example
```

---

## Getting Help

- **GitHub Issues**: https://github.com/ulalax/serverlink/issues
- **Documentation**: `docs/spot/`
- **Examples**: `examples/`
- **Tests**: `tests/spot/`

Happy messaging with SPOT! 🚀

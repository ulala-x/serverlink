# ServerLink SPOT PUB/SUB Examples

SPOT (Scalable Partitioned Ordered Topics) provides location-transparent pub/sub with topic ID-based routing.

## Overview

SPOT enables seamless communication between local and remote topics using the same API. Topics can be hosted locally (on this node) or routed to remote nodes transparently.

## Examples

### 1. spot_basic.c - Basic Usage

**Purpose**: Learn fundamental SPOT operations

**Features**:
- Creating local topics
- Publishing messages
- Subscribing to topics
- Receiving messages
- Unsubscribing

**Run**:
```bash
./spot_basic
```

**Expected Output**:
- Creates 3 topics (news:weather, news:sports, alerts:traffic)
- Subscribes to 2 topics
- Publishes 3 messages
- Receives 2 messages (1 filtered)
- Demonstrates unsubscribe

### 2. spot_multi_topic.c - Multi-Topic Management

**Purpose**: Advanced topic management

**Features**:
- Dynamic topic creation/destruction
- Pattern-based subscriptions
- High water mark configuration
- Topic existence checking
- Topic listing

**Run**:
```bash
./spot_multi_topic
```

**Expected Output**:
- Creates 8 notification topics
- Subscribes using pattern "notify:email:*"
- Publishes to various topics
- Receives filtered messages
- Destroys topics dynamically

### 3. Cluster Examples - Publisher/Subscriber

**Purpose**: Distributed SPOT across multiple nodes

**Features**:
- TCP-based cluster communication
- Topic discovery via cluster sync
- Remote topic routing
- Server mode with ROUTER socket

**Run Publisher**:
```bash
./spot_cluster_publisher
```

**Run Subscriber** (in another terminal):
```bash
./spot_cluster_subscriber
```

**What Happens**:
1. Publisher binds to `tcp://*:5555`
2. Publisher creates 6 stock/forex/crypto topics
3. Subscriber connects to publisher
4. Subscriber synchronizes topics (discovers remote topics)
5. Subscriber subscribes to 3 topics
6. Publisher sends periodic price updates
7. Subscriber receives updates over TCP

**Key Insight**: Subscriber uses the same API for remote topics as it would for local topics!

### 4. spot_mmorpg_cell.c - MMORPG Spatial Pub/Sub

**Purpose**: Game server scenario demonstrating location transparency

**Scenario**:
- Game world divided into cells (grid-based spatial partitioning)
- Server A manages cells (5,7) and (5,8) - LOCAL
- Server B manages cell (6,7) - REMOTE
- Servers subscribe to adjacent cells for Area of Interest (AoI)

**Features**:
- Spatial interest management
- Adjacent cell subscription
- Location-transparent event publishing
- Inproc (local) and TCP (remote) routing

**Run**:
```bash
./spot_mmorpg_cell
```

**Expected Output**:
- Creates local cells (5,7) and (5,8)
- Registers remote cell (6,7) routing
- Subscribes to adjacent cells
- Publishes player events
- Receives events from subscribed adjacent cells

**Key Benefits**:
```c
// Same API for local and remote!
slk_spot_publish(spot, "zone1:cell:5,7", data, len);  // LOCAL (inproc)
slk_spot_publish(spot, "zone1:cell:6,7", data, len);  // REMOTE (tcp)
```

## Building Examples

```bash
cd build
cmake -B . -S .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target spot_basic spot_multi_topic \
                        spot_cluster_publisher spot_cluster_subscriber \
                        spot_mmorpg_cell
```

## API Quick Reference

### Initialization
```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);
```

### Local Topics
```c
slk_spot_topic_create(spot, "topic:name");        // Create local topic
slk_spot_topic_destroy(spot, "topic:name");       // Destroy topic
slk_spot_topic_exists(spot, "topic:name");        // Check existence
slk_spot_topic_is_local(spot, "topic:name");      // Check if local
```

### Remote Topics
```c
slk_spot_topic_route(spot, "topic:name", "tcp://host:port");  // Route to remote
slk_spot_cluster_add(spot, "tcp://host:port");                // Add cluster node
slk_spot_cluster_sync(spot, 5000);                            // Sync topics
slk_spot_cluster_remove(spot, "tcp://host:port");             // Remove node
```

### Publishing
```c
slk_spot_publish(spot, "topic:name", data, len);  // Publish message
```

### Subscribing
```c
slk_spot_subscribe(spot, "topic:name");           // Exact subscription
slk_spot_subscribe_pattern(spot, "prefix:*");     // Pattern subscription
slk_spot_unsubscribe(spot, "topic:name");         // Unsubscribe
```

### Receiving
```c
slk_spot_recv(spot, topic_buf, topic_size, &topic_len,
              data_buf, data_size, &data_len, flags);
```

### Server Mode
```c
slk_spot_bind(spot, "tcp://*:5555");  // Accept cluster connections
```

### Configuration
```c
slk_spot_set_hwm(spot, send_hwm, recv_hwm);  // Set high water marks
slk_spot_fd(spot, &fd);                       // Get pollable FD
```

### Cleanup
```c
slk_spot_destroy(&spot);
slk_ctx_destroy(ctx);
```

## Use Cases

### Local Pub/Sub
- Single process, multiple threads
- High performance with inproc transport
- Use `spot_basic.c` as reference

### Distributed Pub/Sub
- Multiple processes/servers
- Cluster synchronization
- Use `spot_cluster_*.c` as reference

### Game Servers
- Spatial partitioning (cells, zones)
- Area of Interest (AoI) management
- Server load balancing
- Use `spot_mmorpg_cell.c` as reference

### Microservices
- Service discovery
- Event distribution
- Topic-based routing
- Use cluster examples as reference

## Performance Notes

### Local Topics (inproc)
- Very high throughput (millions of msg/s)
- Low latency (microseconds)
- Zero-copy message passing
- Lock-free queue implementation

### Remote Topics (tcp)
- Network-limited throughput
- Automatic connection management
- Transparent failover
- Configurable HWM

### Best Practices

1. **HWM Configuration**: Set appropriate high water marks
   ```c
   slk_spot_set_hwm(spot, 10000, 10000);  // Game servers
   slk_spot_set_hwm(spot, 1000, 1000);    // Normal apps
   ```

2. **Topic Naming**: Use hierarchical naming
   ```c
   "zone:cell:x,y"      // Game spatial
   "service:event:type" // Microservices
   "category:subcategory:item" // General
   ```

3. **Pattern Subscriptions**: Use for LOCAL topics only
   ```c
   slk_spot_subscribe_pattern(spot, "player:*");  // All player events
   ```

4. **Error Handling**: Always check return values
   ```c
   if (slk_spot_publish(spot, topic, data, len) < 0) {
       fprintf(stderr, "Publish failed: %s\n", slk_strerror(slk_errno()));
   }
   ```

5. **Cleanup**: Always destroy resources
   ```c
   slk_spot_destroy(&spot);  // Closes all sockets
   slk_ctx_destroy(ctx);
   ```

## Troubleshooting

### "Failed to bind: Address already in use"
- Another process is using the port
- Solution: Change port or kill existing process

### "Cluster sync failed: Connection refused"
- Remote server not running
- Solution: Start remote server first

### "No messages received"
- Subscription not propagated yet
- Solution: Add `slk_sleep(10)` after subscribe

### Messages dropped (HWM reached)
- Producer faster than consumer
- Solution: Increase HWM or slow down producer

## See Also

- `serverlink.h` - Full C API reference
- `tests/pubsub/` - Additional pub/sub examples
- `CLAUDE.md` - Project documentation

[![English](https://img.shields.io/badge/lang:en-red.svg)](PATTERNS.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](PATTERNS.ko.md)

# SPOT PUB/SUB Usage Patterns

Common design patterns and best practices for SPOT applications.

## Table of Contents

1. [Overview](#overview)
2. [Explicit Routing (Game Servers)](#explicit-routing-game-servers)
3. [Central Registry (Microservices)](#central-registry-microservices)
4. [Hybrid Approach (Partial Automation)](#hybrid-approach-partial-automation)
5. [Producer-Consumer Pattern](#producer-consumer-pattern)
6. [Fan-Out Pattern (1:N)](#fan-out-pattern-1n)
7. [Fan-In Pattern (N:1)](#fan-in-pattern-n1)
8. [Request-Reply Pattern](#request-reply-pattern)
9. [Event Sourcing](#event-sourcing)
10. [Stream Processing](#stream-processing)

---

## Overview

SPOT supports multiple architectural patterns depending on your use case.

**Pattern Selection Criteria:**
- **Explicit Routing**: Full control, manual topic-to-endpoint mapping
- **Central Registry**: Automatic discovery, service registry required
- **Hybrid**: Mix of explicit and automatic routing

---

## Explicit Routing (Game Servers)

**Use Case:** Game server assigns players to rooms, explicit topic routing to room servers.

### Architecture

```
┌─────────────────┐
│  Game Server    │  (Coordinator)
│   tcp://*:5555  │
└────────┬────────┘
         │
         │ Explicit routing:
         │ player:123 → tcp://room1:5556
         │ player:456 → tcp://room2:5557
         │
    ┌────┴────┐
    ▼         ▼
┌─────────┐ ┌─────────┐
│ Room 1  │ │ Room 2  │
│tcp://*: │ │tcp://*: │
│  5556   │ │  5557   │
└─────────┘ └─────────┘
```

### Implementation

**Game Server (Coordinator):**
```c
typedef struct {
    const char *player_id;
    const char *room_endpoint;
} player_assignment_t;

player_assignment_t assignments[] = {
    {"player:123", "tcp://room1:5556"},
    {"player:456", "tcp://room2:5557"},
    {"player:789", "tcp://room1:5556"}
};

slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Route players to room servers
for (int i = 0; i < 3; i++) {
    slk_spot_topic_route(spot, assignments[i].player_id,
                         assignments[i].room_endpoint);
}

// Subscribe to all player topics
for (int i = 0; i < 3; i++) {
    slk_spot_subscribe(spot, assignments[i].player_id);
}

// Receive player events
while (1) {
    char topic[256], data[4096];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, 100);

    if (rc == 0) {
        topic[topic_len] = '\0';
        printf("Player event: %s\n", topic);
        // Route to game logic
    }
}
```

**Room Server:**
```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Create LOCAL topics for players in this room
slk_spot_topic_create(spot, "player:123");
slk_spot_topic_create(spot, "player:789");

// Bind for external connections
slk_spot_bind(spot, "tcp://*:5556");

// Publish player events
slk_spot_publish(spot, "player:123", "spawn", 5);
slk_spot_publish(spot, "player:789", "move", 4);
```

**Advantages:**
- Full control over routing
- No service discovery overhead
- Deterministic message flow

**Disadvantages:**
- Manual configuration
- Static topology
- No automatic failover

---

## Central Registry (Microservices)

**Use Case:** Microservices architecture with dynamic service discovery.

### Architecture

```
┌──────────────────────┐
│  Service Registry    │  (Consul, etcd, ZooKeeper)
│  - service:auth → tcp://auth:5555
│  - service:user → tcp://user:5556
│  - service:order → tcp://order:5557
└──────────┬───────────┘
           │
           │ Query registry
           │ Auto-route topics
           ▼
┌──────────────────────┐
│   API Gateway        │
│   (SPOT Client)      │
└──────────────────────┘
```

### Implementation

**Service Registration:**
```c
// Service Registry API (pseudo-code)
typedef struct {
    const char *service_name;
    const char *endpoint;
} service_entry_t;

service_entry_t services[] = {
    {"service:auth", "tcp://auth:5555"},
    {"service:user", "tcp://user:5556"},
    {"service:order", "tcp://order:5557"}
};

// Publish to registry (Consul HTTP API)
void register_service(const char *name, const char *endpoint)
{
    // PUT /v1/agent/service/register
    // {"Name": "auth", "Address": "auth", "Port": 5555}
}
```

**Auto-Discovery Client:**
```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Query service registry
service_entry_t *services = query_service_registry();
int service_count = get_service_count();

// Auto-route topics based on registry
for (int i = 0; i < service_count; i++) {
    slk_spot_topic_route(spot, services[i].service_name,
                         services[i].endpoint);
}

// Subscribe to service events
slk_spot_subscribe(spot, "service:auth");
slk_spot_subscribe(spot, "service:user");
```

**Service Implementation:**
```c
// Auth Service
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "service:auth");
slk_spot_bind(spot, "tcp://*:5555");

// Register with registry
register_service("service:auth", "tcp://auth:5555");

// Publish auth events
slk_spot_publish(spot, "service:auth", "user_login", 10);
```

**Advantages:**
- Dynamic service discovery
- Automatic failover (if registry supports health checks)
- Scalable architecture

**Disadvantages:**
- External dependency (registry)
- Additional network hops
- Registry becomes single point of failure

---

## Hybrid Approach (Partial Automation)

**Use Case:** Mix of critical services (explicit routing) and dynamic services (cluster sync).

### Architecture

```
┌────────────────────┐       ┌────────────────────┐
│  Critical Services │       │  Dynamic Services  │
│  (Explicit)        │       │  (Cluster Sync)    │
├────────────────────┤       ├────────────────────┤
│ - payment:*        │       │ - analytics:*      │
│ - security:*       │       │ - logging:*        │
│ - billing:*        │       │ - metrics:*        │
└────────────────────┘       └────────────────────┘
         │                            │
         └─────────┬──────────────────┘
                   ▼
           ┌──────────────┐
           │  API Gateway │
           └──────────────┘
```

### Implementation

```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Explicit routing for critical services
slk_spot_topic_route(spot, "payment:process", "tcp://payment:5555");
slk_spot_topic_route(spot, "security:audit", "tcp://security:5556");

// Cluster sync for dynamic services
slk_spot_cluster_add(spot, "tcp://analytics:5557");
slk_spot_cluster_add(spot, "tcp://logging:5558");
slk_spot_cluster_sync(spot, 1000);

// Subscribe to all topics
slk_spot_subscribe(spot, "payment:process");
slk_spot_subscribe(spot, "security:audit");
slk_spot_subscribe_pattern(spot, "analytics:*");
slk_spot_subscribe_pattern(spot, "logging:*");
```

**Advantages:**
- Best of both worlds
- Critical paths are deterministic
- Dynamic services can scale independently

**Disadvantages:**
- More complex configuration
- Mixed failure modes

---

## Producer-Consumer Pattern

**Use Case:** Work queue for asynchronous task processing.

### Architecture

```
┌──────────┐     ┌──────────┐     ┌──────────┐
│Producer 1│────>│          │<────│Consumer 1│
└──────────┘     │  Queue   │     └──────────┘
┌──────────┐     │ (SPOT)   │     ┌──────────┐
│Producer 2│────>│          │<────│Consumer 2│
└──────────┘     └──────────┘     └──────────┘
```

### Implementation

**Shared Queue Topic:**
```c
// Producer 1
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "jobs:queue");

for (int i = 0; i < 100; i++) {
    char job[64];
    snprintf(job, sizeof(job), "job_%d", i);
    slk_spot_publish(spot, "jobs:queue", job, strlen(job));
}
```

**Consumer (Fair Queueing):**
```c
// Consumer 1
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_subscribe(spot, "jobs:queue");

while (1) {
    char topic[256], data[4096];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, -1);

    if (rc == 0) {
        data[data_len] = '\0';
        printf("Consumer 1 processing: %s\n", data);
        process_job(data);
    }
}
```

**Note:** SPOT uses **fair-queueing** for subscriptions. Multiple consumers receive messages in round-robin fashion.

---

## Fan-Out Pattern (1:N)

**Use Case:** Broadcast message to multiple subscribers.

### Architecture

```
┌────────────┐
│ Publisher  │
└──────┬─────┘
       │
       │ Broadcast
       │
   ┌───┴────┬────┬────┐
   ▼        ▼    ▼    ▼
┌─────┐  ┌─────┐ ┌─────┐ ┌─────┐
│Sub 1│  │Sub 2│ │Sub 3│ │Sub 4│
└─────┘  └─────┘ └─────┘ └─────┘
```

### Implementation

**Publisher:**
```c
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "broadcast:alerts");

// Publish to all subscribers
slk_spot_publish(spot, "broadcast:alerts", "System maintenance at 2AM", 26);
```

**Subscribers:**
```c
// Each subscriber creates own SPOT instance
slk_spot_t *sub1 = slk_spot_new(ctx);
slk_spot_subscribe(sub1, "broadcast:alerts");

slk_spot_t *sub2 = slk_spot_new(ctx);
slk_spot_subscribe(sub2, "broadcast:alerts");

// All receive the same message
```

---

## Fan-In Pattern (N:1)

**Use Case:** Aggregate data from multiple sources.

### Architecture

```
┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐
│Src 1│  │Src 2│  │Src 3│  │Src 4│
└──┬──┘  └──┬──┘  └──┬──┘  └──┬──┘
   │        │        │        │
   └────────┴────────┴────────┘
                │
                ▼
         ┌─────────────┐
         │ Aggregator  │
         └─────────────┘
```

### Implementation

**Sources:**
```c
// Source 1
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "source1:data");
slk_spot_publish(spot, "source1:data", "123", 3);

// Source 2
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "source2:data");
slk_spot_publish(spot, "source2:data", "456", 3);
```

**Aggregator (Pattern Subscription):**
```c
slk_spot_t *aggregator = slk_spot_new(ctx);
slk_spot_subscribe_pattern(aggregator, "source*:data");

while (1) {
    char topic[256], data[4096];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(aggregator, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, 100);

    if (rc == 0) {
        topic[topic_len] = '\0';
        data[data_len] = '\0';
        printf("Aggregated from %s: %s\n", topic, data);
    }
}
```

---

## Request-Reply Pattern

**Use Case:** Synchronous RPC-style communication.

**Note:** SPOT is designed for pub/sub, not request/reply. For RPC, consider using ServerLink ROUTER/DEALER directly.

### Workaround Implementation

```c
// Client: Publish request with reply-to topic
slk_spot_t *client = slk_spot_new(ctx);
slk_spot_topic_create(client, "reply:client123");
slk_spot_subscribe(client, "reply:client123");

// Send request
char request[256];
snprintf(request, sizeof(request), "GET /user/123 reply:client123");
slk_spot_publish(client, "service:api", request, strlen(request));

// Wait for reply
char topic[256], data[4096];
size_t topic_len, data_len;
slk_spot_recv(client, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, 5000); // 5s timeout
```

**Server:**
```c
slk_spot_t *server = slk_spot_new(ctx);
slk_spot_topic_create(server, "service:api");
slk_spot_subscribe(server, "service:api");

// Process request
char topic[256], data[4096];
size_t topic_len, data_len;
slk_spot_recv(server, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, -1);

// Parse request and extract reply-to topic
char reply_topic[256];
sscanf(data, "GET /user/%*s %s", reply_topic);

// Send reply
slk_spot_topic_route(server, reply_topic, "tcp://client:5556");
slk_spot_publish(server, reply_topic, "{\"id\":123,\"name\":\"John\"}", 24);
```

**Limitations:**
- Manual correlation ID management
- No timeout handling in SPOT
- Better suited for ServerLink ROUTER/DEALER

---

## Event Sourcing

**Use Case:** Store all state changes as immutable events.

### Architecture

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ Command Side │────>│ Event Store  │────>│  Query Side  │
│  (Write)     │     │   (SPOT)     │     │   (Read)     │
└──────────────┘     └──────────────┘     └──────────────┘
```

### Implementation

**Event Store (Publisher):**
```c
typedef struct {
    const char *event_type;
    const char *aggregate_id;
    const char *data;
} event_t;

void publish_event(slk_spot_t *spot, const event_t *event)
{
    char topic[256];
    snprintf(topic, sizeof(topic), "events:%s:%s",
             event->event_type, event->aggregate_id);

    slk_spot_topic_create(spot, topic);
    slk_spot_publish(spot, topic, event->data, strlen(event->data));
}

// Example events
event_t events[] = {
    {"UserCreated", "user123", "{\"name\":\"John\"}"},
    {"UserUpdated", "user123", "{\"email\":\"john@example.com\"}"},
    {"UserDeleted", "user123", "{}"}
};

for (int i = 0; i < 3; i++) {
    publish_event(spot, &events[i]);
}
```

**Event Subscriber (Projections):**
```c
slk_spot_t *subscriber = slk_spot_new(ctx);
slk_spot_subscribe_pattern(subscriber, "events:User*");

// Rebuild state from events
while (1) {
    char topic[256], data[4096];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(subscriber, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, -1);

    if (rc == 0) {
        topic[topic_len] = '\0';
        data[data_len] = '\0';

        // Apply event to projection
        if (strstr(topic, "UserCreated")) {
            // Create user in read model
        } else if (strstr(topic, "UserUpdated")) {
            // Update user in read model
        } else if (strstr(topic, "UserDeleted")) {
            // Delete user from read model
        }
    }
}
```

---

## Stream Processing

**Use Case:** Real-time analytics and metrics.

### Architecture

```
┌────────────┐     ┌────────────┐     ┌────────────┐
│  Sensors   │────>│  Streamer  │────>│ Dashboard  │
│ (Publishers)     │  (Filter)  │     │ (Consumer) │
└────────────┘     └────────────┘     └────────────┘
```

### Implementation

**Sensors (Publishers):**
```c
slk_spot_t *sensor = slk_spot_new(ctx);
slk_spot_topic_create(sensor, "sensor:temperature");

while (1) {
    float temp = read_temperature();
    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%.2f", temp);
    slk_spot_publish(sensor, "sensor:temperature", temp_str, strlen(temp_str));

    slk_sleep(1000); // 1 Hz
}
```

**Stream Processor (Filter/Transform):**
```c
slk_spot_t *processor = slk_spot_new(ctx);
slk_spot_subscribe_pattern(processor, "sensor:*");
slk_spot_topic_create(processor, "alerts:high_temp");

while (1) {
    char topic[256], data[256];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(processor, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, 100);

    if (rc == 0) {
        data[data_len] = '\0';
        float temp = atof(data);

        // Filter: High temperature alert
        if (temp > 30.0) {
            char alert[256];
            snprintf(alert, sizeof(alert), "High temp: %.2f", temp);
            slk_spot_publish(processor, "alerts:high_temp", alert, strlen(alert));
        }
    }
}
```

**Dashboard (Consumer):**
```c
slk_spot_t *dashboard = slk_spot_new(ctx);
slk_spot_subscribe(dashboard, "alerts:high_temp");

while (1) {
    char topic[256], data[256];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(dashboard, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, -1);

    if (rc == 0) {
        data[data_len] = '\0';
        printf("ALERT: %s\n", data);
    }
}
```

---

## Best Practices

1. **Topic Naming Conventions**
   - Use hierarchical names: `domain:entity:action`
   - Example: `game:player:spawn`, `chat:room:message`

2. **Error Handling**
   - Always check return values
   - Use `slk_errno()` for error codes
   - Implement retry logic with exponential backoff

3. **Resource Management**
   - Destroy SPOT instances when done
   - Use `slk_spot_destroy()` to clean up sockets

4. **Performance**
   - Use LOCAL topics for same-process communication
   - Batch publishes when possible
   - Tune HWM for high-throughput scenarios

5. **Testing**
   - Unit test with LOCAL topics
   - Integration test with REMOTE topics
   - Use separate SPOT instances for isolation

---

## See Also

- [API Reference](API.md)
- [Quick Start Guide](QUICK_START.md)
- [Architecture Overview](ARCHITECTURE.md)
- [Clustering Guide](CLUSTERING.md)

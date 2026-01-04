# SPOT PUB/SUB Clustering Guide

Complete guide to deploying multi-node SPOT clusters.

## Table of Contents

1. [Overview](#overview)
2. [Single Node Setup](#single-node-setup)
3. [Two-Node Cluster](#two-node-cluster)
4. [N-Node Mesh Topology](#n-node-mesh-topology)
5. [Hub-Spoke Topology](#hub-spoke-topology)
6. [Failure Handling](#failure-handling)
7. [Network Considerations](#network-considerations)
8. [Monitoring and Observability](#monitoring-and-observability)
9. [Production Deployment](#production-deployment)

---

## Overview

SPOT supports distributed pub/sub across multiple nodes using cluster synchronization.

**Key Concepts:**
- **LOCAL Topics**: Hosted on this node (XPUB bound to inproc)
- **REMOTE Topics**: Hosted on other nodes (routed via TCP)
- **Cluster Sync**: Discovery of remote topics via QUERY/QUERY_RESP
- **Mesh Topology**: All nodes connect to all other nodes
- **Hub-Spoke**: Central hub with spoke nodes

**Prerequisites:**
- ServerLink built with TCP support
- Network connectivity between nodes
- Unique bind endpoints for each node

---

## Single Node Setup

Start with a simple single-node deployment.

### Configuration

```c
#include <serverlink/serverlink.h>

int main()
{
    slk_ctx_t *ctx = slk_ctx_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    // Create LOCAL topics
    slk_spot_topic_create(spot, "game:player:spawn");
    slk_spot_topic_create(spot, "game:player:death");
    slk_spot_topic_create(spot, "chat:lobby");

    // Subscribe to local topics
    slk_spot_subscribe(spot, "game:player:spawn");
    slk_spot_subscribe_pattern(spot, "chat:*");

    // Application logic
    while (1) {
        char topic[256], data[4096];
        size_t topic_len, data_len;

        int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                               data, sizeof(data), &data_len, 100);

        if (rc == 0) {
            // Process message
        }
    }

    slk_spot_destroy(&spot);
    slk_ctx_destroy(ctx);
    return 0;
}
```

**Characteristics:**
- Zero network overhead (inproc only)
- Nanosecond latency
- No failure scenarios
- Ideal for development and testing

---

## Two-Node Cluster

Simplest distributed setup with one publisher and one subscriber node.

### Architecture

```
┌─────────────────────┐         ┌─────────────────────┐
│   Node A (Server)   │         │   Node B (Client)   │
├─────────────────────┤         ├─────────────────────┤
│ Bind: tcp://*:5555  │◄────────┤ Connect to:         │
│                     │         │ tcp://nodeA:5555    │
│ LOCAL topics:       │         │                     │
│ - sensor:temp       │         │ REMOTE topics:      │
│ - sensor:humidity   │         │ - sensor:temp       │
└─────────────────────┘         └─────────────────────┘
```

### Node A (Server)

```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Create LOCAL topics
slk_spot_topic_create(spot, "sensor:temperature");
slk_spot_topic_create(spot, "sensor:humidity");

// Bind to accept cluster connections
slk_spot_bind(spot, "tcp://*:5555");

printf("Node A ready on tcp://*:5555\n");

// Publish sensor data
while (1) {
    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%.1f", read_temperature());
    slk_spot_publish(spot, "sensor:temperature", temp_str, strlen(temp_str));

    slk_sleep(1000); // 1 second
}
```

### Node B (Client)

```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Add Node A to cluster
slk_sleep(100); // Wait for Node A to bind
int rc = slk_spot_cluster_add(spot, "tcp://192.168.1.100:5555");
if (rc != 0) {
    fprintf(stderr, "Failed to add Node A: %s\n", slk_strerror(slk_errno()));
    return -1;
}

// Synchronize to discover topics
rc = slk_spot_cluster_sync(spot, 1000);
if (rc != 0) {
    fprintf(stderr, "Cluster sync failed\n");
}

// Subscribe to REMOTE topic
rc = slk_spot_subscribe(spot, "sensor:temperature");
if (rc != 0) {
    fprintf(stderr, "Subscribe failed: %s\n", slk_strerror(slk_errno()));
}

// Receive sensor data
while (1) {
    char topic[256], data[256];
    size_t topic_len, data_len;

    rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 1000);

    if (rc == 0) {
        topic[topic_len] = '\0';
        data[data_len] = '\0';
        printf("Received: %s = %s\n", topic, data);
    }
}
```

**Key Points:**
- Server binds first, then client connects
- `cluster_sync()` discovers remote topics
- Automatic reconnection on TCP failure

---

## N-Node Mesh Topology

All nodes connect to all other nodes (full mesh).

### Architecture (3 Nodes)

```
        ┌─────────────────┐
        │    Node A       │
        │  tcp://*:5555   │
        └────────┬────────┘
                 │
        ┌────────┼────────┐
        │        │        │
        ▼        ▼        ▼
┌───────────┐ ┌───────────┐
│  Node B   │ │  Node C   │
│tcp://*:5556│ │tcp://*:5557│
└───────┬───┘ └───┬───────┘
        │         │
        └─────────┘
```

**Connections:**
- Node A ↔ Node B
- Node A ↔ Node C
- Node B ↔ Node C

### Node A Configuration

```c
slk_spot_t *spot = slk_spot_new(ctx);

// Create LOCAL topics
slk_spot_topic_create(spot, "nodeA:data");

// Bind for server mode
slk_spot_bind(spot, "tcp://*:5555");

// Connect to other nodes
slk_spot_cluster_add(spot, "tcp://nodeB:5556");
slk_spot_cluster_add(spot, "tcp://nodeC:5557");

// Synchronize to discover topics
slk_spot_cluster_sync(spot, 1000);

// Subscribe to remote topics
slk_spot_subscribe(spot, "nodeB:data");
slk_spot_subscribe(spot, "nodeC:data");
```

### Node B Configuration

```c
slk_spot_t *spot = slk_spot_new(ctx);

// Create LOCAL topics
slk_spot_topic_create(spot, "nodeB:data");

// Bind for server mode
slk_spot_bind(spot, "tcp://*:5556");

// Connect to other nodes
slk_spot_cluster_add(spot, "tcp://nodeA:5555");
slk_spot_cluster_add(spot, "tcp://nodeC:5557");

// Synchronize to discover topics
slk_spot_cluster_sync(spot, 1000);

// Subscribe to remote topics
slk_spot_subscribe(spot, "nodeA:data");
slk_spot_subscribe(spot, "nodeC:data");
```

### Node C Configuration

Similar to Node B, adjust endpoints accordingly.

### Startup Sequence

1. **All nodes bind** to their respective ports (can happen in parallel)
2. **Wait for all binds** to complete (e.g., 100ms delay)
3. **All nodes add cluster peers** via `cluster_add()`
4. **All nodes synchronize** via `cluster_sync(1000)`
5. **Nodes subscribe** to discovered topics

**Complexity:**
- N nodes: N×(N-1)/2 TCP connections
- 10 nodes: 45 connections
- 100 nodes: 4,950 connections

**Use Cases:**
- High availability (no single point of failure)
- Low latency (direct peer-to-peer)
- Small clusters (<10 nodes)

---

## Hub-Spoke Topology

Central hub with spoke nodes connecting to it.

### Architecture

```
                  ┌─────────────┐
                  │  Hub Node   │
                  │tcp://*:5555 │
                  └──────┬──────┘
                         │
       ┌─────────────────┼─────────────────┐
       │                 │                 │
       ▼                 ▼                 ▼
┌──────────┐      ┌──────────┐      ┌──────────┐
│ Spoke 1  │      │ Spoke 2  │      │ Spoke 3  │
│ tcp://*: │      │ tcp://*: │      │ tcp://*: │
│   5556   │      │   5557   │      │   5558   │
└──────────┘      └──────────┘      └──────────┘
```

**Connections:**
- Hub ↔ Spoke 1
- Hub ↔ Spoke 2
- Hub ↔ Spoke 3
- **No** Spoke ↔ Spoke connections

### Hub Node Configuration

```c
slk_spot_t *hub = slk_spot_new(ctx);

// Bind for server mode
slk_spot_bind(hub, "tcp://*:5555");

// Connect to all spokes
slk_spot_cluster_add(hub, "tcp://spoke1:5556");
slk_spot_cluster_add(hub, "tcp://spoke2:5557");
slk_spot_cluster_add(hub, "tcp://spoke3:5558");

// Synchronize to discover spoke topics
slk_spot_cluster_sync(hub, 1000);

// Subscribe to all spoke topics (pattern)
char **topics;
size_t count;
slk_spot_list_topics(hub, &topics, &count);
for (size_t i = 0; i < count; i++) {
    slk_spot_subscribe(hub, topics[i]);
}
slk_spot_list_topics_free(topics, count);

// Forward messages between spokes (relay logic)
// ...
```

### Spoke Node Configuration

```c
slk_spot_t *spoke = slk_spot_new(ctx);

// Create LOCAL topics
slk_spot_topic_create(spoke, "spoke1:sensor:temp");

// Bind for server mode
slk_spot_bind(spoke, "tcp://*:5556");

// Connect ONLY to hub
slk_spot_cluster_add(spoke, "tcp://hub:5555");

// Synchronize to discover hub topics
slk_spot_cluster_sync(spoke, 1000);

// Subscribe to topics of interest
slk_spot_subscribe(spoke, "hub:config");
```

**Message Relay (Hub):**
```c
// Hub receives message from Spoke 1
char topic[256], data[4096];
size_t topic_len, data_len;

int rc = slk_spot_recv(hub, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 100);

if (rc == 0) {
    // Forward to all other spokes
    // (requires explicit publish to remote topics)
    // Note: Current SPOT doesn't support direct forwarding
    // This is a future enhancement
}
```

**Complexity:**
- N spokes: N TCP connections (hub has N connections)
- Scales better than mesh
- Single point of failure (hub)

**Use Cases:**
- Large clusters (>10 nodes)
- Centralized monitoring/logging
- Message brokering

---

## Failure Handling

### Network Partition

**Scenario:** Node B loses connection to Node A.

```
┌─────────┐         ┌─────────┐
│ Node A  │    X    │ Node B  │
└─────────┘         └─────────┘
     │                   │
     │                   │
     ▼                   ▼
┌─────────┐         ┌─────────┐
│ Node C  │◄───────►│ Node D  │
└─────────┘         └─────────┘
```

**Automatic Behavior:**
- Node B's ROUTER socket detects TCP disconnect
- Subscriptions to Node A topics fail with `EHOSTUNREACH`
- Node B attempts automatic reconnection (backoff: 100ms → 5000ms)

**Application Handling:**
```c
int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 100);

if (rc != 0) {
    int err = slk_errno();
    if (err == SLK_EAGAIN) {
        // No message available (normal)
    } else if (err == SLK_EHOSTUNREACH) {
        // Node disconnected, retry or failover
        slk_sleep(1000);
        slk_spot_cluster_sync(spot, 1000); // Re-sync
    }
}
```

---

### Node Failure

**Scenario:** Node A crashes.

**Detection:**
- TCP keepalive detects dead connection (default: 30s)
- SPOT marks node as disconnected
- REMOTE topics from Node A become unavailable

**Recovery:**
```c
// Manual retry after delay
slk_sleep(5000); // Wait 5 seconds

// Re-add cluster node
slk_spot_cluster_remove(spot, "tcp://nodeA:5555"); // Clean up
slk_spot_cluster_add(spot, "tcp://nodeA:5555");    // Reconnect

// Re-sync
if (slk_spot_cluster_sync(spot, 1000) == 0) {
    printf("Node A recovered\n");
}
```

**Failover Example:**
```c
// Primary and backup servers
const char *primary = "tcp://primary:5555";
const char *backup = "tcp://backup:5555";

int rc = slk_spot_topic_route(spot, "critical:topic", primary);
if (rc != 0) {
    // Primary failed, use backup
    rc = slk_spot_topic_route(spot, "critical:topic", backup);
}
```

---

### Split-Brain Scenario

**Scenario:** Cluster splits into two partitions.

```
Partition 1              Partition 2
┌─────────┐              ┌─────────┐
│ Node A  │              │ Node C  │
│ Node B  │              │ Node D  │
└─────────┘              └─────────┘
```

**Current Behavior:**
- No automatic partition detection
- Each partition operates independently
- Potential message loss for REMOTE topics

**Mitigation:**
- Use quorum-based decisions (application-level)
- Implement health checks and monitoring
- Design for eventual consistency

---

## Network Considerations

### Firewall Configuration

**Required Ports:**
```bash
# Node A
iptables -A INPUT -p tcp --dport 5555 -j ACCEPT

# Node B
iptables -A INPUT -p tcp --dport 5556 -j ACCEPT
```

**Docker Networking:**
```yaml
version: '3'
services:
  spot-node-a:
    image: serverlink-spot
    ports:
      - "5555:5555"
    environment:
      - SPOT_BIND=tcp://*:5555
    networks:
      - spot-net

  spot-node-b:
    image: serverlink-spot
    ports:
      - "5556:5556"
    environment:
      - SPOT_BIND=tcp://*:5556
      - SPOT_CLUSTER=tcp://spot-node-a:5555
    networks:
      - spot-net

networks:
  spot-net:
    driver: bridge
```

---

### Latency Optimization

**TCP Tuning:**
```c
// Disable Nagle's algorithm for low latency
int nodelay = 1;
slk_setsockopt(socket, SLK_TCP_NODELAY, &nodelay, sizeof(nodelay));

// Adjust TCP keepalive
int keepalive = 1;
int keepalive_idle = 10;  // 10 seconds
int keepalive_intvl = 5;  // 5 seconds
int keepalive_cnt = 3;    // 3 probes

slk_setsockopt(socket, SLK_TCP_KEEPALIVE, &keepalive, sizeof(keepalive));
slk_setsockopt(socket, SLK_TCP_KEEPALIVE_IDLE, &keepalive_idle, sizeof(keepalive_idle));
slk_setsockopt(socket, SLK_TCP_KEEPALIVE_INTVL, &keepalive_intvl, sizeof(keepalive_intvl));
slk_setsockopt(socket, SLK_TCP_KEEPALIVE_CNT, &keepalive_cnt, sizeof(keepalive_cnt));
```

**Note:** ServerLink does not yet expose `SLK_TCP_NODELAY`, this is a future enhancement.

---

### Bandwidth Management

**HWM Tuning:**
```c
// High-bandwidth scenario
slk_spot_set_hwm(spot, 100000, 100000);

// Low-bandwidth scenario (IoT)
slk_spot_set_hwm(spot, 100, 100);
```

**Message Batching:**
```c
// Instead of many small publishes
for (int i = 0; i < 1000; i++) {
    slk_spot_publish(spot, topic, &data[i], sizeof(data[i]));
}

// Batch into larger message
slk_spot_publish(spot, topic, data, sizeof(data));
```

---

## Monitoring and Observability

### Health Checks

```c
// Check if cluster node is reachable
int is_healthy(slk_spot_t *spot, const char *endpoint)
{
    // Attempt cluster sync with short timeout
    int rc = slk_spot_cluster_sync(spot, 100);
    return (rc == 0) ? 1 : 0;
}
```

### Metrics Collection

```c
// Track publish/subscribe counts
typedef struct {
    uint64_t publishes;
    uint64_t subscribes;
    uint64_t recv_msgs;
    uint64_t recv_bytes;
} spot_metrics_t;

spot_metrics_t metrics = {0};

// Increment on publish
slk_spot_publish(spot, topic, data, len);
metrics.publishes++;

// Increment on recv
int rc = slk_spot_recv(spot, topic, topic_size, &topic_len,
                       data, data_size, &data_len, flags);
if (rc == 0) {
    metrics.recv_msgs++;
    metrics.recv_bytes += data_len;
}
```

### Logging

```c
// Enable verbose logging (application-level)
#define SPOT_LOG_LEVEL_DEBUG 1

void spot_log(int level, const char *fmt, ...)
{
    if (level <= SPOT_LOG_LEVEL_DEBUG) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

// Example usage
spot_log(SPOT_LOG_LEVEL_DEBUG, "Cluster sync: discovered %zu topics\n", count);
```

---

## Production Deployment

### Checklist

- [ ] **Network Security**: Firewalls configured, ports exposed
- [ ] **Monitoring**: Health checks, metrics collection
- [ ] **Logging**: Centralized logging (syslog, ELK)
- [ ] **High Availability**: Redundant nodes, failover logic
- [ ] **Resource Limits**: HWM tuning, memory limits
- [ ] **Testing**: Load testing, failure injection
- [ ] **Documentation**: Topology diagrams, runbooks

### Example Production Config

```c
typedef struct {
    const char *bind_endpoint;
    const char **cluster_peers;
    int peer_count;
    int sndhwm;
    int rcvhwm;
    int sync_timeout_ms;
} spot_config_t;

int spot_init_production(slk_spot_t *spot, const spot_config_t *config)
{
    // Bind to endpoint
    if (slk_spot_bind(spot, config->bind_endpoint) != 0) {
        fprintf(stderr, "Failed to bind to %s\n", config->bind_endpoint);
        return -1;
    }

    // Set HWM
    if (slk_spot_set_hwm(spot, config->sndhwm, config->rcvhwm) != 0) {
        fprintf(stderr, "Failed to set HWM\n");
        return -1;
    }

    // Add cluster peers
    for (int i = 0; i < config->peer_count; i++) {
        int rc = slk_spot_cluster_add(spot, config->cluster_peers[i]);
        if (rc != 0) {
            fprintf(stderr, "Warning: Failed to add peer %s\n",
                    config->cluster_peers[i]);
            // Continue with other peers
        }
    }

    // Initial sync
    if (slk_spot_cluster_sync(spot, config->sync_timeout_ms) != 0) {
        fprintf(stderr, "Warning: Cluster sync incomplete\n");
        // Not fatal, can retry later
    }

    return 0;
}
```

**Usage:**
```c
const char *peers[] = {
    "tcp://node2:5556",
    "tcp://node3:5557"
};

spot_config_t config = {
    .bind_endpoint = "tcp://*:5555",
    .cluster_peers = peers,
    .peer_count = 2,
    .sndhwm = 10000,
    .rcvhwm = 10000,
    .sync_timeout_ms = 1000
};

slk_spot_t *spot = slk_spot_new(ctx);
spot_init_production(spot, &config);
```

---

## See Also

- [API Reference](API.md)
- [Architecture Overview](ARCHITECTURE.md)
- [Protocol Specification](PROTOCOL.md)
- [Usage Patterns](PATTERNS.md)

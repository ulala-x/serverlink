[![English](https://img.shields.io/badge/lang:en-red.svg)](PROTOCOL.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](PROTOCOL.ko.md)

# SPOT PUB/SUB Protocol Specification

Node-to-node communication protocol for ServerLink SPOT.

## Table of Contents

1. [Overview](#overview)
2. [Message Format](#message-format)
3. [Command Codes](#command-codes)
4. [Message Types](#message-types)
5. [Protocol Flows](#protocol-flows)
6. [Error Handling](#error-handling)
7. [Wire Format Examples](#wire-format-examples)

---

## Overview

SPOT uses a binary protocol over ServerLink ROUTER/DEALER sockets for cluster communication.

**Transport:**
- **Inproc**: LOCAL topics (XPUB/XSUB)
- **TCP**: REMOTE topics and cluster protocol (ROUTER)

**Message Framing:**
- Multi-frame messages using ServerLink's SNDMORE flag
- Frame 0: Routing ID (ROUTER messages only)
- Frame 1: Empty delimiter (ROUTER messages only)
- Frame 2+: Protocol-specific frames

**Byte Order:**
- Little-endian for all multi-byte integers
- UTF-8 for all strings

---

## Message Format

### ROUTER Message Envelope

All cluster protocol messages use ROUTER framing:

```
┌────────────────┐
│  Routing ID    │  Frame 0: Variable length (0-255 bytes)
├────────────────┤
│  Empty Frame   │  Frame 1: 0 bytes (delimiter)
├────────────────┤
│  Payload       │  Frame 2+: Protocol-specific
└────────────────┘
```

**Routing ID:**
- Assigned by ROUTER socket
- Used for reply-to addressing
- Opaque binary blob (not null-terminated)

**Empty Frame:**
- Always 0 bytes
- Separates routing envelope from payload
- Required by ROUTER protocol

---

## Command Codes

### Enumeration

```c
enum class spot_command_t : uint8_t {
    PUBLISH      = 0x01,  // Publish message to topic
    SUBSCRIBE    = 0x02,  // Subscribe to topic
    UNSUBSCRIBE  = 0x03,  // Unsubscribe from topic
    QUERY        = 0x04,  // Query local topics
    QUERY_RESP   = 0x05   // Response to QUERY
};
```

### Command Summary

| Code | Name | Direction | Description |
|------|------|-----------|-------------|
| 0x01 | PUBLISH | Client→Server | Publish message to REMOTE topic |
| 0x02 | SUBSCRIBE | Client→Server | Subscribe to REMOTE topic |
| 0x03 | UNSUBSCRIBE | Client→Server | Unsubscribe from REMOTE topic |
| 0x04 | QUERY | Client→Server | Request list of LOCAL topics |
| 0x05 | QUERY_RESP | Server→Client | Response with topic list |

---

## Message Types

### PUBLISH (0x01)

**Purpose:** Publish a message to a REMOTE topic.

**Frame Structure:**
```
Frame 0: Routing ID (variable)
Frame 1: Empty delimiter (0 bytes)
Frame 2: Command (1 byte) = 0x01
Frame 3: Topic ID (variable length string)
Frame 4: Message data (variable length binary)
```

**Example:**
```
Frame 0: [0x12, 0x34, 0x56] (routing_id)
Frame 1: [] (empty)
Frame 2: [0x01] (PUBLISH)
Frame 3: [0x67, 0x61, 0x6D, 0x65, 0x3A, 0x70, 0x31] ("game:p1")
Frame 4: [0x48, 0x65, 0x6C, 0x6C, 0x6F] ("Hello")
```

**C Code:**
```c
// Send PUBLISH
slk_send(socket, routing_id, id_len, SLK_SNDMORE);
slk_send(socket, "", 0, SLK_SNDMORE);
uint8_t cmd = 0x01;
slk_send(socket, &cmd, 1, SLK_SNDMORE);
slk_send(socket, topic_id, strlen(topic_id), SLK_SNDMORE);
slk_send(socket, data, data_len, 0);
```

---

### SUBSCRIBE (0x02)

**Purpose:** Subscribe to a REMOTE topic.

**Frame Structure:**
```
Frame 0: Routing ID (variable)
Frame 1: Empty delimiter (0 bytes)
Frame 2: Command (1 byte) = 0x02
Frame 3: Topic ID (variable length string)
```

**Example:**
```
Frame 0: [0x12, 0x34, 0x56] (routing_id)
Frame 1: [] (empty)
Frame 2: [0x02] (SUBSCRIBE)
Frame 3: [0x67, 0x61, 0x6D, 0x65, 0x3A, 0x70, 0x31] ("game:p1")
```

**C Code:**
```c
// Send SUBSCRIBE
slk_send(socket, routing_id, id_len, SLK_SNDMORE);
slk_send(socket, "", 0, SLK_SNDMORE);
uint8_t cmd = 0x02;
slk_send(socket, &cmd, 1, SLK_SNDMORE);
slk_send(socket, topic_id, strlen(topic_id), 0);
```

---

### UNSUBSCRIBE (0x03)

**Purpose:** Unsubscribe from a REMOTE topic.

**Frame Structure:**
```
Frame 0: Routing ID (variable)
Frame 1: Empty delimiter (0 bytes)
Frame 2: Command (1 byte) = 0x03
Frame 3: Topic ID (variable length string)
```

**Example:**
```
Frame 0: [0x12, 0x34, 0x56] (routing_id)
Frame 1: [] (empty)
Frame 2: [0x03] (UNSUBSCRIBE)
Frame 3: [0x67, 0x61, 0x6D, 0x65, 0x3A, 0x70, 0x31] ("game:p1")
```

**C Code:**
```c
// Send UNSUBSCRIBE
slk_send(socket, routing_id, id_len, SLK_SNDMORE);
slk_send(socket, "", 0, SLK_SNDMORE);
uint8_t cmd = 0x03;
slk_send(socket, &cmd, 1, SLK_SNDMORE);
slk_send(socket, topic_id, strlen(topic_id), 0);
```

---

### QUERY (0x04)

**Purpose:** Request list of LOCAL topics from a cluster node.

**Frame Structure:**
```
Frame 0: Routing ID (variable)
Frame 1: Empty delimiter (0 bytes)
Frame 2: Command (1 byte) = 0x04
```

**Example:**
```
Frame 0: [0x12, 0x34, 0x56] (routing_id)
Frame 1: [] (empty)
Frame 2: [0x04] (QUERY)
```

**C Code:**
```c
// Send QUERY
slk_send(socket, routing_id, id_len, SLK_SNDMORE);
slk_send(socket, "", 0, SLK_SNDMORE);
uint8_t cmd = 0x04;
slk_send(socket, &cmd, 1, 0);
```

---

### QUERY_RESP (0x05)

**Purpose:** Response to QUERY with list of LOCAL topics.

**Frame Structure:**
```
Frame 0: Routing ID (variable)
Frame 1: Empty delimiter (0 bytes)
Frame 2: Command (1 byte) = 0x05
Frame 3: Topic count (4 bytes, uint32_t, little-endian)
Frame 4+: Topic IDs (variable length strings, one per frame)
```

**Example (2 topics):**
```
Frame 0: [0x12, 0x34, 0x56] (routing_id)
Frame 1: [] (empty)
Frame 2: [0x05] (QUERY_RESP)
Frame 3: [0x02, 0x00, 0x00, 0x00] (count = 2)
Frame 4: [0x74, 0x6F, 0x70, 0x69, 0x63, 0x31] ("topic1")
Frame 5: [0x74, 0x6F, 0x70, 0x69, 0x63, 0x32] ("topic2")
```

**C Code:**
```c
// Send QUERY_RESP
slk_send(socket, routing_id, id_len, SLK_SNDMORE);
slk_send(socket, "", 0, SLK_SNDMORE);
uint8_t cmd = 0x05;
slk_send(socket, &cmd, 1, SLK_SNDMORE);
uint32_t count = 2;
slk_send(socket, &count, 4, SLK_SNDMORE);
slk_send(socket, "topic1", 6, SLK_SNDMORE);
slk_send(socket, "topic2", 6, 0);
```

---

## Protocol Flows

### Cluster Synchronization

**Scenario:** Node A discovers topics from Node B.

```
Node A                                Node B
  |                                      |
  | 1. cluster_add("tcp://nodeB:5555")   |
  |─────────────────────────────────────>|
  |         (TCP connect)                |
  |                                      |
  | 2. cluster_sync(1000)                |
  |                                      |
  | ┌─ QUERY ──────────────────────────> |
  | │ [rid][empty][0x04]                 |
  | │                                    |
  | │                     Process QUERY  |
  | │                     Get LOCAL topics|
  | │                                    |
  | │ <──────────────── QUERY_RESP ────┐|
  | │ [rid][empty][0x05][count][topics] ||
  | │                                   ||
  | └─ Register REMOTE topics           ||
  |    (topic1 → tcp://nodeB:5555)      ||
  |    (topic2 → tcp://nodeB:5555)      ||
  |                                     ||
  | Return 0                            ||
  |<─────────────────────────────────────┘|
  |                                      |
```

---

### Remote Topic Publish

**Scenario:** Node A publishes to topic hosted on Node B.

```
Publisher                 Node A (Client)              Node B (Server)
  |                            |                            |
  | slk_spot_publish()         |                            |
  |───────────────────────────>|                            |
  |                            |                            |
  |                            | Lookup: topic is REMOTE   |
  |                            | Find spot_node_t          |
  |                            |                            |
  |                            | ┌─ PUBLISH ──────────────>|
  |                            | │ [rid][empty][0x01]      |
  |                            | │ [topic_id][data]        |
  |                            | │                         |
  |                            | │         Receive on ROUTER|
  |                            | │         Extract topic/data|
  |                            | │         Forward to XPUB  |
  |                            | │         ───────────────> subscribers
  |                            | │                         |
  |                            | └─ (No response)          |
  |                            |                            |
  | Return 0                   |                            |
  |<───────────────────────────|                            |
  |                            |                            |
```

---

### Remote Topic Subscribe

**Scenario:** Node A subscribes to topic hosted on Node B.

```
Subscriber                Node A (Client)              Node B (Server)
  |                            |                            |
  | slk_spot_subscribe()       |                            |
  |───────────────────────────>|                            |
  |                            |                            |
  |                            | Lookup: topic is REMOTE   |
  |                            | Find spot_node_t          |
  |                            |                            |
  |                            | ┌─ SUBSCRIBE ────────────>|
  |                            | │ [rid][empty][0x02]      |
  |                            | │ [topic_id]              |
  |                            | │                         |
  |                            | │         Receive on ROUTER|
  |                            | │         Process SUBSCRIBE|
  |                            | │         (future: register subscription)|
  |                            | │                         |
  |                            | └─ (No response)          |
  |                            |                            |
  | Return 0                   |                            |
  |<───────────────────────────|                            |
  |                            |                            |
  |                            | Now can receive PUBLISH   |
  |                            |<──────────────────────────|
  |                            | [rid][empty][0x01]        |
  |                            | [topic_id][data]          |
  |                            |                            |
```

**Note:** Current implementation does not send SUBSCRIBE to remote node for pattern subscriptions.

---

## Error Handling

### Protocol Errors

**Invalid Command Code:**
```c
// Receiver
if (cmd < 0x01 || cmd > 0x05) {
    errno = EPROTO;
    return -1;
}
```

**Malformed Message:**
```c
// Frame count mismatch
if (frame_count != expected_count) {
    errno = EPROTO;
    return -1;
}

// Invalid data type
if (count_frame_size != sizeof(uint32_t)) {
    errno = EPROTO;
    return -1;
}
```

**Connection Errors:**
```c
// TCP disconnect
if (recv_rc == -1 && errno == ECONNRESET) {
    // Mark node as disconnected
    // Retry connection with backoff
}
```

### Timeout Handling

**QUERY Timeout:**
```c
// cluster_sync() uses non-blocking recv with timeout
int rc = slk_recv(socket, buf, size, SLK_DONTWAIT);
if (rc == -1 && errno == EAGAIN) {
    // No response within timeout
    // Continue with next node
}
```

---

## Wire Format Examples

### Example 1: Simple PUBLISH

**Scenario:** Publish "Hello" to "game:p1"

**Hex Dump:**
```
Frame 0 (Routing ID): 3 bytes
  00 01 02

Frame 1 (Empty): 0 bytes
  (empty)

Frame 2 (Command): 1 byte
  01                            # PUBLISH

Frame 3 (Topic ID): 7 bytes
  67 61 6D 65 3A 70 31          # "game:p1"

Frame 4 (Data): 5 bytes
  48 65 6C 6C 6F                # "Hello"
```

**Total:** 16 bytes (excluding routing overhead)

---

### Example 2: QUERY_RESP with 3 Topics

**Scenario:** Node responds with 3 LOCAL topics

**Hex Dump:**
```
Frame 0 (Routing ID): 3 bytes
  00 01 02

Frame 1 (Empty): 0 bytes
  (empty)

Frame 2 (Command): 1 byte
  05                            # QUERY_RESP

Frame 3 (Topic Count): 4 bytes
  03 00 00 00                   # count = 3 (little-endian)

Frame 4 (Topic 1): 8 bytes
  67 61 6D 65 3A 70 31          # "game:p1"

Frame 5 (Topic 2): 8 bytes
  67 61 6D 65 3A 70 32          # "game:p2"

Frame 6 (Topic 3): 11 bytes
  63 68 61 74 3A 6C 6F 62 62 79 # "chat:lobby"
```

**Total:** 35 bytes (excluding routing overhead)

---

## Version Compatibility

**Current Version:** 1.0

**Future Versioning:**
- Version byte in command frame (reserved for future)
- Backward compatibility via feature negotiation
- Protocol upgrades via QUERY extensions

**Extension Points:**
- Additional command codes (0x06-0xFF)
- Optional frames for metadata
- Topic attributes (TTL, priority, etc.)

---

## Security Considerations

**Current Implementation:**
- No authentication or encryption
- Trust-based cluster membership
- Plain-text topic IDs and data

**Future Enhancements:**
- Topic-level access control
- Encrypted TCP transport (TLS)
- Cluster authentication (shared secret)
- Message signing (HMAC)

---

## Performance Optimizations

### Batching

**Multiple Publishes:**
```c
// Instead of:
for (int i = 0; i < 1000; i++) {
    slk_spot_publish(spot, topic, &data[i], 1);
}

// Consider batching:
slk_spot_publish(spot, topic, data, 1000);
```

### Connection Pooling

**Persistent Connections:**
- SPOT reuses spot_node_t connections
- One TCP connection per remote endpoint
- Automatic reconnection on failure

### Zero-Copy

**LOCAL Topics:**
- inproc transport uses ypipe (zero-copy)
- No serialization overhead
- Direct memory mapping

---

## Debugging Protocol

### Enable Verbose Logging

```c
// Set XPUB_VERBOSE to see subscription messages
int verbose = 1;
slk_setsockopt(xpub_socket, SLK_XPUB_VERBOSE, &verbose, sizeof(verbose));
```

### Packet Capture

**Using tcpdump:**
```bash
# Capture SPOT traffic on port 5555
tcpdump -i any -w spot.pcap port 5555

# Analyze with Wireshark
wireshark spot.pcap
```

**Custom Protocol Dissector:**
- Wireshark Lua dissector (future contribution)
- Parse ROUTER framing and SPOT commands

---

## See Also

- [API Reference](API.md)
- [Architecture Overview](ARCHITECTURE.md)
- [Clustering Guide](CLUSTERING.md)
- [Quick Start](QUICK_START.md)

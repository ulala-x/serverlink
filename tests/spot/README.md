[![English](https://img.shields.io/badge/lang:en-red.svg)](README.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](README.ko.md)

# SPOT PUB/SUB Tests

ServerLink SPOT (Scalable PUB/SUB over Topics) test suite.

## Test Status

**Total Tests: 31 (All Passing)** ✅

| Test File | Test Count | Status |
|-----------|------------|--------|
| test_spot_basic | 11 | ✅ PASS |
| test_spot_local | 6 | ✅ PASS |
| test_spot_remote | 5 | ✅ PASS |
| test_spot_cluster | 4 | ✅ PASS |
| test_spot_mixed | 5 | ✅ PASS |

## Test Organization

### test_spot_basic.cpp (11 tests)

Basic SPOT functionality:
- `test_spot_create_destroy` - Instance lifecycle
- `test_spot_topic_create` - Single topic creation
- `test_spot_topic_create_multiple` - Multiple topic creation
- `test_spot_subscribe` - Basic subscription
- `test_spot_subscribe_multiple` - Multiple subscriptions
- `test_spot_unsubscribe` - Unsubscription
- `test_spot_subscribe_pattern` - Pattern-based subscription
- `test_spot_basic_pubsub` - Basic publish/subscribe
- `test_spot_publish_nonexistent` - Non-existent topic error handling
- `test_spot_multiple_messages` - Message ordering and delivery
- `test_spot_topic_destroy` - Topic cleanup

### test_spot_local.cpp (6 tests)

LOCAL publish/subscribe scenarios:
- `test_spot_multi_topic` - Multiple topics in single instance
- `test_spot_multi_subscriber` - Multiple subscribers to same topic
- `test_spot_pattern_matching` - Pattern-based topic filtering
- `test_spot_selective_unsubscribe` - Selective unsubscription
- `test_spot_large_message` - 1MB message handling
- `test_spot_rapid_pubsub` - High-frequency messaging (100 messages)

### test_spot_remote.cpp (5 tests)

REMOTE communication via TCP/inproc:
- `test_spot_remote_tcp` - Remote pub/sub over TCP
- `test_spot_remote_inproc` - Remote pub/sub over inproc
- `test_spot_bidirectional_remote` - Bidirectional node communication
- `test_spot_reconnect` - Disconnect and reconnect handling
- `test_spot_multiple_remote_subscribers` - Broadcast to multiple remote nodes

### test_spot_cluster.cpp (4 tests)

Multi-node cluster scenarios:
- `test_spot_three_node_cluster` - 3-node full mesh cluster
- `test_spot_topic_sync` - Cross-cluster topic synchronization
- `test_spot_node_failure_recovery` - Node failure and recovery
- `test_spot_dynamic_membership` - Dynamic cluster membership changes

### test_spot_mixed.cpp (5 tests)

LOCAL/REMOTE mixed scenarios:
- `test_spot_mixed_local_remote` - Mixed LOCAL and REMOTE subscribers
- `test_spot_multi_transport` - Multiple transports (TCP + inproc)
- `test_spot_topic_routing_mixed` - Topic routing from mixed sources
- `test_spot_pattern_mixed` - Pattern subscription from mixed sources
- `test_spot_high_load_mixed` - High-load mixed scenario (50 messages)

## Running Tests

### Run All SPOT Tests
```bash
ctest -R spot --output-on-failure
```

### Run Individual Test Files
```bash
./build/tests/Release/test_spot_basic
./build/tests/Release/test_spot_local
./build/tests/Release/test_spot_remote
./build/tests/Release/test_spot_cluster
./build/tests/Release/test_spot_mixed
```

### On Windows
```powershell
.\build\tests\Release\test_spot_basic.exe
.\build\tests\Release\test_spot_local.exe
.\build\tests\Release\test_spot_remote.exe
.\build\tests\Release\test_spot_cluster.exe
.\build\tests\Release\test_spot_mixed.exe
```

## Test Patterns

All tests follow ServerLink testing conventions:
- Use `testutil.hpp` helper macros and functions
- `TEST_ASSERT*` assertion pattern
- Context management for setup/teardown
- Use `test_sleep_ms()` for synchronization
- `SETTLE_TIME` for TCP connection establishment (default 300ms)
- Proper cleanup via `slk_spot_destroy()` and `slk_ctx_destroy()`

## Coverage Matrix

| Feature | Basic | Local | Remote | Cluster | Mixed |
|---------|-------|-------|--------|---------|-------|
| Topic CRUD | ✓ | ✓ | ✓ | ✓ | ✓ |
| Subscribe/Unsubscribe | ✓ | ✓ | ✓ | ✓ | ✓ |
| Pattern Matching | ✓ | ✓ | - | - | ✓ |
| Publish/Receive | ✓ | ✓ | ✓ | ✓ | ✓ |
| Topic Routing | - | ✓ | - | - | ✓ |
| Cluster Management | - | - | ✓ | ✓ | ✓ |
| Multi-Transport | - | - | ✓ | - | ✓ |
| Node Failure | - | - | ✓ | ✓ | - |
| Large Message | - | ✓ | - | - | - |
| High Frequency | - | ✓ | - | - | ✓ |

## Key Test Scenarios

### Pattern Subscription (Prefix Matching)

XPUB/XSUB uses prefix matching:
```c
// "events:*" pattern is converted to "events:" prefix
slk_spot_subscribe_pattern(sub, "events:*");

// Matches all of the following topics:
// - events:login
// - events:logout
// - events:user:created
```

### Multi-Publisher Scenario

```c
// Publishers A and B each bind
slk_spot_bind(pub_a, "tcp://*:5555");
slk_spot_bind(pub_b, "tcp://*:5556");

// Subscriber connects to both
slk_spot_cluster_add(sub, "tcp://...:5555");
slk_spot_cluster_add(sub, "tcp://...:5556");
slk_spot_subscribe_pattern(sub, "events:*");

// Receives messages from both publishers
```

### Dynamic Cluster Membership

```c
// Add node
slk_spot_cluster_add(spot, "tcp://new-node:5555");
slk_spot_subscribe(spot, "topic");

// Remove node (actual disconnection)
slk_spot_cluster_remove(spot, "tcp://old-node:5555");
// No more messages received from that node
```

## Notes

- All tests use temporary TCP ports to avoid conflicts
- Proper cleanup included to prevent resource leaks
- Timing-sensitive tests use configurable `SETTLE_TIME` (default 300ms)
- Large message tests verify 1MB payload handling
- Pattern matching uses XPUB prefix style (`events:*` → `events:`)

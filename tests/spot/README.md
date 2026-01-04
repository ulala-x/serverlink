# SPOT PUB/SUB Tests

Comprehensive test suite for ServerLink SPOT (Scalable PUB/SUB over Topics) implementation.

## Test Organization

### Unit Tests

#### `test_spot_basic.cpp` (267 lines, 11 tests)
Basic SPOT functionality tests:
- `test_spot_create_destroy` - Instance lifecycle
- `test_spot_topic_create` - Single topic creation
- `test_spot_topic_create_multiple` - Multiple topics
- `test_spot_subscribe` - Basic subscription
- `test_spot_subscribe_multiple` - Multiple subscriptions
- `test_spot_unsubscribe` - Unsubscribe functionality
- `test_spot_subscribe_pattern` - Pattern-based subscription
- `test_spot_basic_pubsub` - Basic publish/subscribe roundtrip
- `test_spot_publish_nonexistent` - Error handling for invalid topics
- `test_spot_multiple_messages` - Message ordering and delivery
- `test_spot_topic_destroy` - Topic cleanup

### Integration Tests

#### `test_spot_local.cpp` (373 lines, 6 tests)
Local publish/subscribe scenarios:
- `test_spot_multi_topic` - Multiple topics on single instance
- `test_spot_multi_subscriber` - Multiple subscribers to same topic
- `test_spot_pattern_matching` - Pattern-based topic filtering
- `test_spot_selective_unsubscribe` - Selective topic unsubscription
- `test_spot_large_message` - 1MB message handling
- `test_spot_rapid_pubsub` - High-frequency messaging (100 msgs)

#### `test_spot_remote.cpp` (353 lines, 5 tests)
Remote communication via TCP and inproc:
- `test_spot_remote_tcp` - Remote pub/sub via TCP
- `test_spot_remote_inproc` - Remote pub/sub via inproc
- `test_spot_bidirectional_remote` - Bidirectional node communication
- `test_spot_reconnect` - Disconnection and reconnection handling
- `test_spot_multiple_remote_subscribers` - Broadcast to multiple remote nodes

#### `test_spot_cluster.cpp` (407 lines, 4 tests)
Multi-node cluster scenarios:
- `test_spot_three_node_cluster` - Full mesh 3-node cluster
- `test_spot_topic_sync` - Topic synchronization across cluster
- `test_spot_node_failure_recovery` - Node failure and recovery
- `test_spot_dynamic_membership` - Dynamic cluster membership changes

#### `test_spot_mixed.cpp` (373 lines, 5 tests)
Mixed local/remote scenarios:
- `test_spot_mixed_local_remote` - Local and remote subscribers mixed
- `test_spot_multi_transport` - Multiple transports (TCP + inproc)
- `test_spot_topic_routing_mixed` - Topic routing with mixed sources
- `test_spot_pattern_mixed` - Pattern subscriptions with mixed sources
- `test_spot_high_load_mixed` - High-load mixed scenario (50 msgs)

## Test Summary

**Total Tests**: 31 tests across 5 files
**Total Lines**: 1,773 lines of test code

### Test Categories
- **Basic Functionality**: 11 tests
- **Local Scenarios**: 6 tests
- **Remote Communication**: 5 tests
- **Cluster Operations**: 4 tests
- **Mixed Scenarios**: 5 tests

## Running Tests

### Run all SPOT tests
```bash
make test-spot
# or
ctest -L spot --output-on-failure
```

### Run individual test files
```bash
./build/tests/test_spot_basic
./build/tests/test_spot_local
./build/tests/test_spot_remote
./build/tests/test_spot_cluster
./build/tests/test_spot_mixed
```

### Run specific test within CTest
```bash
ctest -R test_spot_basic -V
```

## Test Patterns Used

All tests follow ServerLink testing conventions:
- Use `testutil.hpp` helper macros and functions
- Follow `TEST_ASSERT*` assertion patterns
- Implement setup/teardown with context management
- Use `test_sleep_ms()` for synchronization
- Use `SETTLE_TIME` for TCP connection establishment
- Proper cleanup with `slk_spot_destroy()` and `slk_ctx_destroy()`

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
| Large Messages | - | ✓ | - | - | - |
| High Frequency | - | ✓ | - | - | ✓ |

## Test Execution Order

Tests are designed to be independent and can run in any order. However, the logical progression is:

1. **Basic** - Verify core SPOT API functionality
2. **Local** - Test local pub/sub scenarios
3. **Remote** - Test remote communication
4. **Cluster** - Test multi-node coordination
5. **Mixed** - Test complex real-world scenarios

## Future Test Additions

Potential areas for additional test coverage:
- [ ] Performance benchmarks (throughput, latency)
- [ ] Thread safety tests (concurrent publishers/subscribers)
- [ ] Error recovery edge cases
- [ ] Topic hierarchy and namespacing
- [ ] Security and authentication
- [ ] QoS levels and message persistence
- [ ] Backpressure and flow control
- [ ] Custom message serialization

## Notes

- All tests use ephemeral TCP ports to avoid conflicts
- Tests include proper cleanup to prevent resource leaks
- Timing-sensitive tests use configurable `SETTLE_TIME` (300ms default)
- Large message test validates 1MB payload handling
- Pattern matching uses glob-style wildcards (`events:*`)

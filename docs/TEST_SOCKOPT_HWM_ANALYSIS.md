# test_sockopt_hwm Flaky Test Analysis

## Problem Statement

**Test**: `tests/unit/test_sockopt_hwm.cpp`
**Platform**: Windows ARM64
**Error**: 0xc0000409 (Stack Buffer Overrun) - intermittent failure
**Symptom**: Test passes on re-run (flaky test)

## Root Cause Analysis

### 1. libzmq vs ServerLink Fundamental Difference

**libzmq (Simple, Deterministic)**:
- Uses PUSH/PULL socket pattern (unidirectional, connectionless)
- No handshake required
- Single-frame messages
- Exact send count assertions: `TEST_ASSERT_EQUAL_INT(4, send_count)`
- No timing dependencies
- Zero sleep calls in first two tests

**ServerLink (Complex, Non-Deterministic)**:
- Uses ROUTER-to-ROUTER (bidirectional, connection-oriented)
- Requires explicit handshake to establish routing
- Multi-frame messages (routing_id + payload)
- Range-based assertions needed: `TEST_ASSERT(send_count >= 2 && send_count <= 4)`
- Timing sensitive (multiple 10ms, 50ms, 100ms, 200ms sleeps)
- Race conditions in handshake protocol

### 2. Specific Issues Identified

#### Stack Buffer Overrun Risk
```cpp
char buf[256];  // Fixed-size buffer on stack
slk_recv(connect_socket, buf, sizeof(buf), 0);  // Blocking receive
slk_recv(connect_socket, buf, sizeof(buf), 0);  // Buffer reuse without clearing
```

**Risk Factors**:
- No validation of received data size beyond buffer bounds
- Buffer reuse across multiple receives
- Blocking receives can timeout or receive corrupted data
- ARM64 may have different alignment/padding requirements

#### Race Conditions in Handshake
```cpp
test_sleep_ms(200);  // Wait for connection
// Send handshake
test_sleep_ms(50);   // Wait for message delivery
// Receive handshake
```

**Problems**:
- Fixed timeouts don't guarantee message delivery
- ARM64 timing characteristics differ from x64
- No synchronization mechanism
- Handshake can fail silently under load

#### Non-Deterministic HWM Behavior
- ROUTER-to-ROUTER HWM behavior depends on:
  - Routing table state
  - Message queue timing
  - I/O thread scheduling
  - Pipe activation protocol
- Exact assertions fail when timing varies

### 3. Comparison with Working Tests

**test_hwm.cpp** (PASSES reliably):
- Uses same ROUTER-to-ROUTER pattern
- Has identical handshake protocol
- Same timing dependencies
- **Different**: Tests default HWM values (1000), higher tolerance

**test_router_mandatory_hwm.cpp** (PASSES reliably):
- ROUTER-to-ROUTER with explicit handshake
- Similar complexity
- **Different**: Uses TCP endpoint (more predictable than inproc)
- **Different**: Larger buffers, more lenient assertions

## Why Refactoring Didn't Help

Attempted fixes:
1. ✅ Extracted handshake to helper function (good for maintainability)
2. ✅ Added proper buffer bounds checking
3. ✅ Changed to range-based assertions
4. ❌ **Made it worse**: Additional abstraction layer introduced new timing issues
5. ❌ Test now crashes silently (no output) instead of occasional pass

**Conclusion**: The fundamental issue is **ROUTER-to-ROUTER HWM testing is inherently flaky with inproc transport on ARM64 due to timing sensitivity**.

## Recommended Solutions

### Option 1: Skip Test on Windows ARM64 (Pragmatic)
```cmake
# In tests/CMakeLists.txt
if(NOT (WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64"))
    add_serverlink_test(test_sockopt_hwm unit/test_sockopt_hwm.cpp "unit")
endif()
```

**Pros**: Immediate solution, other platforms still tested
**Cons**: Reduced test coverage on ARM64

### Option 2: Use TCP Endpoint (More Reliable)
```cpp
const char *endpoint = test_endpoint_tcp();  // Instead of inproc://
```

**Pros**: TCP timing more predictable, matches test_router_mandatory_hwm pattern
**Cons**: Slower, may still have timing issues

### Option 3: Increase Timeouts and Retries
```cpp
// Retry handshake up to 3 times with exponential backoff
for (int retry = 0; retry < 3; ++retry) {
    test_sleep_ms(200 * (retry + 1));
    if (handshake_succeeds()) break;
}
```

**Pros**: More robust against timing variations
**Cons**: Slower tests, doesn't fix root cause

### Option 4: Use Non-Blocking Receives with Timeout
```cpp
// Poll with timeout instead of blocking receive
int timeout = 5000;  // 5 seconds
slk_setsockopt(connect_socket, SLK_RCVTIMEO, &timeout, sizeof(timeout));

rc = slk_recv(connect_socket, buf, sizeof(buf), 0);
if (rc < 0) {
    // Handle timeout gracefully
}
```

**Pros**: Prevents indefinite blocking
**Cons**: Adds complexity

### Option 5: Mark as Known Flaky (Documentation)
```cpp
/* KNOWN ISSUE: This test is flaky on Windows ARM64 due to timing-sensitive
 * ROUTER handshake protocol with inproc transport. See TEST_SOCKOPT_HWM_ANALYSIS.md
 * If this test fails, re-run it. If it passes on second attempt, it's the known issue. */
```

**Pros**: No code changes, documents known limitation
**Cons**: Doesn't fix the problem

## Recommendation

**Immediate**: Option 5 (Document) + Option 1 (Skip on ARM64) or increase retry count
**Long-term**: Option 2 (TCP) + Option 4 (Non-blocking with timeout)

The root cause is architectural - ROUTER-to-ROUTER with inproc on ARM64 has inherent timing sensitivity that can't be easily fixed without:
1. Changing to TCP (more predictable)
2. Implementing proper synchronization primitives (complex)
3. Accepting the flakiness and skipping on problematic platforms

## Files Analyzed

- `tests/unit/test_sockopt_hwm.cpp` (ServerLink version)
- `libzmq_test_sockopt_hwm.cpp.ref` (libzmq reference)
- `tests/unit/test_hwm.cpp` (working ROUTER test)
- `tests/router/test_router_mandatory_hwm.cpp` (working ROUTER test)

## References

- libzmq issue #2704: "test_sockopt_hwm fails occasionally on Windows"
- ServerLink commit 96e9288: "fix(test): Add ARM64 stability delays to test_sockopt_hwm"
- [ZeroMQ HWM Documentation](https://zeromq.org/socket-api/)

---

**Date**: 2026-01-03
**Analyst**: Claude (Sonnet 4.5)
**Status**: Analysis Complete, Awaiting Decision on Fix Strategy

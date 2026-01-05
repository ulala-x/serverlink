# IOCP Phase 3 Implementation Complete

## Overview
Implemented the core IOCP event loop with asynchronous I/O operations for Windows ServerLink.

## Implemented Functions

### 1. `loop()` - Main Event Loop
**Location**: `src/io/iocp.cpp:284-362`

**Key Features**:
- Uses `GetQueuedCompletionStatusEx` for batch completion processing (up to 256 events)
- Timer execution integrated with event loop
- Graceful shutdown via `SHUTDOWN_KEY` completion packet
- Error classification and handling for each completion
- Periodic cleanup of retired entries

**Pattern**:
```cpp
OVERLAPPED_ENTRY entries[MAX_COMPLETIONS];
while (!_stopping) {
    timeout = execute_timers();
    GetQueuedCompletionStatusEx(_iocp, entries, MAX_COMPLETIONS, &count, timeout, FALSE);

    for each entry:
        - Check for shutdown signal
        - Extract iocp_entry and overlapped_ex
        - Get error status via GetOverlappedResult
        - Route to handle_read_completion or handle_write_completion

    cleanup_retired();
}
```

### 2. `start_async_recv()` - Initiate Async Read
**Location**: `src/io/iocp.cpp:364-420`

**Key Features**:
- Atomic pending flag check to prevent duplicate operations
- Retired entry check before starting I/O
- OVERLAPPED structure reset (preserves type, socket, entry)
- WSARecv with completion port notification
- Immediate error handling with classification
- Pending count tracking for safe cleanup

**Error Handling**:
- `WSA_IO_PENDING`: Normal async operation, increment pending count
- `RETRY`: Temporary error, skip and retry later
- `CLOSE/FATAL`: Notify via `in_event()` callback

### 3. `start_async_send()` - Initiate Async Write
**Location**: `src/io/iocp.cpp:422-478`

**Key Features**:
- Mirror of `start_async_recv()` for write operations
- WSASend with dummy buffer (actual data sent via socket API)
- Same pending flag and error handling pattern
- Out event notification on errors

**Note**: IOCP write operations use a probe buffer to detect write readiness. Actual data transmission happens through normal socket send() calls.

### 4. `handle_read_completion()` - Process Read Completion
**Location**: `src/io/iocp.cpp:480-523`

**Key Features**:
- Clear pending flag atomically
- Decrement pending count for cleanup coordination
- Check retired/cancelled state before processing
- Error classification:
  - `IGNORE`: Success, call `in_event()`, restart if `want_pollin`
  - `RETRY`: Temporary error, restart if interested
  - `CLOSE/FATAL`: Error notification via `in_event()`

**Restart Logic**: If the socket still has `want_pollin` set and not retired, automatically start next async recv.

### 5. `handle_write_completion()` - Process Write Completion
**Location**: `src/io/iocp.cpp:525-568`

**Key Features**:
- Identical pattern to read completion
- Uses `out_event()` callback
- Restart logic based on `want_pollout` flag

### 6. `cleanup_retired()` - Safe Entry Cleanup
**Location**: `src/io/iocp.cpp:570-590`

**Key Features**:
- Only deletes entries with `pending_count == 0`
- Keeps entries with pending I/O in retired list
- CancelIoEx already called in `rm_fd()`, so cancelled operations will complete with `ERROR_OPERATION_ABORTED`

**Safety**: Prevents use-after-free by waiting for all I/O completions before deletion.

### 7. `rm_fd()` Enhancement - Add CancelIoEx
**Location**: `src/io/iocp.cpp:189-215`

**Key Changes**:
- Added `CancelIoEx(fd, NULL)` to cancel all pending I/O
- Cancelled operations complete with `ERROR_OPERATION_ABORTED`
- Entry moved to retired list immediately
- Actual deletion deferred to `cleanup_retired()`

## Core Patterns

### 1. Shutdown Handling
```cpp
// In stop()
PostQueuedCompletionStatus(_iocp, 0, SHUTDOWN_KEY, NULL);

// In loop()
if (entry.lpCompletionKey == SHUTDOWN_KEY) {
    _stopping = true;
    break;
}
```

### 2. Error Classification
```cpp
iocp_error_action action = classify_error(error);

switch (action) {
    case IGNORE:  // Success, proceed normally
    case RETRY:   // Temporary error, retry later
    case CLOSE:   // Connection error, notify handler
    case FATAL:   // Programming error, notify handler
}
```

### 3. Safe Entry Deletion
```cpp
// rm_fd() - Mark retired and cancel I/O
entry->retired.store(true);
CancelIoEx(fd, NULL);
_retired.push_back(entry);

// cleanup_retired() - Delete when safe
if (entry->pending_count == 0) {
    delete entry;
}
```

### 4. Pending I/O Tracking
```cpp
// Start operation
ovl->pending.compare_exchange_strong(false, true);
entry->pending_count.fetch_add(1);

// Complete operation
ovl->pending.store(false);
entry->pending_count.fetch_sub(1);

// Check if safe to delete
if (pending_count == 0) { ... }
```

## Thread Safety

All atomic operations use proper memory ordering:
- `memory_order_acq_rel` for CAS operations
- `memory_order_acquire` for reading shared state
- `memory_order_release` for publishing state changes
- `memory_order_relaxed` for non-synchronizing updates

## Error Extraction from OVERLAPPED

For `GetQueuedCompletionStatusEx`, we use `GetOverlappedResult()` to extract detailed error status:

```cpp
DWORD error = ERROR_SUCCESS;
if (bytes == 0 || ovl->Internal != 0) {
    BOOL result = GetOverlappedResult(socket, ovl, &ovl_bytes, FALSE);
    if (!result) {
        error = GetLastError();
    }
}
```

This approach is more robust than directly interpreting `OVERLAPPED.Internal` which contains NTSTATUS codes.

## Testing Recommendations

1. **Basic I/O**: Test TCP read/write operations
2. **Shutdown**: Verify graceful shutdown with `PostQueuedCompletionStatus(SHUTDOWN_KEY)`
3. **Cleanup**: Test socket removal during active I/O
4. **Error Handling**: Simulate network errors (disconnect, reset, timeout)
5. **High Load**: Stress test with many concurrent connections

## Performance Characteristics

- **Batch Processing**: Up to 256 completions per `GetQueuedCompletionStatusEx` call
- **Zero-Copy**: Direct buffer access through OVERLAPPED structures
- **Async Scalability**: Non-blocking I/O scales to thousands of connections
- **Efficient Cleanup**: Deferred deletion prevents I/O thread blocking

## Next Steps

1. Integrate with existing ServerLink socket types (ROUTER, PUB/SUB, DEALER, REP)
2. Test inproc vs TCP performance on Windows
3. Compare IOCP vs select() performance benchmarks
4. Add IOCP-specific unit tests
5. Validate thread safety under high concurrency

## References

- Windows IOCP Documentation: https://docs.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports
- libzmq IOCP implementation patterns
- ServerLink select.cpp and epoll.cpp for event loop patterns

---

**Implementation Date**: 2026-01-05
**Status**: Core event loop complete, ready for integration testing

# Phase 6: ConnectEx Asynchronous Connection Implementation

**Date:** 2026-01-05
**Status:** ✅ Implementation Complete

## Overview

Implemented Windows IOCP ConnectEx for asynchronous TCP connection establishment in ServerLink, following the same pattern as Phase 5's AcceptEx implementation.

## Key Components Modified

### 1. Interface Layer (`i_poll_events.hpp`)

Added `connect_completed()` virtual function for ConnectEx completion notification:

```cpp
#ifdef SL_USE_IOCP
virtual void connect_completed (int error_)
{
    // Default: forward to out_event() on success
    if (error_ == 0) {
        out_event ();
    }
}
#endif
```

**Design Rationale:**
- Default implementation provides backward compatibility
- Connecter classes override for direct ConnectEx handling
- Follows same pattern as `accept_completed()`

### 2. IO Object Layer (`io_object.hpp/cpp`)

Added `enable_connect()` wrapper function:

```cpp
#ifdef SL_USE_IOCP
void enable_connect (handle_t handle_, const struct sockaddr *addr_,
                     int addrlen_);
#endif
```

**Implementation:**
- Delegates to poller's `enable_connect()`
- Maintains abstraction between io_object and poller

### 3. IOCP Poller (`iocp.hpp/cpp`)

#### Data Structures

**overlapped_ex_t** - Added ConnectEx fields:
```cpp
// ConnectEx 전용 필드
sockaddr_storage remote_addr;  // ConnectEx 대상 주소
int remote_addrlen;            // 주소 길이
```

**iocp_entry_t** - Added connect OVERLAPPED:
```cpp
overlapped_ptr connect_ovl;  // ConnectEx 전용 OVERLAPPED
```

**iocp_t** - Added ConnectEx function pointer:
```cpp
LPFN_CONNECTEX _connectex_fn;  // ConnectEx 함수 포인터 (동적 로드)
```

#### Key Functions

**enable_connect()** - ConnectEx initialization:
```cpp
void iocp_t::enable_connect (handle_t handle_, const struct sockaddr *addr_,
                             int addrlen_)
{
    // 1. Load ConnectEx function pointer (first time only)
    if (!_connectex_fn) {
        GUID guid = WSAID_CONNECTEX;
        WSAIoctl(entry->fd, SIO_GET_EXTENSION_FUNCTION_POINTER, ...);
    }

    // 2. Store remote address in OVERLAPPED structure
    memcpy(&ovl->remote_addr, addr_, addrlen_);

    // 3. Start async connect
    start_async_connect(entry);
}
```

**start_async_connect()** - Initiate ConnectEx:
```cpp
void iocp_t::start_async_connect (iocp_entry_t *entry_)
{
    // ⚠️ ConnectEx requires prior bind()!
    // Socket must be bound before calling ConnectEx

    BOOL ok = _connectex_fn(entry_->fd,
                            (const sockaddr*)&ovl->remote_addr,
                            ovl->remote_addrlen,
                            NULL, 0,  // No send data
                            &bytes,
                            ovl);

    // Handle immediate success or WSA_IO_PENDING
}
```

**handle_connect_completion()** - Process completion:
```cpp
void iocp_t::handle_connect_completion (iocp_entry_t *entry_, DWORD error_)
{
    if (action == iocp_error_action::IGNORE) {
        // ✅ Success - SO_UPDATE_CONNECT_CONTEXT is MANDATORY!
        setsockopt(entry_->fd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT,
                   NULL, 0);

        // Notify success
        entry_->events->connect_completed(0);
    } else {
        // Notify error for reconnection handling
        entry_->events->connect_completed(static_cast<int>(error_));
    }
}
```

**IOCP Event Loop** - Added OP_CONNECT handling:
```cpp
// In loop():
else if (ovl->type == overlapped_ex_t::OP_CONNECT) {
    handle_connect_completion(iocp_entry, error);
}
```

### 4. TCP Connecter (`tcp_connecter.hpp/cpp`)

#### Header Changes

Added `connect_completed()` override:
```cpp
#ifdef SL_USE_IOCP
void connect_completed (int error_) override;
#endif
```

#### Implementation Changes

**start_connecting()** - IOCP mode handling:
```cpp
#ifdef SL_USE_IOCP
    // ⚠️ ConnectEx REQUIRES bind() before calling!
    if (!tcp_addr->has_src_addr()) {
        // Bind to INADDR_ANY if no source address specified
        sockaddr_in6 local_addr{};
        local_addr.sin6_family = AF_INET6;
        local_addr.sin6_addr = in6addr_any;
        local_addr.sin6_port = 0;  // any port
        ::bind(_s, (sockaddr*)&local_addr, sizeof(local_addr));
    }

    // Start async ConnectEx
    enable_connect(_handle, tcp_addr->addr(), tcp_addr->addrlen());
#else
    // Traditional select/epoll/kqueue mode
    set_pollout(_handle);
#endif
```

**connect_completed()** - ConnectEx completion handler:
```cpp
void tcp_connecter_t::connect_completed (int error_)
{
    // 1. Cancel connect timer
    cancel_timer(connect_timer_id);

    // 2. Remove from poller
    rm_handle();

    // 3. Handle error
    if (error_ != 0) {
        errno = wsa_error_to_errno(error_);

        // Check reconnect-stop-on-refuse option
        if ((options.reconnect_stop & SL_RECONNECT_STOP_CONN_REFUSED) &&
            errno == ECONNREFUSED) {
            close();
            terminate();
            return;
        }

        // Reconnect on error
        close();
        add_reconnect_timer();
        return;
    }

    // 4. Success - SO_UPDATE_CONNECT_CONTEXT already done by IOCP
    const fd_t fd = _s;
    _s = retired_fd;

    // 5. Tune socket and create engine
    if (!tune_socket(fd)) {
        closesocket(fd);
        add_reconnect_timer();
        return;
    }

    create_engine(fd, get_socket_name<tcp_address_t>(fd, socket_end_local));
}
```

## Critical Requirements

### ⚠️ ConnectEx Mandatory Requirements

1. **bind() MUST be called before ConnectEx**
   - ConnectEx fails with WSAEINVAL if socket is not bound
   - Bind to `INADDR_ANY:0` if no specific source address needed
   - This is different from traditional connect() which doesn't require bind

2. **SO_UPDATE_CONNECT_CONTEXT MUST be called after success**
   - Required to update socket context after ConnectEx completes
   - Without this, functions like `getpeername()` will fail
   - Must be called BEFORE using the socket for I/O

3. **Exponential Backoff for Reconnection**
   - Use existing `add_reconnect_timer()` infrastructure
   - Respects `reconnect_ivl` and `reconnect_ivl_max` options
   - Handles `reconnect_stop` flags (e.g., CONN_REFUSED)

## Error Handling

### Connection Errors

**WSAECONNREFUSED** → `ECONNREFUSED`
- Check `SL_RECONNECT_STOP_CONN_REFUSED` flag
- Terminate or retry based on options

**WSAENETUNREACH** → `ENETUNREACH`
- Network unreachable - retry with backoff

**WSAETIMEDOUT** → `ETIMEDOUT`
- Connection timeout - handled by connect timer

### Critical Errors (Immediate Failure)

- **WSAENOTSOCK** - Invalid socket descriptor
- **WSAEINVAL** - Socket not bound (programming error)
- **WSAEFAULT** - Invalid address pointer

## Architecture Patterns

### Proactor Pattern (IOCP)
```
User Code → enable_connect() → ConnectEx() → IOCP Queue
                                               ↓
                                    GetQueuedCompletionStatusEx()
                                               ↓
                                    handle_connect_completion()
                                               ↓
                                    connect_completed() → User Handler
```

### Reactor Pattern (select/epoll) - Unchanged
```
User Code → connect() → EINPROGRESS → set_pollout() → poll/select
                                                         ↓
                                                     out_event()
                                                         ↓
                                                   getsockopt(SO_ERROR)
                                                         ↓
                                                   User Handler
```

## Platform Compatibility

### Windows IOCP Mode
- ✅ Uses ConnectEx for true asynchronous connect
- ✅ No blocking on connection establishment
- ✅ Integrated with IOCP completion port

### Other Platforms (Unchanged)
- ✅ Linux: epoll + EINPROGRESS
- ✅ BSD/macOS: kqueue + EINPROGRESS
- ✅ Fallback: select + EINPROGRESS

## Testing Strategy

### Test Coverage Required

1. **Basic Connection Tests**
   - `test_router_basic` - Basic ROUTER connection
   - `test_router_to_router` - Router-to-router connection
   - `test_connect_rid` - CONNECT_ROUTING_ID option

2. **Error Handling Tests**
   - Connection refused scenarios
   - Network unreachable
   - Connection timeout
   - Invalid addresses

3. **Reconnection Tests**
   - `test_reconnect_ivl` - Reconnection interval
   - Exponential backoff validation
   - Stop-on-refuse flag

4. **Platform-Specific Tests**
   - IOCP mode on Windows
   - select/epoll fallback paths
   - Cross-platform compatibility

### Expected Test Results

All existing 47 core tests should pass without modification:
- ROUTER tests (8/8)
- PUB/SUB tests (12/12)
- Transport tests (4/4)
- Unit tests (11/11)
- Integration tests (1/1)

## Performance Characteristics

### IOCP Mode Benefits

1. **True Asynchronous Operation**
   - No thread blocking on connection attempts
   - Scalable to thousands of simultaneous connections
   - Efficient CPU utilization

2. **Zero Polling Overhead**
   - No periodic socket checking
   - Event-driven completion notification
   - Lower context switching

3. **Integration with Accept/Send/Recv**
   - Unified IOCP completion port
   - Consistent async I/O model
   - Simplified state management

## Related Documentation

- `docs/PHASE5_ACCEPTEX_IMPLEMENTATION.md` - AcceptEx pattern reference
- `docs/impl/WINDOWS_FDSET_OPTIMIZATION.md` - Windows optimization details
- `CLAUDE.md` - Project status and testing

## Implementation Checklist

- [x] Add `connect_completed()` to `i_poll_events.hpp`
- [x] Add `enable_connect()` to `io_object.hpp/cpp`
- [x] Add ConnectEx support to `iocp.hpp/cpp`
  - [x] Function pointer loading
  - [x] OVERLAPPED structure fields
  - [x] `enable_connect()` implementation
  - [x] `start_async_connect()` implementation
  - [x] `handle_connect_completion()` implementation
  - [x] OP_CONNECT event loop handling
- [x] Modify `tcp_connecter.hpp/cpp`
  - [x] Add `connect_completed()` override
  - [x] Update `start_connecting()` for IOCP
  - [x] Implement ConnectEx-specific logic
  - [x] Handle bind() requirement
  - [x] Handle SO_UPDATE_CONNECT_CONTEXT
- [ ] Build and test verification
  - [ ] Windows Debug build
  - [ ] Windows Release build
  - [ ] Run existing test suite (47 tests)
  - [ ] Verify ROUTER connection tests
  - [ ] Verify reconnection behavior

## Next Steps

1. **Build Verification**
   - Build Debug and Release configurations
   - Verify no compilation errors
   - Check for warnings

2. **Testing**
   - Run all 47 core tests
   - Run ROUTER-specific tests
   - Run reconnection tests
   - Performance benchmarking

3. **Documentation**
   - Update CLAUDE.md with Phase 6 completion
   - Add ConnectEx to API documentation
   - Update architecture diagrams

## Notes

- Implementation follows libzmq patterns for compatibility
- Maintains backward compatibility with select mode
- ConnectEx function pointer is loaded once per context
- Error handling uses existing `classify_error()` infrastructure
- Reconnection uses existing exponential backoff algorithm

---

**Implementation Status:** ✅ Code Complete, Awaiting Build/Test Verification
**Compatibility:** Windows IOCP only (other platforms use existing code paths)
**Breaking Changes:** None (backward compatible)

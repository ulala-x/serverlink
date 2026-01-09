# Windows IOCP Support for ServerLink

## Overview

ServerLink now includes Windows-optimized I/O polling support through the **wepoll** (Windows Event Polling) implementation. This provides significantly better performance than plain `select()` on Windows platforms.

## Implementation Details

### What is wepoll?

The `wepoll` implementation is a **WSAEventSelect-based poller** that provides:

1. **Better scalability** than select() - No FD_SETSIZE limitation (64 sockets)
2. **Efficient event notification** through Windows event objects
3. **Native Windows integration** using WSAEventSelect and WSAWaitForMultipleEvents
4. **Compatible interface** with epoll/kqueue for seamless cross-platform development

### Why Not True IOCP?

Windows I/O Completion Ports (IOCP) is a powerful asynchronous I/O mechanism, but implementing true IOCP would require:

- **Complete rewrite of the I/O model** - IOCP requires overlapped I/O operations
- **Async-first architecture** - All socket operations must be asynchronous
- **Significant API changes** - Different programming model incompatible with current design

Instead, we implemented **WSAEventSelect-based polling**, which:
- Maintains compatibility with libzmq's synchronous I/O model
- Provides much better performance than select() on Windows
- Uses the same poller interface as epoll/kqueue
- Requires minimal changes to existing codebase

This approach is consistent with **libzmq's Windows implementation**, which also uses WSAEventSelect rather than true IOCP.

## Architecture

### File Structure

```
src/io/
├── wepoll.hpp        # Windows event poller header
├── wepoll.cpp        # WSAEventSelect implementation
├── poller.hpp        # Platform-agnostic poller selector
├── epoll.cpp         # Linux epoll implementation
├── kqueue.cpp        # BSD/macOS kqueue implementation
└── select.cpp        # Cross-platform select fallback
```

### Key Components

#### 1. poll_entry_t Structure
```cpp
struct poll_entry_t {
    fd_t fd;                      // Socket file descriptor
    WSAEVENT event;               // Windows event object
    slk::i_poll_events *events;   // Event callback interface
    bool pollin;                  // Monitor read events
    bool pollout;                 // Monitor write events
};
```

#### 2. Event Loop Architecture
```
┌─────────────────────────────────────────────────────┐
│                   wepoll_t::loop()                  │
├─────────────────────────────────────────────────────┤
│  1. Execute due timers                              │
│  2. Collect active (non-retired) poll entries       │
│  3. Process sockets in batches (max 64 per batch)   │
│     ┌─────────────────────────────────────────────┐ │
│     │ WSAEventSelect(fd, event, FD_READ|FD_WRITE) │ │
│     │ WSAWaitForMultipleEvents(events[], timeout) │ │
│     │ WSAEnumNetworkEvents(fd, event, &results)   │ │
│     └─────────────────────────────────────────────┘ │
│  4. Process signaled events (in_event/out_event)    │
│  5. Clean up retired entries                        │
└─────────────────────────────────────────────────────┘
```

#### 3. Event Processing Priority
```
1. Error/Close events  → in_event() for cleanup
2. Write events        → out_event() (FD_WRITE, FD_CONNECT)
3. Read events         → in_event() (FD_READ, FD_ACCEPT)
```

### Windows Event Types Mapped

| WSA Event      | Mapped to      | Condition                   |
|----------------|----------------|-----------------------------|
| FD_READ        | in_event()     | Data available to read      |
| FD_WRITE       | out_event()    | Socket ready for write      |
| FD_ACCEPT      | in_event()     | Incoming connection         |
| FD_CONNECT     | out_event()    | Connection established      |
| FD_CLOSE       | in_event()     | Connection closed           |
| Error codes    | in_event()     | Any network error           |

## Performance Characteristics

### Scalability Limits

- **Maximum events per wait**: 64 (MAXIMUM_WAIT_OBJECTS Windows limit)
- **Solution**: Batching - processes sockets in groups of 64
- **No hard socket limit**: Only limited by system resources

### Performance Advantages over select()

1. **No FD_SETSIZE limitation** - Can handle thousands of sockets
2. **Event-driven waiting** - More efficient than polling fd_sets
3. **Native Windows API** - Better OS integration
4. **Reduced memory copying** - Event objects vs fd_set structures

### Performance Comparison

| Poller Type    | Max Sockets | CPU Efficiency | Memory Overhead |
|----------------|-------------|----------------|-----------------|
| wepoll         | ~64K        | High           | Low             |
| select         | 64          | Medium         | Medium          |
| epoll (Linux)  | ~1M         | Very High      | Very Low        |
| kqueue (BSD)   | ~1M         | Very High      | Very Low        |

## Platform Detection

### CMake Configuration

```cmake
# cmake/platform.cmake
if(WIN32)
    set(SL_HAVE_WEPOLL 1)
    message(STATUS "Windows event polling (wepoll) support detected")
endif()

# Poller priority: wepoll > epoll > kqueue > select
if(SL_HAVE_WEPOLL)
    set(SL_POLLER_NAME "wepoll")
elseif(SL_HAVE_EPOLL)
    set(SL_POLLER_NAME "epoll")
...
```

### Compile-Time Selection

```cpp
// src/io/poller.hpp
#if defined SL_USE_WEPOLL
    #include "wepoll.hpp"
#elif defined SL_USE_EPOLL
    #include "epoll.hpp"
#elif defined SL_USE_KQUEUE
    #include "kqueue.hpp"
#else
    #include "select.hpp"
#endif
```

## Building on Windows

### Prerequisites

```bash
# Install Visual Studio 2019 or later with C++ tools
# Or MinGW-w64 with GCC 10+

# CMake 3.14 or later
cmake --version
```

### Build Commands

```powershell
# Configure with wepoll (automatic on Windows)
cmake -B build -S . -G "Visual Studio 16 2019" -A x64
# Or with MinGW
cmake -B build -S . -G "MinGW Makefiles"

# Build
cmake --build build --config Release

# Verify wepoll is enabled
# Look for: "Windows event polling (wepoll) support detected"
# And: "I/O Poller: wepoll"
```

### Disabling wepoll (Use select instead)

```cmake
# Not recommended, but possible for testing
cmake -B build -S . -DSL_HAVE_WEPOLL=0
```

## API Compatibility

The wepoll implementation provides the **exact same interface** as other pollers:

```cpp
class wepoll_t final : public worker_poller_base_t {
public:
    typedef void *handle_t;

    // Standard poller interface
    handle_t add_fd(fd_t fd_, i_poll_events *events_);
    void rm_fd(handle_t handle_);
    void set_pollin(handle_t handle_);
    void reset_pollin(handle_t handle_);
    void set_pollout(handle_t handle_);
    void reset_pollout(handle_t handle_);
    void stop();

    static int max_fds();
};
```

## Error Handling

### Windows-Specific Errors

```cpp
// wsa_assert macro for Windows API calls
#define wsa_assert(x) \
    do { \
        if (!(x)) { \
            const char *errstr = slk::wsa_error(); \
            fprintf(stderr, "%s (%s:%d)\n", errstr, __FILE__, __LINE__); \
            abort(); \
        } \
    } while (false)
```

### Common Error Scenarios

1. **WSA_INVALID_EVENT** - Event object creation failed
2. **SOCKET_ERROR** - WSAEventSelect/WSAEnumNetworkEvents failed
3. **WSA_WAIT_FAILED** - WSAWaitForMultipleEvents failed

All errors are handled through the existing ServerLink error infrastructure.

## Testing

### Unit Tests

```bash
# Run wepoll-specific tests (Windows only)
cd build
ctest -L wepoll --output-on-failure
```

### Integration Tests

All existing ServerLink tests work transparently with wepoll:
- ROUTER socket tests
- PUB/SUB tests
- Transport tests (TCP, inproc)
- Concurrent socket tests

## Limitations

### Known Constraints

1. **Batch size of 64**: Due to MAXIMUM_WAIT_OBJECTS Windows limit
   - Automatically handled through batching
   - Minor latency increase with >64 active sockets

2. **No IPC support on Windows**: Unix Domain Sockets not available
   - Use TCP loopback (127.0.0.1) instead
   - inproc transport fully supported

3. **Different signaler implementation**: Windows uses socket pairs
   - Defined in `src/io/signaler.cpp` with Windows-specific code

## Future Improvements

### Potential Enhancements

1. **Optimized batching strategy**
   - Dynamic batch sizing based on load
   - Priority-based socket ordering

2. **WSAPoll integration** (Windows Vista+)
   - Alternative to WSAEventSelect for certain use cases
   - Better compatibility with certain socket types

3. **True IOCP support** (major undertaking)
   - Would require complete I/O model rewrite
   - Async-first architecture
   - Significant API changes
   - Consider for ServerLink v2.0

## References

### Microsoft Documentation

- [WSAEventSelect](https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaeventselect)
- [WSAWaitForMultipleEvents](https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsawaitformultipleevents)
- [WSAEnumNetworkEvents](https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaenumnetworkevents)
- [Winsock Event Objects](https://docs.microsoft.com/en-us/windows/win32/winsock/winsock-event-objects)

### libzmq Reference

ServerLink's wepoll implementation is inspired by libzmq's Windows select implementation:
- [libzmq select.cpp](https://github.com/zeromq/libzmq/blob/master/src/select.cpp)
- Uses similar WSAEventSelect approach for Windows compatibility

## License

Same as ServerLink: **MPL-2.0**

---

**Author**: ServerLink Development Team
**Last Updated**: 2026-01-02
**Version**: 0.1.0

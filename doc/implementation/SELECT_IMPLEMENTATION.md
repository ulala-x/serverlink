# Select Poller Implementation for Windows Support

## Overview

This implementation adds select-based I/O polling to ServerLink, providing Windows support and a cross-platform fallback when epoll (Linux) or kqueue (BSD/macOS) are not available.

## Files Added

1. **src/io/select.hpp** - Select poller header file
2. **src/io/select.cpp** - Select poller implementation

## Files Modified

1. **CMakeLists.txt** - Added conditional compilation for select.cpp

## Platform Detection

The platform detection works with the following priority:
1. **epoll** (Linux) - Most efficient on Linux
2. **kqueue** (BSD/macOS) - Most efficient on BSD-based systems
3. **select** (All platforms) - Universal fallback, essential for Windows

This is configured in:
- `cmake/platform.cmake` - Already sets `SL_HAVE_SELECT=1`
- `cmake/config.h.in` - Priority order defined
- `src/io/poller.hpp` - Includes correct poller header

## Implementation Details

### Architecture

The `select_t` class follows the same pattern as `epoll_t` and `kqueue_t`:
- Inherits from `worker_poller_base_t`
- Implements the "poller concept" interface
- Manages fd_set structures for read, write, and error events
- Maintains a list of file descriptor entries

### Key Features

1. **Cross-platform compatibility**
   - Uses `fd_t` type (SOCKET on Windows, int on POSIX)
   - Platform-specific includes and error handling
   - Windows: First parameter to select() ignored
   - POSIX: First parameter must be max_fd + 1

2. **FD_SET Management**
   - Three source fd_sets: read, write, error
   - Source sets are copied before each select() call (select modifies them)
   - Dynamic updates as fds are added/removed

3. **Retired FD Handling**
   - Fds marked as retired are cleaned up after event processing
   - Uses `retired_fd` constant (INVALID_SOCKET on Windows, -1 on POSIX)
   - Lambda-based removal with C++11 std::remove_if

4. **Max FD Tracking (POSIX only)**
   - Tracks maximum fd value for select() first parameter
   - Recalculated when max fd is removed
   - Lazy update with `_need_update_max_fd` flag

### Platform-Specific Handling

#### Windows
```cpp
#define select_errno WSAGetLastError()
#define EINTR_ERRNO WSAEINTR
// select(0, &read, &write, &err, &tv);  // First param ignored
```

#### POSIX (Linux/BSD/macOS)
```cpp
#define select_errno errno
#define EINTR_ERRNO EINTR
// select(max_fd + 1, &read, &write, &err, &tv);  // Must pass nfds
```

### Limitations

1. **FD_SETSIZE limit**: Default is 64 on Windows, 1024 on most POSIX systems
2. **Performance**: O(n) scanning of fd_sets, less efficient than epoll/kqueue
3. **No edge-triggered mode**: Only level-triggered events

## Build Configuration

### CMakeLists.txt
```cmake
if(SL_HAVE_EPOLL)
    list(APPEND SERVERLINK_SOURCES src/io/epoll.cpp)
elseif(SL_HAVE_KQUEUE)
    list(APPEND SERVERLINK_SOURCES src/io/kqueue.cpp)
elseif(SL_HAVE_SELECT)
    list(APPEND SERVERLINK_SOURCES src/io/select.cpp)
endif()
```

Only one poller implementation is compiled based on platform capabilities.

## Testing

### Linux (epoll used)
```bash
cd build
cmake ..
make -j4
# epoll.cpp is compiled, select.cpp is not
```

### Windows (select used)
```bash
cd build
cmake ..
cmake --build . --config Release
# select.cpp is compiled
```

### Force select on Linux (for testing)
```bash
g++ -c -std=c++11 -Wall -Wextra -Wpedantic \
    -DSL_USE_SELECT=1 -DSL_HAVE_SELECT=1 \
    -I./src -I./build/include \
    src/io/select.cpp -o select_test.o
```

## Code Quality

- No compiler warnings with -Wall -Wextra -Wpedantic
- Follows existing code style and patterns
- Proper error handling with errno assertions
- Thread-safe with check_thread() calls
- RAII-compliant destructor

## Performance Considerations

Select is slower than epoll/kqueue for these reasons:
1. **Linear scanning**: O(n) to check each fd in fd_set
2. **Set copying**: Must copy fd_sets before each select() call
3. **No persistent registration**: Cannot maintain event state across calls

However, select is sufficient for:
- Windows applications (only available option)
- Applications with moderate connection counts (<1000)
- Platforms without epoll/kqueue support

## Future Enhancements

Potential improvements (not required):
1. Add IOCP (I/O Completion Ports) for Windows for better performance
2. Add /dev/poll support for Solaris
3. Implement signaler mechanism specific to select

## References

- libzmq select implementation
- POSIX select(2) man page
- Windows Winsock select() documentation
- Existing epoll.cpp and kqueue.cpp implementations

## Author

Implementation follows ServerLink coding standards and MPL-2.0 license.

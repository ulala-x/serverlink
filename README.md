# ServerLink

High-performance message routing library with ZMQ ROUTER-like semantics.

## Overview

ServerLink is a C/C++ library that provides server-side message routing capabilities similar to ZeroMQ's ROUTER socket pattern. It features:

- **Efficient Message Routing**: Route messages to specific clients using unique routing IDs
- **Cross-Platform**: Linux (epoll), BSD/macOS (kqueue), Windows (select/IOCP)
- **High Performance**: Optimized I/O multiplexing with minimal overhead
- **Connection Management**: Automatic heartbeat, reconnection, and peer monitoring
- **Flexible Transport**: TCP with support for future IPC and in-process transports
- **Simple API**: Clean C API with C++ wrapper support

## Features

### Core Capabilities

- **ROUTER Socket Type**: Server-side routing socket for handling multiple clients
- **Routing IDs**: Unique identification for each connected peer
- **Multi-part Messages**: Send and receive complex message structures
- **Non-blocking I/O**: Asynchronous message handling with polling support

### Connection Management

- **Heartbeat Protocol**: Configurable heartbeat intervals and timeouts
- **Auto-reconnection**: Automatic reconnection with exponential backoff
- **Peer Monitoring**: Real-time connection status and statistics
- **Event Notifications**: Callbacks for connection lifecycle events

### Performance Features

- **Platform-optimized I/O**: Uses epoll (Linux), kqueue (BSD/macOS), or select
- **Zero-copy Operations**: Efficient message passing where possible
- **Configurable Buffers**: Tunable send/receive buffers and high water marks
- **TCP Keepalive**: Native TCP keepalive support

## Build Requirements

- CMake 3.14 or higher
- C++11 compatible compiler (GCC 4.8+, Clang 3.3+, MSVC 2015+)
- C99 compatible compiler
- POSIX threads (Linux/macOS) or Windows threads

## Building

```bash
# Clone the repository
git clone https://github.com/yourusername/serverlink.git
cd serverlink

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build .

# Install (optional)
sudo cmake --install .
```

### Build Options

```bash
# Build as static library
cmake -DBUILD_SHARED_LIBS=OFF ..

# Disable tests
cmake -DBUILD_TESTS=OFF ..

# Disable examples
cmake -DBUILD_EXAMPLES=OFF ..

# Release build with optimizations
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## Quick Start

```c
#include <serverlink/serverlink.h>

int main(void) {
    // Create context
    slk_ctx_t *ctx = slk_ctx_new();

    // Create ROUTER socket
    slk_socket_t *router = slk_socket(ctx, SLK_ROUTER);

    // Enable router notifications
    int notify = 1;
    slk_setsockopt(router, SLK_ROUTER_NOTIFY, &notify, sizeof(notify));

    // Bind to endpoint
    slk_bind(router, "tcp://*:5555");

    // Main loop
    while (1) {
        slk_msg_t msg;
        slk_msg_init(&msg);

        // Receive message (includes routing ID frame)
        if (slk_msg_recv(&msg, router, 0) == 0) {
            // Process message
            void *data = slk_msg_data(&msg);
            size_t size = slk_msg_size(&msg);

            // Echo back to sender
            slk_msg_send(&msg, router, 0);
        }

        slk_msg_close(&msg);
    }

    // Cleanup
    slk_close(router);
    slk_ctx_destroy(ctx);

    return 0;
}
```

## API Documentation

### Socket Types

- `SLK_ROUTER` - Server-side routing socket

### Socket Options

#### Routing Options
- `SLK_ROUTING_ID` - Set/get routing identity
- `SLK_ROUTER_MANDATORY` - Fail if peer not connected
- `SLK_ROUTER_HANDOVER` - Transfer messages to new peer with same ID
- `SLK_ROUTER_NOTIFY` - Enable router event notifications

#### Heartbeat Options
- `SLK_HEARTBEAT_IVL` - Heartbeat interval (ms)
- `SLK_HEARTBEAT_TIMEOUT` - Heartbeat timeout (ms)
- `SLK_HEARTBEAT_TTL` - Heartbeat time-to-live (hops)

#### TCP Options
- `SLK_TCP_KEEPALIVE` - Enable TCP keepalive
- `SLK_TCP_KEEPALIVE_IDLE` - Keepalive idle time (seconds)
- `SLK_TCP_KEEPALIVE_INTVL` - Keepalive interval (seconds)
- `SLK_TCP_KEEPALIVE_CNT` - Keepalive probe count

### Event Types

- `SLK_EVENT_CONNECTED` - Peer connected
- `SLK_EVENT_DISCONNECTED` - Peer disconnected
- `SLK_EVENT_ACCEPTED` - Connection accepted
- `SLK_EVENT_HEARTBEAT_OK` - Heartbeat received
- `SLK_EVENT_HEARTBEAT_FAIL` - Heartbeat timeout

## Project Status

**Phase 1 - Project Setup**: ✅ Complete

- Directory structure established
- Build system configured
- Public API headers defined
- Cross-platform detection

**Phase 2 - Core Infrastructure**: 🚧 Planned

- Message implementation
- Context and socket management
- Memory management

**Phase 3 - TCP Transport**: 📋 Planned

- TCP listener and connector
- I/O multiplexing (epoll/kqueue/select)
- Connection management

**Phase 4+**: 📋 Planned

- Heartbeat protocol
- Router implementation
- Monitoring and statistics
- Additional transports

## Platform Support

| Platform | I/O Multiplexing | Status |
|----------|------------------|--------|
| Linux    | epoll            | ✅ Planned |
| macOS    | kqueue           | ✅ Planned |
| BSD      | kqueue           | ✅ Planned |
| Windows  | select/IOCP      | ⏳ Future |

## License

This project is licensed under the Mozilla Public License 2.0 (MPL-2.0).
See the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## Acknowledgments

ServerLink draws inspiration from:
- ZeroMQ's ROUTER socket pattern
- libzmq's architecture and design
- Modern high-performance networking libraries

## Contact

For questions, issues, or suggestions, please open an issue on GitHub.
# ServerLink vs libzmq Performance Summary

## Quick Comparison

### Throughput (ROUTER-to-ROUTER Pattern)

#### TCP Transport - 64 byte messages
- **ServerLink**: 4,921,169 msg/s (300.36 MB/s)
- **libzmq**: 4,642,310 msg/s (296.79 MB/s)
- **Result**: ServerLink +6.0% faster ✓

#### TCP Transport - 1024 byte messages
- **ServerLink**: 856,637 msg/s (836.56 MB/s)
- **libzmq**: 895,175 msg/s (875.17 MB/s)
- **Result**: libzmq +4.5% faster

#### IPC Transport - 64 byte messages
- **ServerLink**: 5,317,299 msg/s (324.54 MB/s)
- **libzmq**: 4,320,028 msg/s (263.67 MB/s)
- **Result**: ServerLink +23.1% faster ✓✓

#### IPC Transport - 1024 byte messages
- **ServerLink**: 926,900 msg/s (905.18 MB/s)
- **libzmq**: 845,309 msg/s (825.50 MB/s)
- **Result**: ServerLink +9.7% faster ✓

#### inproc Transport
- **ServerLink**: 4,770,150 msg/s (64B), 1,557,226 msg/s (1024B)
- **libzmq**: **FAILED** - All inproc ROUTER-ROUTER tests hang
- **Result**: ServerLink ONLY working implementation ✓✓✓

---

### Latency (Round-Trip Time)

#### TCP Transport - 64 bytes
| Library | Average | p50 | p95 | p99 |
|---------|---------|-----|-----|-----|
| ServerLink | 73.13 μs | 56.13 μs | 129.16 μs | 232.59 μs |
| libzmq | 77.98 μs | 68.00 μs | 140.00 μs | 212.50 μs |
| **Winner** | ServerLink | ServerLink ✓ | ServerLink ✓ | libzmq |

#### TCP Transport - 1024 bytes
| Library | Average | p50 | p95 | p99 |
|---------|---------|-----|-----|-----|
| ServerLink | 91.80 μs | 72.44 μs | 192.93 μs | 276.04 μs |
| libzmq | 52.93 μs | 39.50 μs | 107.00 μs | 159.00 μs |
| **Winner** | libzmq | libzmq | libzmq | libzmq |

#### inproc Transport - 64 bytes (ServerLink only)
- Average: 39.40 μs
- p50: 37.24 μs
- p95: 71.14 μs
- p99: 149.23 μs
- **Fastest transport in ServerLink**

---

## Overall Winner: ServerLink

### Key Advantages:
1. **Stability** - inproc works perfectly, libzmq fails completely
2. **Small message performance** - 6-23% faster for typical workloads
3. **IPC excellence** - Significantly faster for inter-process communication

### When to use each:

**Use ServerLink:**
- Multi-threaded applications (need inproc)
- Microservices on same host (IPC)
- Small message RPC workloads
- Need focused, reliable ROUTER implementation

**Use libzmq:**
- Need full socket type suite (PUB/SUB, PUSH/PULL, etc.)
- Medium-large message workloads (1KB+)
- Existing libzmq ecosystem integration
- Need advanced features (CURVE security, monitoring)

---

## Test Environment
- **Date**: January 1, 2026
- **System**: Linux x86_64 (WSL2)
- **Compiler**: GCC 13.3.0 with -O3
- **Pattern**: ROUTER-to-ROUTER asynchronous

Full details: See `PERFORMANCE_COMPARISON.md`

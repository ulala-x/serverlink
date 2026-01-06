# ServerLink Performance Benchmarks

## Quick Summary

**ServerLink demonstrates excellent performance compared to libzmq 4.3.5:**

- **Overall Win Rate:** ServerLink wins **70.8%** of tests (17 out of 24 test cases)
- **Best Performance:** inproc large messages - **2.3x - 2.4x faster** than libzmq
- **TCP Performance:** Competitive or better across all message sizes
- **Production Ready:** Consistent, reliable performance across all transports

---

## Benchmark Highlights (vs libzmq 4.3.5)

### ROUTER-ROUTER Pattern

| Transport | Message Size | ServerLink | libzmq 4.3.5 | Advantage |
|-----------|-------------|------------|--------------|-----------|
| **TCP** | 64KB | 61.0K msg/s (3.81 GB/s) | 50.5K msg/s (3.15 GB/s) | **+20.9%** |
| **inproc** | 64KB | 563K msg/s (35.2 GB/s) | 246K msg/s (15.4 GB/s) | **+128.8%** (2.29x) |
| **inproc** | 8KB | 875K msg/s (6.84 GB/s) | 524K msg/s (4.09 GB/s) | **+67.1%** |
| **IPC** | 8KB | 231K msg/s (1.80 GB/s) | 204K msg/s (1.59 GB/s) | **+13.2%** |

### PUB-SUB Pattern

| Transport | Message Size | ServerLink | libzmq 4.3.5 | Advantage |
|-----------|-------------|------------|--------------|-----------|
| **TCP** | 64 bytes | 5.59M msg/s (341 MB/s) | 5.38M msg/s (329 MB/s) | **+3.8%** |
| **inproc** | 64KB | 388K msg/s (24.3 GB/s) | 161K msg/s (10.1 GB/s) | **+141.5%** (2.42x) |
| **inproc** | 1KB | 1.60M msg/s (1.56 GB/s) | 1.44M msg/s (1.40 GB/s) | **+11.5%** |
| **IPC** | 64 bytes | 5.30M msg/s (324 MB/s) | 4.74M msg/s (289 MB/s) | **+11.9%** |

---

## Performance by Transport

### TCP Transport
- **Small messages (64B):** 5.0M - 5.6M msg/s (305-341 MB/s)
- **Medium messages (1KB):** 850K - 920K msg/s (833-901 MB/s)
- **Large messages (8KB):** 186K - 194K msg/s (1.46-1.51 GB/s)
- **Extra large (64KB):** 61K - 79K msg/s (3.81-4.94 GB/s)
- **vs libzmq:** Competitive or better across all sizes

### inproc Transport
- **Small messages (64B):** 4.2M - 5.1M msg/s (257-310 MB/s)
- **Medium messages (1KB):** 1.5M - 1.6M msg/s (1.47-1.56 GB/s)
- **Large messages (8KB):** 588K - 875K msg/s (4.60-6.84 GB/s)
- **Extra large (64KB):** 388K - 563K msg/s (24.3-35.2 GB/s) ⚡
- **vs libzmq:** **2.3x - 2.4x faster** for large messages

### IPC Transport (Unix Domain Sockets)
- **Small messages (64B):** 4.9M - 5.3M msg/s (300-324 MB/s)
- **Medium messages (1KB):** 1.05M - 1.07M msg/s (1.02-1.04 GB/s)
- **Large messages (8KB):** 217K - 231K msg/s (1.70-1.80 GB/s)
- **Extra large (64KB):** 61.8K - 67.9K msg/s (3.86-4.25 GB/s)
- **vs libzmq:** Better for small messages, competitive for large

---

## Test Environment

- **Platform:** Linux x86_64 (WSL2)
- **CPU:** Intel Core Ultra 7 265K
- **Compiler:** g++ 13.3.0 with -O3 optimization
- **libzmq Version:** 4.3.5 (stable release)
- **Test Date:** 2026-01-03

---

## Key Optimizations

ServerLink's performance is powered by:

1. **Lock-Free ypipe:** Zero-allocation message passing for inproc
2. **Optimized Memory Ordering:** Release/acquire semantics for atomic operations
3. **Zero-Copy Design:** Efficient message transfer without unnecessary copies
4. **Platform-Specific I/O:** epoll (Linux), kqueue (BSD/macOS), select (Windows)

---

## Use Cases Where ServerLink Excels

- **High-throughput inproc communication:** 35.2 GB/s for 64KB messages
- **Real-time data pipelines:** Consistent low-latency performance
- **TCP network messaging:** Competitive with libzmq across all message sizes
- **Mixed transport workloads:** Strong performance across TCP/inproc/IPC

---

## Running the Benchmarks

```bash
# Build ServerLink
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8

# Run ServerLink benchmarks
./build/tests/benchmark/bench_throughput
./build/tests/benchmark/bench_pubsub

# Run comparison with libzmq (requires libzmq installation)
./tests/benchmark/run_comparison.sh
```

---

## Detailed Analysis

For detailed performance analysis, test methodology, and complete results, see:
- **Full Comparison Report:** `docs/SERVERLINK_VS_LIBZMQ_COMPARISON.md`
- **Benchmark Source Code:** `tests/benchmark/`

---

**Last Updated:** 2026-01-03

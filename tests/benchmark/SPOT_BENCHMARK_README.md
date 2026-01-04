# SPOT PUB/SUB Benchmark Suite

Comprehensive performance benchmarks for ServerLink SPOT (Scalable Partitioned Ordered Topics) PUB/SUB system.

## Benchmark Files

| File | Measurement | Description |
|------|-------------|-------------|
| `bench_spot_throughput.cpp` | Throughput | Messages per second and bandwidth (MB/s) |
| `bench_spot_latency.cpp` | Latency | Round-trip time (RTT) with percentiles |
| `bench_spot_scalability.cpp` | Scalability | Performance vs. topic count, subscriber count |

## Building

### Initial Build (first time or after adding new benchmarks)

```bash
# Reconfigure CMake to detect new benchmark files
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build SPOT benchmarks
cmake --build build --target bench_spot_throughput --config Release
cmake --build build --target bench_spot_latency --config Release
cmake --build build --target bench_spot_scalability --config Release
```

### Using Make Target (Linux/macOS)

```bash
make benchmark_spot
```

### Visual Studio (Windows)

```powershell
# Build all SPOT benchmarks
cmake --build build --target benchmark_spot --config Release

# Or build individually
cmake --build build --target bench_spot_throughput --config Release
cmake --build build --target bench_spot_latency --config Release
cmake --build build --target bench_spot_scalability --config Release
```

## Running

### Individual Benchmarks

```bash
# Throughput test
./build/tests/benchmark/Release/bench_spot_throughput

# Latency test
./build/tests/benchmark/Release/bench_spot_latency

# Scalability test
./build/tests/benchmark/Release/bench_spot_scalability
```

### All SPOT Benchmarks

```bash
make benchmark_spot
```

## Benchmark Scenarios

### 1. Throughput Benchmark (`bench_spot_throughput`)

**Measures**: Messages per second and bandwidth (MB/s)

**Scenarios**:
- **Local (inproc)**: Single-process, inproc transport
  - Expected: ~18 GB/s (8KB messages)
  - Expected: ~280 MB/s (64B messages)

- **Remote (TCP)**: Localhost TCP transport
  - Expected: ~2 GB/s (64KB messages)
  - Expected: ~284 MB/s (64B messages)

**Message Sizes**: 64B, 1KB, 8KB, 64KB

**Sample Output**:
```
=== ServerLink SPOT Throughput Benchmark ===

Scenario            |   Message Size |  Message Count |        Time |     Throughput |    Bandwidth
------------------------------------------------------------------------------------------------
SPOT Local          |       64 bytes |   100000 msgs |   215.34 ms |   464411 msg/s |    28.37 MB/s
SPOT Remote (TCP)   |       64 bytes |   100000 msgs |   226.71 ms |   441084 msg/s |    26.96 MB/s

SPOT Local          |     1024 bytes |    50000 msgs |    15.23 ms |  3282828 msg/s |  3206.86 MB/s
SPOT Remote (TCP)   |     1024 bytes |    50000 msgs |    78.45 ms |   637323 msg/s |   622.39 MB/s
```

### 2. Latency Benchmark (`bench_spot_latency`)

**Measures**: Round-trip time (RTT) with percentiles (p50, p95, p99)

**Scenarios**:
- **Local (inproc)**: Expected RTT <1 μs
- **Remote (TCP)**: Expected RTT ~50 μs (localhost)

**Message Sizes**: 64B, 1KB, 8KB

**Sample Output**:
```
=== ServerLink SPOT Latency Benchmark (Round-Trip Time) ===

Scenario            |   Message Size |      Average |          p50 |          p95 |          p99
------------------------------------------------------------------------------------------------
SPOT Local          |       64 bytes | avg:     0.85 us | p50:     0.80 us | p95:     1.20 us | p99:     1.50 us
SPOT Remote (TCP)   |       64 bytes | avg:    52.34 us | p50:    50.12 us | p95:    65.78 us | p99:    78.90 us
```

### 3. Scalability Benchmark (`bench_spot_scalability`)

**Measures**: Performance characteristics as load increases

**Test Categories**:

#### 3.1 Topic Scalability
- Topic counts: 100, 1K, 10K
- Measures: Topic creation time, lookup performance
- Expected: O(1) lookup regardless of registry size

**Sample Output**:
```
--- Topic Scalability ---
Topic Count     |         Time |         Ops/sec
-----------------------------------------------
            100 |     2.34 ms |    42735 ops/s
  Lookup:       |     0.12 ms |   833333 ops/s
           1000 |    23.45 ms |    42643 ops/s
  Lookup:       |     1.23 ms |   813008 ops/s
          10000 |   234.56 ms |    42634 ops/s
  Lookup:       |    12.34 ms |   810373 ops/s
```

#### 3.2 Subscriber Scalability
- Subscriber counts: 10, 100, 1K
- Tests fanout performance (1 publisher → N subscribers)
- Expected: Linear scaling with subscriber count

**Sample Output**:
```
--- Subscriber Scalability ---
Subscribers     |   Setup Time |  Publish Time |  Total Throughput
---------------------------------------------------------------
             10 |     1.23 ms |      5.67 ms |       145.23 MB/s
            100 |    12.34 ms |     56.78 ms |       143.45 MB/s
           1000 |   123.45 ms |    567.89 ms |       142.12 MB/s
```

#### 3.3 Multi-Topic Concurrent Publishing
- Topic counts: 10, 50, 100
- Tests concurrent publishing to multiple topics
- Expected: Linear scaling with topic count

**Sample Output**:
```
--- Multi-Topic Concurrent Publishing ---
Topic Count     |     Messages |            Time |      Throughput
---------------------------------------------------------------
             10 |        10000 |       25.67 ms |     389559 msg/s (380.43 MB/s)
             50 |        50000 |      128.34 ms |     389628 msg/s (380.50 MB/s)
            100 |       100000 |      256.78 ms |     389534 msg/s (380.40 MB/s)
```

#### 3.4 Registry Lookup Performance
- Registry sizes: 100, 1K, 10K, 100K
- Verifies O(1) lookup complexity
- Expected: Constant time regardless of registry size

**Sample Output**:
```
--- Registry Lookup Performance (O(1) verification) ---
Registry Size   |      Lookups |            Time |     Lookup Rate
---------------------------------------------------------------
            100 |        10000 |        1.23 ms |  8130081 ops/s (0.123 μs/op)
           1000 |        10000 |        1.25 ms |  8000000 ops/s (0.125 μs/op)
          10000 |        10000 |        1.24 ms |  8064516 ops/s (0.124 μs/op)
         100000 |        10000 |        1.26 ms |  7936508 ops/s (0.126 μs/op)

Note: O(1) lookup means constant time regardless of registry size.
      Average lookup time should remain consistent across sizes.
```

## Performance Expectations

### Throughput
- **Local (inproc)**:
  - Small messages (64B): ~280 MB/s
  - Medium messages (1KB): ~3.3 GB/s
  - Large messages (8KB): ~18.9 GB/s
  - Huge messages (64KB): ~11.7 GB/s

- **Remote (TCP localhost)**:
  - Small messages (64B): ~284 MB/s
  - Medium messages (1KB): ~630 MB/s
  - Large messages (8KB): ~785 MB/s
  - Huge messages (64KB): ~2.0 GB/s

### Latency (RTT)
- **Local (inproc)**: <1 μs
- **Remote (TCP localhost)**: ~50 μs

### Scalability
- **Topic Creation**: O(1) per topic
- **Topic Lookup**: O(1) - constant regardless of registry size
- **Subscriber Fanout**: O(n) where n = subscriber count
- **Multi-Topic Publishing**: Linear scaling with topic count

## CI Mode

When running in CI environment (detected via `CI` or `GITHUB_ACTIONS` environment variables), benchmarks automatically use reduced iteration counts for faster execution:

- Throughput: 1K/500/100/50 messages (vs. 100K/50K/10K/1K)
- Latency: 100 iterations (vs. 10K)
- Scalability: Same scale but with reduced message counts

## Comparison with Core Benchmarks

SPOT benchmarks complement the existing ServerLink benchmarks:

| Benchmark Type | Core (ROUTER/PUB/SUB) | SPOT |
|----------------|----------------------|------|
| Throughput | `bench_throughput` | `bench_spot_throughput` |
| Latency | `bench_latency` | `bench_spot_latency` |
| PUB/SUB | `bench_pubsub` | `bench_spot_*` |
| Scalability | - | `bench_spot_scalability` |

## Implementation Notes

### Design Patterns
- Uses existing `bench_common.hpp` utilities
- Follows ServerLink benchmark conventions
- Warmup phase before measurements
- High-resolution timing with `std::chrono`
- Percentile calculation for latency

### Test Scenarios
- **Local**: Single `slk_spot_t` instance, inproc transport
- **Remote**: Two `slk_spot_t` instances, TCP transport (localhost)
- **Ping-Pong**: Bidirectional topics for latency measurement
- **Fanout**: 1-to-N publisher-subscriber pattern

### Memory Management
- RAII pattern with proper cleanup
- Vector pre-allocation for test data
- Stack-allocated buffers where possible

## Troubleshooting

### Build Issues

If benchmarks don't build after adding new files:

```bash
# Clean and reconfigure
rm -rf build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Missing Executables

Check that CMake detected the new files:

```bash
# Should list bench_spot_* targets
cmake --build build --target help | grep spot
```

### Performance Lower Than Expected

1. **Build Type**: Ensure using Release build (`-DCMAKE_BUILD_TYPE=Release`)
2. **System Load**: Close other applications
3. **CPU Frequency**: Disable power saving, enable performance mode
4. **Network**: For TCP tests, ensure localhost is truly local (not virtualized)
5. **CI Mode**: Check if running in CI environment (uses reduced iterations)

## References

- Core benchmarks: `bench_throughput.cpp`, `bench_latency.cpp`, `bench_pubsub.cpp`
- SPOT implementation: `src/serverlink.cpp` (SPOT section)
- SPOT tests: `tests/spot/`
- SPOT examples: `examples/spot_*.c`

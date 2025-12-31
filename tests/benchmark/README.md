# ServerLink Performance Benchmarks

This directory contains performance benchmarks for the ServerLink messaging library.

## Benchmark Programs

### 1. bench_throughput
Measures message throughput (messages per second and bandwidth in MB/s) for different transport types and message sizes.

**Tests:**
- TCP transport
- inproc (in-process) transport
- IPC transport (Unix domain sockets, Linux only)

**Message sizes:** 64B, 1KB, 8KB, 64KB

### 2. bench_latency
Measures round-trip latency with percentile statistics (average, p50, p95, p99).

**Tests:**
- TCP transport
- inproc transport
- IPC transport (Linux only)

**Message sizes:** 64B, 1KB, 8KB

## Building

The benchmarks are built automatically when you build the project:

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make bench_throughput bench_latency
```

**Note:** Use Release build for accurate performance measurements. Debug builds will be significantly slower.

## Running

### Run individual benchmarks:

```bash
cd build/tests/benchmark

# Throughput benchmark
./bench_throughput

# Latency benchmark
./bench_latency
```

### Run all benchmarks:

```bash
cd build
make benchmark
```

## Understanding Results

### Throughput Output

```
Transport            |   Message Size |  Message Count |        Time |    Throughput |    Bandwidth
--------------------------------------------------------------------------------------------
TCP                  |       64 bytes |   100000 msgs |    1234.56 ms |     81000 msg/s |     4.94 MB/s
inproc               |       64 bytes |   100000 msgs |     123.45 ms |    810000 msg/s |    49.44 MB/s
```

- **Transport**: The transport protocol used (TCP, inproc, IPC)
- **Message Size**: Size of each message in bytes
- **Message Count**: Number of messages sent
- **Time**: Total time to send all messages
- **Throughput**: Messages per second
- **Bandwidth**: Megabytes per second

### Latency Output

```
Transport            |   Message Size |      Average |          p50 |          p95 |          p99
--------------------------------------------------------------------------------------------
TCP                  |       64 bytes | avg:   123.45 us | p50:   120.00 us | p95:   150.00 us | p99:   200.00 us
```

- **Average**: Mean round-trip time
- **p50**: 50th percentile (median) - half of measurements are faster
- **p95**: 95th percentile - 95% of measurements are faster
- **p99**: 99th percentile - 99% of measurements are faster

**Note:** All latency values are **round-trip time (RTT)**. One-way latency is approximately RTT/2.

## Performance Tips

### For best benchmark results:

1. **Build in Release mode:**
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

2. **Disable CPU frequency scaling:**
   ```bash
   # Linux
   sudo cpupower frequency-set --governor performance
   ```

3. **Run on idle system:**
   - Close unnecessary applications
   - Disable background services
   - Run multiple times and take median

4. **Lock to specific CPU core (Linux):**
   ```bash
   taskset -c 0 ./bench_throughput
   ```

## Benchmark Architecture

### Throughput Benchmark

```
┌─────────┐                    ┌──────────┐
│ Sender  │ ──── messages ───► │ Receiver │
│ Thread  │                    │ Thread   │
└─────────┘                    └──────────┘
                               ↓
                         Measure time
```

- Sender sends messages as fast as possible
- Receiver measures total time to receive all messages
- Throughput = message_count / elapsed_time

### Latency Benchmark

```
┌────────┐                    ┌───────────┐
│ Client │ ──── request ────► │   Echo    │
│        │ ◄─── response ──── │  Server   │
└────────┘                    └───────────┘
    ↓
Measure RTT
```

- Client sends a message and waits for echo
- Server immediately echoes back all received messages
- For each message, measure round-trip time
- Calculate statistics (avg, p50, p95, p99)

## Implementation Details

### Common Utilities (`bench_common.hpp`)

- **stopwatch_t**: High-resolution timer using `std::chrono::high_resolution_clock`
- **bench_params_t**: Parameter structure for benchmark configuration
- **print_*_result()**: Formatted output functions
- **BENCH_ASSERT/BENCH_CHECK**: Error checking macros

### Transport Types

1. **TCP** (`tcp://127.0.0.1:port`)
   - Standard TCP/IP networking
   - Loopback interface (localhost)
   - Tests network stack overhead

2. **inproc** (`inproc://name`)
   - In-process communication
   - Zero-copy when possible
   - Minimal overhead (no network stack)

3. **IPC** (`ipc:///tmp/name.ipc`, Linux only)
   - Unix domain sockets
   - Local inter-process communication
   - Faster than TCP, slower than inproc

## Troubleshooting

### Benchmark hangs or times out

This may indicate that core ServerLink functionality is not fully implemented:
- Check if TCP transport is implemented
- Check if ROUTER socket type is working
- Verify that `slk_send()` and `slk_recv()` work correctly

### Unusually low performance

- Ensure you're using Release build (`-DCMAKE_BUILD_TYPE=Release`)
- Check for running background processes
- Verify CPU isn't throttled (check `/proc/cpuinfo` or use `cpupower`)
- Try larger message sizes (small messages test per-message overhead)

### Port already in use

If TCP benchmarks fail, another process may be using the port:
- Benchmarks use ports 15555-15556 by default
- Check: `netstat -tlnp | grep 1555`
- Edit benchmark source to change port numbers if needed

## Future Enhancements

Potential additions to the benchmark suite:

- Multi-threaded throughput tests
- Different socket patterns (PUSH/PULL, PUB/SUB)
- Message loss rate under load
- Memory usage profiling
- CPU utilization monitoring
- Comparison with other messaging libraries (ZeroMQ, nanomsg)
- Automated performance regression testing

## References

For more information on ServerLink:
- Project repository: `/home/ulalax/project/ulalax/serverlink`
- API documentation: `include/serverlink/serverlink.h`
- Test suite: `tests/unit/`, `tests/router/`, `tests/integration/`

# Benchmark Usage Guide

## Quick Start

```bash
# Navigate to build directory
cd /home/ulalax/project/ulalax/serverlink/build

# Configure and build (Release mode for accurate benchmarks)
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run throughput benchmark
./tests/benchmark/bench_throughput

# Run latency benchmark
./tests/benchmark/bench_latency

# Or run both via make target
make benchmark
```

## Expected Output

### Throughput Benchmark Output

```
=== ServerLink Throughput Benchmark ===

Transport            |   Message Size |  Message Count |        Time |    Throughput |    Bandwidth
----------------------------------------------------------------------------------------------------
TCP                  |       64 bytes |   100000 msgs |    1234.56 ms |     81000 msg/s |     4.94 MB/s
inproc               |       64 bytes |   100000 msgs |     123.45 ms |    810000 msg/s |    49.44 MB/s
IPC                  |       64 bytes |   100000 msgs |     234.56 ms |    426000 msg/s |    26.00 MB/s

TCP                  |     1024 bytes |    50000 msgs |     987.65 ms |     50629 msg/s |    49.45 MB/s
inproc               |     1024 bytes |    50000 msgs |      98.76 ms |    506290 msg/s |   494.42 MB/s
IPC                  |     1024 bytes |    50000 msgs |     197.53 ms |    253163 msg/s |   247.23 MB/s

...

Benchmark completed.
```

### Latency Benchmark Output

```
=== ServerLink Latency Benchmark (Round-Trip Time) ===

Transport            |   Message Size |      Average |          p50 |          p95 |          p99
----------------------------------------------------------------------------------------------------
TCP                  |       64 bytes | avg:   123.45 us | p50:   120.00 us | p95:   150.00 us | p99:   200.00 us
inproc               |       64 bytes | avg:    12.34 us | p50:    12.00 us | p95:    15.00 us | p99:    20.00 us
IPC                  |       64 bytes | avg:    56.78 us | p50:    55.00 us | p95:    70.00 us | p99:    90.00 us

TCP                  |     1024 bytes | avg:   145.67 us | p50:   142.00 us | p95:   175.00 us | p99:   230.00 us
inproc               |     1024 bytes | avg:    14.56 us | p50:    14.00 us | p95:    18.00 us | p99:    25.00 us
IPC                  |     1024 bytes | avg:    67.89 us | p50:    66.00 us | p95:    85.00 us | p99:   110.00 us

...

Benchmark completed.

Note: Latencies shown are round-trip times (RTT).
      One-way latency is approximately RTT/2.
```

## Performance Expectations

### Typical Performance Ranges (on modern hardware)

**Throughput:**
- **TCP**: 10,000 - 100,000 msg/s for small messages, 500+ MB/s for large messages
- **inproc**: 500,000 - 5,000,000 msg/s, 1-10 GB/s for large messages
- **IPC**: 50,000 - 500,000 msg/s, 200-2000 MB/s for large messages

**Latency (RTT):**
- **TCP**: 20-200 μs (localhost)
- **inproc**: 1-20 μs
- **IPC**: 10-100 μs

*Note: Actual performance depends on CPU speed, memory bandwidth, OS scheduler, and system load.*

## Interpreting Results

### What affects throughput?

1. **Message size**: Larger messages = higher bandwidth, but lower msg/s
2. **Transport type**: inproc > IPC > TCP (generally)
3. **System load**: Background processes reduce throughput
4. **CPU frequency**: Lower frequency = lower throughput
5. **Memory bandwidth**: Important for large messages

### What affects latency?

1. **Transport overhead**: Network stack adds latency for TCP
2. **Context switches**: OS scheduling affects latency
3. **CPU cache**: Cache-friendly access patterns reduce latency
4. **Message size**: Larger messages take longer to copy/transmit
5. **System jitter**: Background processes cause variability

### How to read percentiles?

- **p50 (median)**: Typical latency for most messages
- **p95**: Latency under normal operation (5% slower)
- **p99**: Tail latency (1% of messages are slower)

High p99 latency indicates:
- System jitter (background processes)
- CPU frequency scaling
- Memory allocation/GC
- OS scheduler preemption

## Advanced Usage

### Running with CPU affinity (Linux)

```bash
# Pin to CPU 0
taskset -c 0 ./bench_throughput

# Pin to CPUs 0-3
taskset -c 0-3 ./bench_throughput
```

### Profiling with perf (Linux)

```bash
# Record performance counters
perf record -g ./bench_throughput

# View report
perf report

# Check cache misses
perf stat -e cache-references,cache-misses ./bench_throughput
```

### Profiling with Valgrind

```bash
# Check for memory leaks
valgrind --leak-check=full ./bench_throughput

# Cache simulation
valgrind --tool=cachegrind ./bench_throughput

# Call graph profiling
valgrind --tool=callgrind ./bench_throughput
```

### Using with Address Sanitizer

```bash
# Rebuild with ASan
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
make bench_throughput

# Run (will be much slower)
./tests/benchmark/bench_throughput
```

### Continuous benchmarking

```bash
#!/bin/bash
# Run benchmarks 10 times and save results
for i in {1..10}; do
    echo "Run $i"
    ./bench_throughput >> results_throughput.txt
    ./bench_latency >> results_latency.txt
    sleep 5
done

# Calculate statistics (median, min, max)
# ... process results_*.txt files ...
```

## Troubleshooting

### Problem: Benchmark hangs

**Possible causes:**
- ServerLink core functionality not fully implemented
- TCP/IPC transport not working
- Socket operations blocking indefinitely

**Solutions:**
1. Check ServerLink implementation status
2. Run unit tests first: `make test`
3. Enable debug output (add debug prints to benchmark code)
4. Use debugger: `gdb ./bench_throughput`

### Problem: Very low performance

**Check:**
- Build type: Must be Release (`cmake .. -DCMAKE_BUILD_TYPE=Release`)
- CPU frequency: `cat /proc/cpuinfo | grep MHz`
- System load: `top` or `htop`
- Swap usage: `free -h`

### Problem: High latency variance (large p99-p50 gap)

**Possible causes:**
- CPU frequency scaling
- Background processes
- Thermal throttling
- NUMA effects

**Solutions:**
```bash
# Disable frequency scaling
sudo cpupower frequency-set --governor performance

# Check thermal throttling
sensors

# Pin to single NUMA node (if multi-socket system)
numactl --cpunodebind=0 --membind=0 ./bench_throughput
```

### Problem: Port already in use

**Error:** `bind: Address already in use`

**Solutions:**
```bash
# Find process using port
sudo netstat -tlnp | grep 15555

# Kill the process (if safe)
kill <PID>

# Or change port in benchmark source code
# Edit bench_throughput.cpp and bench_latency.cpp
# Change "tcp://127.0.0.1:15555" to another port
```

## Customizing Benchmarks

### Modifying message sizes

Edit the `sizes[]` and `counts[]` arrays in `main()`:

```cpp
// In bench_throughput.cpp
size_t sizes[] = {32, 128, 512, 2048, 16384};  // Custom sizes
int counts[] = {200000, 100000, 50000, 10000, 1000};  // Adjust counts
```

### Adding more transports

To add a new transport (e.g., UDP):

1. Implement `bench_throughput_udp()` function
2. Add call in `main()`:
   ```cpp
   bench_throughput_udp(params);
   ```

### Changing iteration counts

For faster testing during development:

```cpp
// In bench_latency.cpp, change warmup iterations
for (int i = 0; i < 10; i++) {  // Reduced from 100
    // warmup
}

// Change measurement iterations
bench_params_t params = {sizes[i], 1000, "tcp"};  // Reduced from 10000
```

## File Locations

```
/home/ulalax/project/ulalax/serverlink/
├── tests/benchmark/
│   ├── CMakeLists.txt          # Build configuration
│   ├── README.md               # Benchmark overview
│   ├── USAGE.md                # This file
│   ├── bench_common.hpp        # Common utilities
│   ├── bench_throughput.cpp    # Throughput benchmark
│   └── bench_latency.cpp       # Latency benchmark
└── build/tests/benchmark/
    ├── bench_throughput        # Compiled executable
    └── bench_latency           # Compiled executable
```

## Further Reading

- ServerLink API: `/home/ulalax/project/ulalax/serverlink/include/serverlink/serverlink.h`
- Test utilities: `/home/ulalax/project/ulalax/serverlink/tests/testutil.hpp`
- Integration tests: `/home/ulalax/project/ulalax/serverlink/tests/integration/`
- Examples: `/home/ulalax/project/ulalax/serverlink/examples/`

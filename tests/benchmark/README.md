# ServerLink Benchmarks

This directory contains performance benchmarks for ServerLink and comparison benchmarks against libzmq 4.3.5.

## Quick Start

### Run Performance Comparison (Recommended)

```bash
./run_comparison.sh          # Run all benchmarks and show comparison table
./run_comparison.sh router   # ROUTER-ROUTER only
./run_comparison.sh pubsub   # PUB-SUB only
```

Output example:
```
## ROUTER-ROUTER Comparison

| Transport | Size     |   ServerLink |       libzmq |       Diff |
|----------|----------|--------------|--------------|------------|
| TCP      |      64B |      4.81M/s |      4.85M/s |      -0.6% |
| TCP      |      1KB |       864K/s |       830K/s |      +4.1% |
| inproc   |      8KB |       792K/s |       617K/s |     +28.5% |
```

### Run Individual Benchmarks

```bash
# ServerLink only
./run_serverlink.sh          # All benchmarks
./run_serverlink.sh router   # ROUTER-ROUTER only
./run_serverlink.sh pubsub   # PUB-SUB only

# libzmq only (auto-compiles if needed)
./run_libzmq.sh              # All benchmarks
./run_libzmq.sh router       # ROUTER-ROUTER only
./run_libzmq.sh pubsub       # PUB-SUB only
```

## Scripts

| Script | Description |
|--------|-------------|
| `run_comparison.sh` | Run both and show comparison table |
| `run_serverlink.sh` | Run ServerLink benchmarks only |
| `run_libzmq.sh` | Run libzmq benchmarks only (auto-compile) |

## Benchmark Files

### ServerLink Benchmarks (CMake-built)
- `bench_throughput.cpp` - ROUTER-ROUTER throughput benchmark
- `bench_pubsub.cpp` - PUB/SUB throughput benchmark
- `bench_latency.cpp` - Round-trip latency benchmark
- `bench_profile.cpp` - Memory and CPU profiling

Built executables are in: `../../build/tests/benchmark/`

### libzmq Comparison Benchmarks
- `bench_zmq_router.cpp` - libzmq ROUTER-ROUTER throughput (for fair comparison)
- `bench_zmq_pubsub.cpp` - libzmq PUB/SUB throughput (for fair comparison)

## Benchmark Configuration

All benchmarks use identical configurations for fair comparison:

| Parameter | Value |
|-----------|-------|
| Message Sizes | 64B, 1KB, 8KB, 64KB |
| Message Counts | 100K (64B), 50K (1KB), 10K (8KB), 1K (64KB) |
| High Water Mark | 0 (unlimited) |
| Transports | TCP, inproc, IPC (Unix only) |
| Compiler Flags | -O3 -std=c++20 -pthread |

## Results

Performance comparison results are available in:
- `../../benchmark_results/libzmq_vs_serverlink_comparison.md` - Detailed analysis
- `../../benchmark_results/performance_summary.txt` - Quick summary

## Key Findings

### ServerLink Advantages
- **inproc 8KB**: +66% faster (824K vs 497K msg/s)
- **inproc 64KB**: +36% faster (254K vs 187K msg/s)
- **TCP small messages**: +5% consistently
- **IPC PUB/SUB**: +14-16% for small/medium messages

### libzmq Advantages
- **TCP large messages**: Better for 8KB+ (up to -21%)
- **inproc 1KB**: -19% ROUTER, -17% PUB/SUB (8KB)

### Performance Parity
- inproc 64B (both patterns)
- IPC 1KB (both patterns)
- TCP 8KB PUB/SUB

## Dependencies

### For libzmq Benchmarks
- libzmq 4.3.5 installed at `/home/ulalax/project/ulalax/libzmq-native/deps/linux-x64/zeromq-4.3.5`
- GCC or Clang with C++17 support
- pthread

### For ServerLink Benchmarks
- ServerLink built with CMake (see main README)
- C++20 compiler

## CI Mode

Benchmarks detect CI environment (`CI` or `GITHUB_ACTIONS` env var) and reduce iteration counts by ~100x for faster runs:

| Mode | 64B | 1KB | 8KB | 64KB | Transports |
|------|-----|-----|-----|------|------------|
| Full | 100K | 50K | 10K | 1K | TCP, inproc, IPC |
| CI | 1K | 500 | 100 | 50 | TCP, inproc, IPC |

```bash
export CI=true
./run_serverlink.sh  # Uses reduced iteration counts
```

## Troubleshooting

### libzmq not found
```bash
# Verify libzmq installation
ls /home/ulalax/project/ulalax/libzmq-native/deps/linux-x64/zeromq-4.3.5/include/zmq.h
ls /home/ulalax/project/ulalax/libzmq-native/deps/linux-x64/zeromq-4.3.5/build/lib/libzmq.so.5.2.5
```

### ServerLink benchmarks not built
```bash
cd ../..
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build --parallel 8
```

### Port already in use (TCP benchmarks)
TCP benchmarks use ports 15556-15557. If busy, kill processes or wait a moment.

## Performance Notes

1. **inproc Performance**: ServerLink's memory ordering optimization (acq_rel → release/acquire) provides significant improvements for large messages
2. **TCP Performance**: Both libraries achieve excellent throughput; ServerLink optimized for small messages
3. **IPC Performance**: ServerLink shows advantages in PUB/SUB pattern
4. **Real-world Workloads**: Choose based on your specific message size and transport needs

## Further Reading

- [ServerLink Performance Optimizations](../../docs/impl/WINDOWS_FDSET_OPTIMIZATION.md)
- [C++20 Porting Complete](../../docs/CPP20_PORTING_COMPLETE.md)
- [Inproc Pipe Activation Bug Fix](../../FIX_INPROC_ACTIVATION_BUG.md)

## License

Both ServerLink and these comparison benchmarks are MPL-2.0 licensed.

---

**Last Updated**: 2026-01-03

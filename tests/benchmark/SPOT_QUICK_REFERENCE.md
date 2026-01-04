# SPOT Benchmark Quick Reference

## 🚀 Quick Start

```bash
# 1. Configure (first time only)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 2. Build
cmake --build build --target benchmark_spot --config Release

# 3. Run
cd build/tests/benchmark/Release
./bench_spot_throughput
./bench_spot_latency
./bench_spot_scalability
```

## 📊 Benchmark Files

| File | Purpose | Time | Key Metrics |
|------|---------|------|-------------|
| `bench_spot_throughput` | Throughput | ~30s | msg/s, MB/s |
| `bench_spot_latency` | Latency | ~2min | RTT (μs), p95, p99 |
| `bench_spot_scalability` | Scaling | ~1min | Topics, Subs, O(1) |

## 🎯 Expected Performance

### Throughput
```
Local (inproc):  18 GB/s @ 8KB
Remote (TCP):     2 GB/s @ 64KB
```

### Latency (RTT)
```
Local (inproc):  <1 μs
Remote (TCP):    50 μs
```

### Scalability
```
Topic Lookup:    O(1) - constant time
Subscriber Fanout: O(n) - linear
Multi-Topic:     O(n) - linear
```

## 🔧 Build Commands

### Windows (Visual Studio)
```powershell
cmake -B build -S . -A x64
cmake --build build --target benchmark_spot --config Release
```

### Linux/macOS (Make)
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
make benchmark_spot
```

### Individual Targets
```bash
cmake --build build --target bench_spot_throughput --config Release
cmake --build build --target bench_spot_latency --config Release
cmake --build build --target bench_spot_scalability --config Release
```

## 📈 Sample Output

### Throughput
```
Scenario            |   Message Size |  Message Count |        Time |     Throughput |    Bandwidth
------------------------------------------------------------------------------------------------
SPOT Local          |     8192 bytes |    10000 msgs |    43.21 ms |     231424 msg/s |  1846.61 MB/s
SPOT Remote (TCP)   |    65536 bytes |     1000 msgs |    32.45 ms |      30817 msg/s |  1976.06 MB/s
```

### Latency
```
Scenario            |   Message Size |      Average |          p50 |          p95 |          p99
------------------------------------------------------------------------------------------------
SPOT Local          |       64 bytes | avg:     0.85 us | p50:     0.80 us | p95:     1.20 us | p99:     1.50 us
SPOT Remote (TCP)   |     1024 bytes | avg:    52.34 us | p50:    50.12 us | p95:    65.78 us | p99:    78.90 us
```

### Scalability
```
--- Registry Lookup Performance (O(1) verification) ---
Registry Size   |      Lookups |            Time |     Lookup Rate
---------------------------------------------------------------
            100 |        10000 |        1.23 ms |  8130081 ops/s (0.123 μs/op)
         100000 |        10000 |        1.26 ms |  7936508 ops/s (0.126 μs/op)
```

## 🛠️ Troubleshooting

### "cmake: command not found"
```bash
# Windows: Add CMake to PATH or use full path
"C:\Program Files\CMake\bin\cmake.exe" -B build -S .
```

### Benchmarks not found after build
```bash
# Reconfigure CMake
rm -rf build/CMakeCache.txt
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Low performance
```bash
# Check build type (should be Release)
grep CMAKE_BUILD_TYPE build/CMakeCache.txt

# Rebuild with Release
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -march=native"
```

## 📋 Checklist

- [ ] CMake 3.10+ installed
- [ ] Compiler with C++11 support
- [ ] ServerLink library built
- [ ] Release build configuration
- [ ] No background CPU load
- [ ] Localhost not virtualized (for TCP tests)

## 🔍 CI Mode

```bash
# Set environment variable for CI mode (faster, less accurate)
export CI=1
./bench_spot_throughput  # Uses 1000 msgs instead of 100000
```

## 📚 Documentation

- **Full Guide**: `SPOT_BENCHMARK_README.md`
- **Summary**: `SPOT_BENCHMARK_SUMMARY.md`
- **Usage**: `USAGE.md` (general benchmarks)
- **Code**: `bench_spot_*.cpp`

## 💡 Tips

1. **First run may be slower** - OS caches warming up
2. **Run multiple times** - Average results for stability
3. **Close background apps** - For consistent measurements
4. **Use Release build** - 10-100x faster than Debug
5. **Check CPU frequency** - Disable power saving mode

---

**Quick Links**:
- [Full Documentation](SPOT_BENCHMARK_README.md)
- [Summary](SPOT_BENCHMARK_SUMMARY.md)
- [SPOT Tests](../spot/)
- [SPOT Examples](../../examples/spot_*.c)

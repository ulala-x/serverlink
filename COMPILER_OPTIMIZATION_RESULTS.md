# Compiler Optimization Results - ServerLink

**Date:** 2026-01-04
**Platform:** Linux x64 (Ubuntu 24.04, GCC 13.3.0)
**CPU:** Native (using -march=native)
**I/O Backend:** epoll

## Executive Summary

Applied aggressive compiler optimizations to ServerLink and measured performance improvements across throughput and latency benchmarks. Results show **mixed performance characteristics** with some improvements and some regressions.

## Compiler Flags Applied

### Baseline (Before)
```cmake
-O3
```

### Optimized (After)
```cmake
-O3                      # Maximum optimization level
-march=native            # Optimize for the host CPU architecture
-mtune=native            # Tune for the host CPU
-funroll-loops           # Unroll loops for better performance
-ftree-vectorize         # Enable auto-vectorization
-fomit-frame-pointer     # Omit frame pointer for slight speedup
-ffast-math              # Fast floating-point math
-fno-signed-zeros        # Assume no signed zeros
-fno-trapping-math       # Assume no FP exceptions

CMAKE_INTERPROCEDURAL_OPTIMIZATION=TRUE  # Link-time optimization (LTO)
```

---

## ROUTER-ROUTER Throughput Benchmark

### TCP Transport

| Message Size | Baseline Throughput | Optimized Throughput | Change | % Improvement |
|--------------|---------------------|----------------------|--------|---------------|
| 64 bytes     | 2,560,361 msg/s     | 4,763,089 msg/s      | +2.2M  | **+86.0%** ✅ |
| 1 KB         | 928,151 msg/s       | 510,706 msg/s        | -417K  | **-45.0%** ❌ |
| 8 KB         | 186,994 msg/s       | 182,329 msg/s        | -4.6K  | **-2.5%** ❌ |
| 64 KB        | 70,006 msg/s        | 53,149 msg/s         | -16.8K | **-24.1%** ❌ |

### inproc Transport

| Message Size | Baseline Throughput | Optimized Throughput | Change | % Improvement |
|--------------|---------------------|----------------------|--------|---------------|
| 64 bytes     | 4,889,601 msg/s     | 4,063,846 msg/s      | -825K  | **-16.9%** ❌ |
| 1 KB         | 1,560,398 msg/s     | 975,357 msg/s        | -585K  | **-37.5%** ❌ |
| 8 KB         | 703,663 msg/s       | 404,993 msg/s        | -298K  | **-42.4%** ❌ |
| 64 KB        | 192,712 msg/s       | 171,090 msg/s        | -21.6K | **-11.2%** ❌ |

### IPC Transport

| Message Size | Baseline Throughput | Optimized Throughput | Change | % Improvement |
|--------------|---------------------|----------------------|--------|---------------|
| 64 bytes     | 4,905,605 msg/s     | 3,410,388 msg/s      | -1.49M | **-30.5%** ❌ |
| 1 KB         | 1,076,601 msg/s     | 900,732 msg/s        | -175K  | **-16.3%** ❌ |
| 8 KB         | 231,979 msg/s       | 214,565 msg/s        | -17.4K | **-7.5%** ❌ |
| 64 KB        | 71,431 msg/s        | 68,815 msg/s         | -2.6K  | **-3.7%** ❌ |

---

## PUB/SUB Throughput Benchmark

### TCP Transport

| Message Size | Baseline Throughput | Optimized Throughput | Change | % Improvement |
|--------------|---------------------|----------------------|--------|---------------|
| 64 bytes     | 5,317,567 msg/s     | 5,171,141 msg/s      | -146K  | **-2.8%** ❌ |
| 1 KB         | 905,982 msg/s       | 877,060 msg/s        | -28.9K | **-3.2%** ❌ |
| 8 KB         | 192,141 msg/s       | 186,586 msg/s        | -5.5K  | **-2.9%** ❌ |
| 64 KB        | 59,361 msg/s        | 62,540 msg/s         | +3.1K  | **+5.4%** ✅ |

### inproc Transport

| Message Size | Baseline Throughput | Optimized Throughput | Change | % Improvement |
|--------------|---------------------|----------------------|--------|---------------|
| 64 bytes     | 4,833,688 msg/s     | 5,014,255 msg/s      | +180K  | **+3.7%** ✅ |
| 1 KB         | 1,671,195 msg/s     | 1,568,307 msg/s      | -102K  | **-6.2%** ❌ |
| 8 KB         | 697,410 msg/s       | 507,320 msg/s        | -190K  | **-27.3%** ❌ |
| 64 KB        | 188,644 msg/s       | 245,430 msg/s        | +56.7K | **+30.1%** ✅ |

### IPC Transport

| Message Size | Baseline Throughput | Optimized Throughput | Change | % Improvement |
|--------------|---------------------|----------------------|--------|---------------|
| 64 bytes     | 4,844,069 msg/s     | 5,047,933 msg/s      | +203K  | **+4.2%** ✅ |
| 1 KB         | 1,063,800 msg/s     | 1,060,112 msg/s      | -3.6K  | **-0.3%** ❌ |
| 8 KB         | 217,125 msg/s       | 217,068 msg/s        | -0.05K | **-0.03%** ~ |
| 64 KB        | 63,447 msg/s        | 65,026 msg/s         | +1.5K  | **+2.5%** ✅ |

---

## Fan-out Benchmark (1 PUB → N SUB)

### TCP Transport

| Subscribers | Baseline Throughput | Optimized Throughput | Change | % Improvement |
|-------------|---------------------|----------------------|--------|---------------|
| 2 subs      | 6,819,350 msg/s     | 6,114,075 msg/s      | -705K  | **-10.3%** ❌ |
| 4 subs      | 7,983,103 msg/s     | 7,814,889 msg/s      | -168K  | **-2.1%** ❌ |
| 8 subs      | 8,909,687 msg/s     | 9,109,578 msg/s      | +199K  | **+2.2%** ✅ |

### inproc Transport

| Subscribers | Baseline Throughput | Optimized Throughput | Change | % Improvement |
|-------------|---------------------|----------------------|--------|---------------|
| 2 subs      | 8,290,826 msg/s     | 8,496,722 msg/s      | +205K  | **+2.5%** ✅ |
| 4 subs      | 12,257,181 msg/s    | 13,202,187 msg/s     | +945K  | **+7.7%** ✅ |
| 8 subs      | 11,228,534 msg/s    | 8,531,711 msg/s      | -2.69M | **-24.0%** ❌ |

---

## Latency Benchmark (Round-Trip Time)

### TCP Transport

| Message Size | Baseline p50 | Optimized p50 | Change    | % Change |
|--------------|--------------|---------------|-----------|----------|
| 64 bytes     | 57.98 μs     | 65.28 μs      | +7.30 μs  | **+12.6%** ❌ |
| 1 KB         | 61.68 μs     | 61.01 μs      | -0.67 μs  | **-1.1%** ✅ |
| 8 KB         | 66.88 μs     | 70.87 μs      | +3.99 μs  | **+6.0%** ❌ |

### inproc Transport

| Message Size | Baseline p50 | Optimized p50 | Change    | % Change |
|--------------|--------------|---------------|-----------|----------|
| 64 bytes     | 23.83 μs     | 24.11 μs      | +0.28 μs  | **+1.2%** ❌ |
| 1 KB         | 24.32 μs     | 25.09 μs      | +0.77 μs  | **+3.2%** ❌ |
| 8 KB         | 25.54 μs     | 25.80 μs      | +0.26 μs  | **+1.0%** ❌ |

### IPC Transport

| Message Size | Baseline p50 | Optimized p50 | Change    | % Change |
|--------------|--------------|---------------|-----------|----------|
| 64 bytes     | 53.49 μs     | 54.98 μs      | +1.49 μs  | **+2.8%** ❌ |
| 1 KB         | 54.06 μs     | 54.36 μs      | +0.30 μs  | **+0.6%** ❌ |
| 8 KB         | 62.70 μs     | 63.27 μs      | +0.57 μs  | **+0.9%** ❌ |

---

## Analysis

### Positive Results ✅
1. **TCP small messages (64B)**: +86% improvement in ROUTER throughput
2. **PUB/SUB large messages (64KB)**: +30.1% for inproc
3. **Fan-out scalability**: +7.7% for 4-subscriber inproc

### Negative Results ❌
1. **Medium-to-large message throughput**: -37% to -45% for ROUTER 1KB-64KB
2. **inproc ROUTER**: Significant regressions across all message sizes
3. **Latency**: Small regressions (1-12%) across most tests

### Root Causes (Hypothesis)

The mixed results suggest that aggressive optimizations may be:

1. **-ffast-math side effects**: May affect lock-free queue timing precision
2. **-march=native code bloat**: Potential instruction cache pressure
3. **Loop unrolling backfire**: May hurt CPU branch predictor on hot paths
4. **LTO interactions**: Whole-program optimization may inline critical sections poorly
5. **Frame pointer omission**: May affect exception handling paths used in error cases

### Key Insights

1. **Small messages benefit** from aggressive optimization (vectorization, CPU-specific instructions)
2. **Larger messages suffer** - suggests memory-bound workloads don't benefit from CPU optimizations
3. **Latency regressions** indicate potential timing-sensitive code affected by optimizations

---

## Recommendation

**DO NOT apply these aggressive optimizations** to the production build for the following reasons:

1. **Net negative impact**: More regressions than improvements
2. **Latency degradation**: Critical for real-time messaging systems
3. **Reproducibility loss**: `-march=native` makes builds non-portable
4. **Code stability**: `-ffast-math` may violate assumptions in lock-free code

### Conservative Alternative

Keep the current baseline optimization:
```cmake
-O3  # Provides good balance of speed and code quality
```

### Selective Optimization Strategy

If optimizations are desired, apply them selectively:

1. **Keep:** `-O3`, `-mtune=native` (portable tuning hints)
2. **Remove:** `-march=native` (breaks portability), `-ffast-math` (breaks correctness)
3. **Profile-Guided Optimization (PGO)**: More effective than blind flags
4. **Manual hot-path optimization**: Focus on specific bottlenecks

---

## Conclusion

Aggressive compiler optimizations produced **mixed results** with an overall **negative impact** on ServerLink performance. The baseline `-O3` optimization level provides the best balance of performance, portability, and stability.

**Recommendation:** Revert to baseline optimization flags.

## Files Modified

- `/home/ulalax/project/ulalax/serverlink/cmake/platform.cmake` (to be reverted)

---

**Test Environment:**
- OS: Linux 6.6.87.2-microsoft-standard-WSL2
- Compiler: GCC 13.3.0
- Build Type: Release
- I/O Backend: epoll

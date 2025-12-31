/* SPDX-License-Identifier: MPL-2.0 */

#ifndef BENCH_COMMON_HPP
#define BENCH_COMMON_HPP

#include <serverlink/serverlink.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>

// High-resolution time measurement utility
class stopwatch_t {
public:
    void start() {
        _start = std::chrono::high_resolution_clock::now();
    }

    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - _start).count();
    }

    double elapsed_us() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - _start).count();
    }

private:
    std::chrono::high_resolution_clock::time_point _start;
};

// Benchmark parameters structure
struct bench_params_t {
    size_t message_size;
    int message_count;
    const char *transport;  // "tcp", "ipc", "inproc"
};

// Print throughput benchmark results
inline void print_throughput_result(const char *test_name,
                                    const bench_params_t &params,
                                    double elapsed_ms) {
    double msgs_per_sec = params.message_count / (elapsed_ms / 1000.0);
    double mb_per_sec = (params.message_count * params.message_size) /
                        (elapsed_ms / 1000.0) / (1024.0 * 1024.0);

    printf("%-20s | %8zu bytes | %8d msgs | %8.2f ms | %10.0f msg/s | %8.2f MB/s\n",
           test_name, params.message_size, params.message_count,
           elapsed_ms, msgs_per_sec, mb_per_sec);
}

// Print latency benchmark results with percentiles
inline void print_latency_result(const char *test_name,
                                 const bench_params_t &params,
                                 const std::vector<double> &latencies_us) {
    if (latencies_us.empty()) {
        printf("%-20s | %8zu bytes | ERROR: No latency data\n", test_name, params.message_size);
        return;
    }

    // Sort for percentile calculation
    std::vector<double> sorted = latencies_us;
    std::sort(sorted.begin(), sorted.end());

    // Calculate average
    double avg = 0;
    for (double l : sorted) avg += l;
    avg /= sorted.size();

    // Calculate percentiles
    size_t p50_idx = sorted.size() * 50 / 100;
    size_t p95_idx = sorted.size() * 95 / 100;
    size_t p99_idx = sorted.size() * 99 / 100;

    // Ensure indices are within bounds
    if (p50_idx >= sorted.size()) p50_idx = sorted.size() - 1;
    if (p95_idx >= sorted.size()) p95_idx = sorted.size() - 1;
    if (p99_idx >= sorted.size()) p99_idx = sorted.size() - 1;

    printf("%-20s | %8zu bytes | avg: %8.2f us | p50: %8.2f us | p95: %8.2f us | p99: %8.2f us\n",
           test_name, params.message_size, avg,
           sorted[p50_idx], sorted[p95_idx], sorted[p99_idx]);
}

// Error checking macro with detailed output
#define BENCH_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "BENCH_ASSERT failed: %s (%s:%d)\n", \
                    #cond, __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

// Extended error checking with error code
#define BENCH_CHECK(expr, msg) \
    do { \
        int rc = (expr); \
        if (rc != 0) { \
            fprintf(stderr, "BENCH_CHECK failed: %s returned %d (%s:%d)\n", \
                    msg, rc, __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

#endif // BENCH_COMMON_HPP

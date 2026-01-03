/* SPDX-License-Identifier: MPL-2.0 */
/* libzmq PUB-SUB throughput benchmark - for fair comparison with ServerLink */

#include "/home/ulalax/project/ulalax/libzmq-native/deps/linux-x64/zeromq-4.3.5/include/zmq.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

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

private:
    std::chrono::high_resolution_clock::time_point _start;
};

struct bench_params_t {
    size_t message_size;
    int message_count;
    const char *transport;
};

void print_throughput_result(const char *test_name,
                              const bench_params_t &params,
                              double elapsed_ms) {
    double msgs_per_sec = params.message_count / (elapsed_ms / 1000.0);
    double mb_per_sec = (params.message_count * params.message_size) /
                        (elapsed_ms / 1000.0) / (1024.0 * 1024.0);

    printf("%-20s | %8zu bytes | %8d msgs | %8.2f ms | %10.0f msg/s | %8.2f MB/s\n",
           test_name, params.message_size, params.message_count,
           elapsed_ms, msgs_per_sec, mb_per_sec);
}

#define BENCH_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "BENCH_ASSERT failed: %s (%s:%d)\n", \
                    #cond, __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

// Publisher thread: sends messages as fast as possible
void run_publisher(void *pub, const bench_params_t &params) {
    std::vector<char> data(params.message_size, 'A');

    for (int i = 0; i < params.message_count; i++) {
        int rc = zmq_send(pub, data.data(), data.size(), 0);
        BENCH_ASSERT(rc == static_cast<int>(data.size()));
    }
}

// Subscriber thread: receives messages and measures time
void run_subscriber(void *sub, const bench_params_t &params,
                    double *elapsed_ms) {
    std::vector<char> buf(params.message_size);

    stopwatch_t sw;
    sw.start();

    for (int i = 0; i < params.message_count; i++) {
        int rc = zmq_recv(sub, buf.data(), buf.size(), 0);
        BENCH_ASSERT(rc == static_cast<int>(params.message_size));
    }

    *elapsed_ms = sw.elapsed_ms();
}

// TCP PUB/SUB benchmark (1:1)
void bench_pubsub_tcp(const bench_params_t &params) {
    void *ctx = zmq_ctx_new();
    BENCH_ASSERT(ctx);

    // Use XPUB instead of PUB for subscription synchronization
    void *pub = zmq_socket(ctx, ZMQ_XPUB);
    void *sub = zmq_socket(ctx, ZMQ_SUB);
    BENCH_ASSERT(pub && sub);

    // Set HWM to 0 (unlimited) for benchmarking
    int hwm = 0;
    int rc = zmq_setsockopt(pub, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(sub, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);

    // Bind publisher first
    rc = zmq_bind(pub, "tcp://127.0.0.1:16557");
    BENCH_ASSERT(rc == 0);

    // Connect subscriber
    rc = zmq_connect(sub, "tcp://127.0.0.1:16557");
    BENCH_ASSERT(rc == 0);

    // Subscribe to all messages (AFTER connect for TCP)
    rc = zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "", 0);
    BENCH_ASSERT(rc == 0);

    // CRITICAL: Wait for XPUB to receive subscription notification
    char sub_msg[32];

    // Poll for subscription with retries (TCP may need a moment to propagate)
    int retries = 100; // 100 * 10ms = 1 second max wait
    do {
        rc = zmq_recv(pub, sub_msg, sizeof(sub_msg), ZMQ_DONTWAIT);
        if (rc > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (--retries > 0);

    BENCH_ASSERT(rc > 0 && sub_msg[0] == 1); // subscription message: [0x01][topic...]

    // Run benchmark
    double elapsed_ms = 0;
    std::thread sub_thread(run_subscriber, sub, std::cref(params), &elapsed_ms);
    std::thread pub_thread(run_publisher, pub, std::cref(params));

    pub_thread.join();
    sub_thread.join();

    print_throughput_result("PUB/SUB TCP", params, elapsed_ms);

    zmq_close(sub);
    zmq_close(pub);
    zmq_ctx_destroy(ctx);
}

// inproc PUB/SUB benchmark (1:1)
void bench_pubsub_inproc(const bench_params_t &params) {
    void *ctx = zmq_ctx_new();
    BENCH_ASSERT(ctx);

    // For inproc, use regular PUB/SUB (subscription is synchronous)
    void *pub = zmq_socket(ctx, ZMQ_PUB);
    void *sub = zmq_socket(ctx, ZMQ_SUB);
    BENCH_ASSERT(pub && sub);

    // Set HWM to 0 (unlimited) for benchmarking
    // For inproc, must set BOTH sndhwm and rcvhwm on BOTH sockets
    int hwm = 0;
    int rc = zmq_setsockopt(pub, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(pub, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(sub, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(sub, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);

    // Bind publisher first
    rc = zmq_bind(pub, "inproc://bench_pubsub");
    BENCH_ASSERT(rc == 0);

    // Connect subscriber
    rc = zmq_connect(sub, "inproc://bench_pubsub");
    BENCH_ASSERT(rc == 0);

    // Subscribe to all messages
    rc = zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "", 0);
    BENCH_ASSERT(rc == 0);

    // Small delay for subscription to propagate
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Run benchmark with multithreaded approach
    double elapsed_ms = 0;
    std::thread sub_thread(run_subscriber, sub, std::cref(params), &elapsed_ms);
    std::thread pub_thread(run_publisher, pub, std::cref(params));

    pub_thread.join();
    sub_thread.join();

    print_throughput_result("PUB/SUB inproc", params, elapsed_ms);

    zmq_close(sub);
    zmq_close(pub);
    zmq_ctx_destroy(ctx);
}

#ifdef __linux__
// IPC PUB/SUB benchmark (1:1)
void bench_pubsub_ipc(const bench_params_t &params) {
    void *ctx = zmq_ctx_new();
    BENCH_ASSERT(ctx);

    // Use XPUB instead of PUB for subscription synchronization
    void *pub = zmq_socket(ctx, ZMQ_XPUB);
    void *sub = zmq_socket(ctx, ZMQ_SUB);
    BENCH_ASSERT(pub && sub);

    // Set HWM to 0 (unlimited) for benchmarking
    int hwm = 0;
    int rc = zmq_setsockopt(pub, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(sub, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);

    // Bind publisher first
    rc = zmq_bind(pub, "ipc:///tmp/bench_zmq_pubsub.ipc");
    BENCH_ASSERT(rc == 0);

    // Connect subscriber
    rc = zmq_connect(sub, "ipc:///tmp/bench_zmq_pubsub.ipc");
    BENCH_ASSERT(rc == 0);

    // Subscribe to all messages (AFTER connect)
    rc = zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "", 0);
    BENCH_ASSERT(rc == 0);

    // CRITICAL: Wait for XPUB to receive subscription notification
    char sub_msg[32];

    // Poll for subscription with retries
    int retries = 100; // 100 * 10ms = 1 second max wait
    do {
        rc = zmq_recv(pub, sub_msg, sizeof(sub_msg), ZMQ_DONTWAIT);
        if (rc > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (--retries > 0);

    BENCH_ASSERT(rc > 0 && sub_msg[0] == 1); // subscription message: [0x01][topic...]

    // Run benchmark
    double elapsed_ms = 0;
    std::thread sub_thread(run_subscriber, sub, std::cref(params), &elapsed_ms);
    std::thread pub_thread(run_publisher, pub, std::cref(params));

    pub_thread.join();
    sub_thread.join();

    print_throughput_result("PUB/SUB IPC", params, elapsed_ms);

    zmq_close(sub);
    zmq_close(pub);
    zmq_ctx_destroy(ctx);

    // Cleanup IPC file
    remove("/tmp/bench_zmq_pubsub.ipc");
}
#endif

int main() {
    printf("\n=== libzmq PUB-SUB Throughput Benchmark ===\n\n");
    printf("%-20s | %14s | %13s | %11s | %14s | %12s\n",
           "Transport", "Message Size", "Message Count", "Time", "Throughput", "Bandwidth");
    printf("------------------------------------------------------------------------------------");
    printf("----------\n");

    // Check for CI environment - use reduced iterations
    const char *ci_env = std::getenv("CI");
    const char *github_actions = std::getenv("GITHUB_ACTIONS");
    bool is_ci = (ci_env != nullptr) || (github_actions != nullptr);

    // Test with various message sizes
    // Match ServerLink benchmark exactly
    size_t sizes[] = {64, 1024, 8192, 65536};
    int counts_full[] = {100000, 50000, 10000, 1000};
    int counts_ci[] = {1000, 500, 100, 50};

    int *counts = is_ci ? counts_ci : counts_full;

    if (is_ci) {
        printf("CI mode: using reduced iteration counts\n\n");
    }

    for (size_t i = 0; i < 4; i++) {
        bench_params_t params = {sizes[i], counts[i], "pubsub"};

        if (!is_ci) {
            bench_pubsub_tcp(params);
        }
        bench_pubsub_inproc(params);

#ifdef __linux__
        if (!is_ci) {
            bench_pubsub_ipc(params);
        }
#endif
        printf("\n");
    }

    printf("Benchmark completed.\n\n");

    return 0;
}

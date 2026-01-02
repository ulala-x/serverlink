/* SPDX-License-Identifier: MPL-2.0 */

#include <serverlink/config.h>
#include "bench_common.hpp"
#include <thread>
#include <vector>

// Print fan-out benchmark results
static void print_fanout_result(const char *test_name,
                                 int num_subscribers,
                                 const bench_params_t &params,
                                 double elapsed_ms) {
    int total_messages = params.message_count * num_subscribers;
    double msgs_per_sec = total_messages / (elapsed_ms / 1000.0);
    double mb_per_sec = (total_messages * params.message_size) /
                        (elapsed_ms / 1000.0) / (1024.0 * 1024.0);

    printf("%-20s | %4d subs | %8zu bytes | %8d msgs | %10.0f msg/s | %8.2f MB/s\n",
           test_name, num_subscribers, params.message_size, total_messages,
           msgs_per_sec, mb_per_sec);
}

// Publisher thread: sends messages as fast as possible
void run_publisher(slk_socket_t *pub, const bench_params_t &params) {
    std::vector<char> data(params.message_size, 'A');

    for (int i = 0; i < params.message_count; i++) {
        int rc = slk_send(pub, data.data(), data.size(), 0);
        BENCH_ASSERT(rc == static_cast<int>(data.size()));
    }
}

// Subscriber thread: receives messages and measures time
void run_subscriber(slk_socket_t *sub, const bench_params_t &params,
                    double *elapsed_ms) {
    std::vector<char> buf(params.message_size);

    stopwatch_t sw;
    sw.start();

    for (int i = 0; i < params.message_count; i++) {
        int rc = slk_recv(sub, buf.data(), buf.size(), 0);
        BENCH_ASSERT(rc == static_cast<int>(params.message_size));
    }

    *elapsed_ms = sw.elapsed_ms();
}

// Fan-out subscriber: simple receive without timing
void run_fanout_subscriber(slk_socket_t *sub, const bench_params_t &params) {
    std::vector<char> buf(params.message_size);

    for (int i = 0; i < params.message_count; i++) {
        int rc = slk_recv(sub, buf.data(), buf.size(), 0);
        BENCH_ASSERT(rc == static_cast<int>(params.message_size));
    }
}

// TCP PUB/SUB benchmark (1:1)
void bench_pubsub_tcp(const bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    // Use XPUB instead of PUB for subscription synchronization
    slk_socket_t *pub = slk_socket(ctx, SLK_XPUB);
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    BENCH_ASSERT(pub && sub);

    // Set HWM to 0 (unlimited) for benchmarking
    int hwm = 0;
    int rc = slk_setsockopt(pub, SLK_SNDHWM, &hwm, sizeof(hwm));
    BENCH_CHECK(rc, "slk_setsockopt(pub SLK_SNDHWM)");
    rc = slk_setsockopt(sub, SLK_RCVHWM, &hwm, sizeof(hwm));
    BENCH_CHECK(rc, "slk_setsockopt(sub SLK_RCVHWM)");

    // Bind publisher first
    rc = slk_bind(pub, "tcp://127.0.0.1:16555");
    BENCH_CHECK(rc, "slk_bind");

    // Connect subscriber first
    rc = slk_connect(sub, "tcp://127.0.0.1:16555");
    BENCH_CHECK(rc, "slk_connect");

    // Subscribe to all messages (AFTER connect for TCP)
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    BENCH_CHECK(rc, "slk_setsockopt(SLK_SUBSCRIBE)");

    // CRITICAL: Wait for XPUB to receive subscription notification
    // This ensures the subscriber is ready before publishing starts
    char sub_msg[32];

    // Poll for subscription with retries (TCP may need a moment to propagate)
    int retries = 100; // 100 * 10ms = 1 second max wait
    do {
        rc = slk_recv(pub, sub_msg, sizeof(sub_msg), SLK_DONTWAIT);
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

    slk_close(sub);
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

// inproc PUB/SUB benchmark (1:1)
void bench_pubsub_inproc(const bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    // For inproc, use regular PUB/SUB (subscription is synchronous)
    slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    BENCH_ASSERT(pub && sub);

    // Set HWM to 0 (unlimited) for benchmarking
    // CRITICAL: For inproc, must set BOTH sndhwm and rcvhwm on BOTH sockets
    // because pipe HWM is determined by cross-socket values during connection
    int hwm = 0;
    int rc = slk_setsockopt(pub, SLK_SNDHWM, &hwm, sizeof(hwm));
    BENCH_CHECK(rc, "slk_setsockopt(pub SLK_SNDHWM)");
    rc = slk_setsockopt(pub, SLK_RCVHWM, &hwm, sizeof(hwm));
    BENCH_CHECK(rc, "slk_setsockopt(pub SLK_RCVHWM)");
    rc = slk_setsockopt(sub, SLK_SNDHWM, &hwm, sizeof(hwm));
    BENCH_CHECK(rc, "slk_setsockopt(sub SLK_SNDHWM)");
    rc = slk_setsockopt(sub, SLK_RCVHWM, &hwm, sizeof(hwm));
    BENCH_CHECK(rc, "slk_setsockopt(sub SLK_RCVHWM)");

    // Bind publisher first
    rc = slk_bind(pub, "inproc://bench_pubsub");
    BENCH_CHECK(rc, "slk_bind");

    // Connect subscriber
    rc = slk_connect(sub, "inproc://bench_pubsub");
    BENCH_CHECK(rc, "slk_connect");

    // Subscribe to all messages
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    BENCH_CHECK(rc, "slk_setsockopt(SLK_SUBSCRIBE)");

    // Small delay for subscription to propagate (inproc is synchronous but safer to wait)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Run benchmark with multithreaded approach
    double elapsed_ms = 0;
    std::thread sub_thread(run_subscriber, sub, std::cref(params), &elapsed_ms);
    std::thread pub_thread(run_publisher, pub, std::cref(params));

    pub_thread.join();
    sub_thread.join();

    print_throughput_result("PUB/SUB inproc", params, elapsed_ms);

    slk_close(sub);
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

#ifdef __linux__
// IPC PUB/SUB benchmark (1:1)
void bench_pubsub_ipc(const bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    // Use XPUB instead of PUB for subscription synchronization
    slk_socket_t *pub = slk_socket(ctx, SLK_XPUB);
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    BENCH_ASSERT(pub && sub);

    // Set HWM to 0 (unlimited) for benchmarking
    int hwm = 0;
    int rc = slk_setsockopt(pub, SLK_SNDHWM, &hwm, sizeof(hwm));
    BENCH_CHECK(rc, "slk_setsockopt(pub SLK_SNDHWM)");
    rc = slk_setsockopt(sub, SLK_RCVHWM, &hwm, sizeof(hwm));
    BENCH_CHECK(rc, "slk_setsockopt(sub SLK_RCVHWM)");

    // Bind publisher first
    rc = slk_bind(pub, "ipc:///tmp/bench_pubsub.ipc");
    BENCH_CHECK(rc, "slk_bind");

    // Connect subscriber first
    rc = slk_connect(sub, "ipc:///tmp/bench_pubsub.ipc");
    BENCH_CHECK(rc, "slk_connect");

    // Subscribe to all messages (AFTER connect)
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
    BENCH_CHECK(rc, "slk_setsockopt(SLK_SUBSCRIBE)");

    // CRITICAL: Wait for XPUB to receive subscription notification
    char sub_msg[32];

    // Poll for subscription with retries
    int retries = 100; // 100 * 10ms = 1 second max wait
    do {
        rc = slk_recv(pub, sub_msg, sizeof(sub_msg), SLK_DONTWAIT);
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

    slk_close(sub);
    slk_close(pub);
    slk_ctx_destroy(ctx);

    // Cleanup IPC file
    remove("/tmp/bench_pubsub.ipc");
}
#endif

// Fan-out benchmark: 1 PUB → N SUB (TCP)
void bench_pubsub_fanout_tcp(int num_subscribers, const bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    // Use XPUB instead of PUB for subscription synchronization
    slk_socket_t *pub = slk_socket(ctx, SLK_XPUB);
    BENCH_ASSERT(pub);

    // Set HWM to 0 (unlimited) for benchmarking
    int hwm = 0;
    int rc = slk_setsockopt(pub, SLK_SNDHWM, &hwm, sizeof(hwm));
    BENCH_CHECK(rc, "slk_setsockopt(pub SLK_SNDHWM)");

    // Bind publisher
    rc = slk_bind(pub, "tcp://127.0.0.1:16556");
    BENCH_CHECK(rc, "slk_bind");

    // Create and connect all subscribers
    std::vector<slk_socket_t*> subs(num_subscribers);
    for (int i = 0; i < num_subscribers; i++) {
        subs[i] = slk_socket(ctx, SLK_SUB);
        BENCH_ASSERT(subs[i]);

        rc = slk_setsockopt(subs[i], SLK_RCVHWM, &hwm, sizeof(hwm));
        BENCH_CHECK(rc, "slk_setsockopt(sub SLK_RCVHWM)");

        // Connect first
        rc = slk_connect(subs[i], "tcp://127.0.0.1:16556");
        BENCH_CHECK(rc, "slk_connect");

        // Subscribe AFTER connect
        rc = slk_setsockopt(subs[i], SLK_SUBSCRIBE, "", 0);
        BENCH_CHECK(rc, "slk_setsockopt(SLK_SUBSCRIBE)");
    }

    // CRITICAL: Wait for XPUB subscription notification
    // Note: When multiple SUBs subscribe to the same topic (""), XPUB may only
    // send one subscription message, so we just wait for at least one
    char sub_msg[32];

    // Poll for subscription with retries
    int retries = 100; // 100 * 10ms = 1 second max wait
    do {
        rc = slk_recv(pub, sub_msg, sizeof(sub_msg), SLK_DONTWAIT);
        if (rc > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (--retries > 0);

    BENCH_ASSERT(rc > 0 && sub_msg[0] == 1); // subscription message: [0x01][topic...]

    // Launch subscriber threads
    std::vector<std::thread> sub_threads;
    for (int i = 0; i < num_subscribers; i++) {
        sub_threads.emplace_back(run_fanout_subscriber, subs[i], std::cref(params));
    }

    // Start timing and publish messages
    stopwatch_t sw;
    sw.start();

    std::thread pub_thread(run_publisher, pub, std::cref(params));
    pub_thread.join();

    // Wait for all subscribers to finish
    for (auto &t : sub_threads) {
        t.join();
    }

    double elapsed_ms = sw.elapsed_ms();
    print_fanout_result("Fan-out TCP", num_subscribers, params, elapsed_ms);

    // Cleanup
    for (auto sub : subs) {
        slk_close(sub);
    }
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

// Fan-out benchmark: 1 PUB → N SUB (inproc)
void bench_pubsub_fanout_inproc(int num_subscribers, const bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    // For inproc, use regular PUB (subscription is synchronous)
    slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
    BENCH_ASSERT(pub);

    // Set HWM to 0 (unlimited) for benchmarking
    // CRITICAL: For inproc, must set BOTH sndhwm and rcvhwm on ALL sockets
    int hwm = 0;
    int rc = slk_setsockopt(pub, SLK_SNDHWM, &hwm, sizeof(hwm));
    BENCH_CHECK(rc, "slk_setsockopt(pub SLK_SNDHWM)");
    rc = slk_setsockopt(pub, SLK_RCVHWM, &hwm, sizeof(hwm));
    BENCH_CHECK(rc, "slk_setsockopt(pub SLK_RCVHWM)");

    // Bind publisher
    rc = slk_bind(pub, "inproc://bench_pubsub_fanout");
    BENCH_CHECK(rc, "slk_bind");

    // Create and connect all subscribers
    std::vector<slk_socket_t*> subs(num_subscribers);
    for (int i = 0; i < num_subscribers; i++) {
        subs[i] = slk_socket(ctx, SLK_SUB);
        BENCH_ASSERT(subs[i]);

        rc = slk_setsockopt(subs[i], SLK_SNDHWM, &hwm, sizeof(hwm));
        BENCH_CHECK(rc, "slk_setsockopt(sub SLK_SNDHWM)");
        rc = slk_setsockopt(subs[i], SLK_RCVHWM, &hwm, sizeof(hwm));
        BENCH_CHECK(rc, "slk_setsockopt(sub SLK_RCVHWM)");

        rc = slk_connect(subs[i], "inproc://bench_pubsub_fanout");
        BENCH_CHECK(rc, "slk_connect");

        rc = slk_setsockopt(subs[i], SLK_SUBSCRIBE, "", 0);
        BENCH_CHECK(rc, "slk_setsockopt(SLK_SUBSCRIBE)");
    }

    // Small delay for subscriptions to propagate (inproc is synchronous but safer to wait)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Launch subscriber threads
    std::vector<std::thread> sub_threads;
    for (int i = 0; i < num_subscribers; i++) {
        sub_threads.emplace_back(run_fanout_subscriber, subs[i], std::cref(params));
    }

    // Start timing and publish messages
    stopwatch_t sw;
    sw.start();

    std::thread pub_thread(run_publisher, pub, std::cref(params));
    pub_thread.join();

    // Wait for all subscribers to finish
    for (auto &t : sub_threads) {
        t.join();
    }

    double elapsed_ms = sw.elapsed_ms();
    print_fanout_result("Fan-out inproc", num_subscribers, params, elapsed_ms);

    // Cleanup
    for (auto sub : subs) {
        slk_close(sub);
    }
    slk_close(pub);
    slk_ctx_destroy(ctx);
}

int main() {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("\n=== ServerLink PUB/SUB Benchmark ===\n\n");
    printf("%-20s | %14s | %13s | %11s | %14s | %12s\n",
           "Transport", "Message Size", "Message Count", "Time", "Throughput", "Bandwidth");
    printf("------------------------------------------------------------------------------------");
    printf("----------\n");

    // Test with various message sizes
    // Larger messages = fewer iterations (to keep test time reasonable)
    size_t sizes[] = {64, 1024, 8192, 65536};
    int counts[] = {100000, 50000, 10000, 1000};

    for (size_t i = 0; i < 4; i++) {
        bench_params_t params = {sizes[i], counts[i], "pubsub"};

        bench_pubsub_tcp(params);
        bench_pubsub_inproc(params);

#if defined(SL_HAVE_IPC) && defined(__linux__)
        bench_pubsub_ipc(params);
#endif
        printf("\n");
    }

    // Fan-out benchmarks (1 PUB → N SUB)
    printf("\n=== Fan-out Benchmark (1 PUB → N SUB) ===\n\n");
    printf("%-20s | %8s | %14s | %13s | %14s | %12s\n",
           "Transport", "Subs", "Message Size", "Total Msgs", "Throughput", "Bandwidth");
    printf("------------------------------------------------------------------------------------");
    printf("----------\n");

    // Test fan-out with different subscriber counts
    int sub_counts[] = {2, 4, 8};

    // Use smaller message size and count for fan-out to keep test time reasonable
    bench_params_t fanout_params = {64, 10000, "fanout"};

    for (size_t i = 0; i < 3; i++) {
        bench_pubsub_fanout_tcp(sub_counts[i], fanout_params);
        bench_pubsub_fanout_inproc(sub_counts[i], fanout_params);
    }
    printf("\n");

    printf("Benchmark completed.\n\n");

    return 0;
}

/* SPDX-License-Identifier: MPL-2.0 */

#include "bench_common.hpp"
#include <thread>
#include <atomic>

std::atomic<bool> g_spot_running(true);

// Echo server thread for latency measurement
void spot_echo_server(slk_spot_t *spot, const char *topic_id) {
    char topic[64], data[65536];

    while (g_spot_running) {
        size_t tlen, dlen;
        int rc = slk_spot_recv(spot, topic, sizeof(topic), &tlen,
                              data, sizeof(data), &dlen, 100);  // 100ms timeout
        if (rc != 0) continue;

        // Echo back the message
        rc = slk_spot_publish(spot, topic_id, data, dlen);
        if (rc != 0) {
            fprintf(stderr, "Echo publish failed\n");
        }
    }
}

// Local latency benchmark (inproc, single process)
void bench_spot_local_latency(size_t message_size, int message_count) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    // Create two SPOT instances for ping-pong
    slk_spot_t *spot_a = slk_spot_new(ctx);
    slk_spot_t *spot_b = slk_spot_new(ctx);
    BENCH_ASSERT(spot_a && spot_b);

    // Setup bidirectional topics
    int rc;
    rc = slk_spot_topic_create(spot_a, "bench:ping");
    BENCH_CHECK(rc, "create ping topic");
    rc = slk_spot_topic_create(spot_b, "bench:pong");
    BENCH_CHECK(rc, "create pong topic");

    // Cross-subscribe
    rc = slk_spot_subscribe(spot_a, "bench:pong");
    BENCH_CHECK(rc, "subscribe to pong");
    rc = slk_spot_subscribe(spot_b, "bench:ping");
    BENCH_CHECK(rc, "subscribe to ping");

    // Start echo server thread
    g_spot_running = true;
    std::thread echo_thread(spot_echo_server, spot_b, "bench:pong");

    // Prepare test data
    std::vector<char> data(message_size, 'A');
    std::vector<double> latencies;
    latencies.reserve(message_count);

    // Warmup phase
    for (int i = 0; i < 100; i++) {
        slk_spot_publish(spot_a, "bench:ping", data.data(), data.size());
        char topic[64], buf[65536];
        size_t tlen, dlen;
        slk_spot_recv(spot_a, topic, sizeof(topic), &tlen,
                     buf, sizeof(buf), &dlen, 0);
    }

    // Actual benchmark
    for (int i = 0; i < message_count; i++) {
        stopwatch_t sw;
        sw.start();

        // Send ping
        rc = slk_spot_publish(spot_a, "bench:ping", data.data(), data.size());
        BENCH_ASSERT(rc == 0);

        // Wait for pong
        char topic[64], buf[65536];
        size_t tlen, dlen;
        rc = slk_spot_recv(spot_a, topic, sizeof(topic), &tlen,
                          buf, sizeof(buf), &dlen, 0);
        BENCH_ASSERT(rc == 0);
        BENCH_ASSERT(dlen == message_size);

        latencies.push_back(sw.elapsed_us());
    }

    print_latency_result("SPOT Local", {message_size, message_count, "local"}, latencies);

    // Cleanup
    g_spot_running = false;
    echo_thread.join();

    slk_spot_destroy(&spot_a);
    slk_spot_destroy(&spot_b);
    slk_ctx_destroy(ctx);
}

// Remote latency benchmark (TCP)
void bench_spot_remote_latency(size_t message_size, int message_count) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    // Create two SPOT instances communicating over TCP
    slk_spot_t *spot_a = slk_spot_new(ctx);
    slk_spot_t *spot_b = slk_spot_new(ctx);
    BENCH_ASSERT(spot_a && spot_b);

    // Setup bidirectional topics over TCP
    int rc;

    // spot_a creates "ping" topic and binds
    rc = slk_spot_topic_create(spot_a, "bench:ping");
    BENCH_CHECK(rc, "create ping topic");
    rc = slk_spot_bind(spot_a, "tcp://127.0.0.1:15601");
    BENCH_CHECK(rc, "bind spot_a");

    // spot_b creates "pong" topic and binds
    rc = slk_spot_topic_create(spot_b, "bench:pong");
    BENCH_CHECK(rc, "create pong topic");
    rc = slk_spot_bind(spot_b, "tcp://127.0.0.1:15602");
    BENCH_CHECK(rc, "bind spot_b");

    // Cross-route and subscribe
    rc = slk_spot_topic_route(spot_a, "bench:pong", "tcp://127.0.0.1:15602");
    BENCH_CHECK(rc, "route pong to spot_a");
    rc = slk_spot_subscribe(spot_a, "bench:pong");
    BENCH_CHECK(rc, "subscribe to pong");

    rc = slk_spot_topic_route(spot_b, "bench:ping", "tcp://127.0.0.1:15601");
    BENCH_CHECK(rc, "route ping to spot_b");
    rc = slk_spot_subscribe(spot_b, "bench:ping");
    BENCH_CHECK(rc, "subscribe to ping");

    // Wait for connections
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Start echo server thread
    g_spot_running = true;
    std::thread echo_thread(spot_echo_server, spot_b, "bench:pong");

    // Prepare test data
    std::vector<char> data(message_size, 'A');
    std::vector<double> latencies;
    latencies.reserve(message_count);

    // Warmup phase
    for (int i = 0; i < 100; i++) {
        slk_spot_publish(spot_a, "bench:ping", data.data(), data.size());
        char topic[64], buf[65536];
        size_t tlen, dlen;
        slk_spot_recv(spot_a, topic, sizeof(topic), &tlen,
                     buf, sizeof(buf), &dlen, 0);
    }

    // Actual benchmark
    for (int i = 0; i < message_count; i++) {
        stopwatch_t sw;
        sw.start();

        // Send ping
        rc = slk_spot_publish(spot_a, "bench:ping", data.data(), data.size());
        BENCH_ASSERT(rc == 0);

        // Wait for pong
        char topic[64], buf[65536];
        size_t tlen, dlen;
        rc = slk_spot_recv(spot_a, topic, sizeof(topic), &tlen,
                          buf, sizeof(buf), &dlen, 0);
        BENCH_ASSERT(rc == 0);
        BENCH_ASSERT(dlen == message_size);

        latencies.push_back(sw.elapsed_us());
    }

    print_latency_result("SPOT Remote (TCP)", {message_size, message_count, "remote"}, latencies);

    // Cleanup
    g_spot_running = false;
    echo_thread.join();

    slk_spot_destroy(&spot_a);
    slk_spot_destroy(&spot_b);
    slk_ctx_destroy(ctx);
}

int main() {
    printf("\n=== ServerLink SPOT Latency Benchmark (Round-Trip Time) ===\n\n");
    printf("%-20s | %14s | %12s | %14s | %14s | %14s\n",
           "Scenario", "Message Size", "Average", "p50", "p95", "p99");
    printf("------------------------------------------------------------------------------------");
    printf("----------\n");

    // Check for CI environment
    const char *ci_env = std::getenv("CI");
    const char *github_actions = std::getenv("GITHUB_ACTIONS");
    bool is_ci = (ci_env != nullptr) || (github_actions != nullptr);

    int iteration_count = is_ci ? 100 : 10000;

    if (is_ci) {
        printf("CI mode: using reduced iteration counts\n\n");
    }

    // Test with various message sizes
    size_t sizes[] = {64, 1024, 8192};

    for (size_t i = 0; i < 3; i++) {
        bench_spot_local_latency(sizes[i], iteration_count);
        bench_spot_remote_latency(sizes[i], iteration_count);
        printf("\n");
    }

    printf("Benchmark completed.\n\n");
    printf("Note: Latencies shown are round-trip times (RTT).\n");
    printf("      One-way latency is approximately RTT/2.\n\n");
    printf("Expected Performance:\n");
    printf("  Local (inproc):  <1 μs RTT\n");
    printf("  Remote (TCP):    ~50 μs RTT (localhost)\n\n");

    return 0;
}

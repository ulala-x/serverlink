/* SPDX-License-Identifier: MPL-2.0 */

#include "bench_common.hpp"
#include <thread>

// Benchmark parameters for SPOT
struct spot_bench_params_t {
    size_t message_size;
    int message_count;
    const char *scenario;  // "local" or "remote"
};

// Local throughput benchmark (single-process, inproc)
void bench_spot_local_throughput(const spot_bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    slk_spot_t *spot = slk_spot_new(ctx);
    BENCH_ASSERT(spot);

    // Create local topic
    int rc = slk_spot_topic_create(spot, "bench:throughput");
    BENCH_CHECK(rc, "slk_spot_topic_create");

    // Subscribe to the topic
    rc = slk_spot_subscribe(spot, "bench:throughput");
    BENCH_CHECK(rc, "slk_spot_subscribe");

    // Prepare test data
    std::vector<char> data(params.message_size, 'X');

    // Warmup phase
    for (int i = 0; i < 1000; i++) {
        rc = slk_spot_publish(spot, "bench:throughput", data.data(), data.size());
        BENCH_ASSERT(rc == 0);

        char topic[64], buf[65536];
        size_t tlen, dlen;
        rc = slk_spot_recv(spot, topic, sizeof(topic), &tlen,
                          buf, sizeof(buf), &dlen, 0);
        BENCH_ASSERT(rc == 0);
    }

    // Benchmark: send all messages first
    stopwatch_t sw;
    sw.start();

    for (int i = 0; i < params.message_count; i++) {
        rc = slk_spot_publish(spot, "bench:throughput", data.data(), data.size());
        BENCH_ASSERT(rc == 0);
    }

    // Then receive all
    for (int i = 0; i < params.message_count; i++) {
        char topic[64], buf[65536];
        size_t tlen, dlen;
        rc = slk_spot_recv(spot, topic, sizeof(topic), &tlen,
                          buf, sizeof(buf), &dlen, 0);
        BENCH_ASSERT(rc == 0);
        BENCH_ASSERT(dlen == params.message_size);
    }

    double elapsed_ms = sw.elapsed_ms();

    // Calculate and print results
    double msgs_per_sec = params.message_count / (elapsed_ms / 1000.0);
    double mb_per_sec = (params.message_count * params.message_size) /
                        (elapsed_ms / 1000.0) / (1024.0 * 1024.0);

    printf("%-20s | %8zu bytes | %8d msgs | %8.2f ms | %10.0f msg/s | %8.2f MB/s\n",
           "SPOT Local", params.message_size, params.message_count,
           elapsed_ms, msgs_per_sec, mb_per_sec);

    slk_spot_destroy(&spot);
    slk_ctx_destroy(ctx);
}

// Remote throughput benchmark (TCP)
void bench_spot_remote_throughput(const spot_bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    // Publisher and subscriber in same process but using TCP
    slk_spot_t *pub = slk_spot_new(ctx);
    slk_spot_t *sub = slk_spot_new(ctx);
    BENCH_ASSERT(pub && sub);

    // Publisher creates local topic and binds
    int rc = slk_spot_topic_create(pub, "bench:remote");
    BENCH_CHECK(rc, "slk_spot_topic_create");

    rc = slk_spot_bind(pub, "tcp://127.0.0.1:15600");
    BENCH_CHECK(rc, "slk_spot_bind");

    // Subscriber routes to remote topic
    rc = slk_spot_topic_route(sub, "bench:remote", "tcp://127.0.0.1:15600");
    BENCH_CHECK(rc, "slk_spot_topic_route");

    rc = slk_spot_subscribe(sub, "bench:remote");
    BENCH_CHECK(rc, "slk_spot_subscribe");

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Prepare test data
    std::vector<char> data(params.message_size, 'X');

    // Warmup phase
    for (int i = 0; i < 100; i++) {
        rc = slk_spot_publish(pub, "bench:remote", data.data(), data.size());
        BENCH_ASSERT(rc == 0);

        char topic[64], buf[65536];
        size_t tlen, dlen;
        rc = slk_spot_recv(sub, topic, sizeof(topic), &tlen,
                          buf, sizeof(buf), &dlen, 0);
        BENCH_ASSERT(rc == 0);
    }

    // Benchmark
    stopwatch_t sw;
    sw.start();

    // Send all messages
    for (int i = 0; i < params.message_count; i++) {
        rc = slk_spot_publish(pub, "bench:remote", data.data(), data.size());
        BENCH_ASSERT(rc == 0);
    }

    // Receive all
    for (int i = 0; i < params.message_count; i++) {
        char topic[64], buf[65536];
        size_t tlen, dlen;
        rc = slk_spot_recv(sub, topic, sizeof(topic), &tlen,
                          buf, sizeof(buf), &dlen, 0);
        BENCH_ASSERT(rc == 0);
        BENCH_ASSERT(dlen == params.message_size);
    }

    double elapsed_ms = sw.elapsed_ms();

    // Calculate and print results
    double msgs_per_sec = params.message_count / (elapsed_ms / 1000.0);
    double mb_per_sec = (params.message_count * params.message_size) /
                        (elapsed_ms / 1000.0) / (1024.0 * 1024.0);

    printf("%-20s | %8zu bytes | %8d msgs | %8.2f ms | %10.0f msg/s | %8.2f MB/s\n",
           "SPOT Remote (TCP)", params.message_size, params.message_count,
           elapsed_ms, msgs_per_sec, mb_per_sec);

    slk_spot_destroy(&pub);
    slk_spot_destroy(&sub);
    slk_ctx_destroy(ctx);
}

int main() {
    printf("\n=== ServerLink SPOT Throughput Benchmark ===\n\n");
    printf("%-20s | %14s | %13s | %11s | %14s | %12s\n",
           "Scenario", "Message Size", "Message Count", "Time", "Throughput", "Bandwidth");
    printf("------------------------------------------------------------------------------------");
    printf("----------\n");

    // Check for CI environment
    const char *ci_env = std::getenv("CI");
    const char *github_actions = std::getenv("GITHUB_ACTIONS");
    bool is_ci = (ci_env != nullptr) || (github_actions != nullptr);

    // Test with various message sizes
    size_t sizes[] = {64, 1024, 8192, 65536};
    int counts_full[] = {100000, 50000, 10000, 1000};
    int counts_ci[] = {1000, 500, 100, 50};

    int *counts = is_ci ? counts_ci : counts_full;

    if (is_ci) {
        printf("CI mode: using reduced iteration counts\n\n");
    }

    for (size_t i = 0; i < 4; i++) {
        spot_bench_params_t local_params = {sizes[i], counts[i], "local"};
        spot_bench_params_t remote_params = {sizes[i], counts[i], "remote"};

        bench_spot_local_throughput(local_params);
        bench_spot_remote_throughput(remote_params);
        printf("\n");
    }

    printf("Benchmark completed.\n\n");
    printf("Expected Performance:\n");
    printf("  Local (inproc):  ~18 GB/s (8KB messages)\n");
    printf("  Remote (TCP):    ~2 GB/s  (64KB messages)\n\n");

    return 0;
}

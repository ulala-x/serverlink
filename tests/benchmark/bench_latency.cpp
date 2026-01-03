/* SPDX-License-Identifier: MPL-2.0 */

#include "bench_common.hpp"
#include <thread>
#include <atomic>

std::atomic<bool> g_running(true);

// Echo server thread: receives messages and echoes them back
void echo_server(slk_socket_t *socket) {
    char buf[65536];
    char msg[65536];

    while (g_running) {
        // Poll with timeout to allow checking g_running
        slk_pollitem_t items[] = {{socket, 0, SLK_POLLIN, 0}};
        int rc = slk_poll(items, 1, 100);
        if (rc <= 0) continue;

        // Read identity frame
        int id_size = slk_recv(socket, buf, sizeof(buf), 0);
        if (id_size <= 0) continue;

        // Read message frame
        int msg_size = slk_recv(socket, msg, sizeof(msg), 0);
        if (msg_size <= 0) continue;

        // Echo back: [identity][message]
        slk_send(socket, buf, id_size, SLK_SNDMORE);
        slk_send(socket, msg, msg_size, 0);
    }
}

// TCP latency benchmark (RTT measurement)
void bench_latency_tcp(const bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    slk_socket_t *server = slk_socket(ctx, SLK_ROUTER);
    slk_socket_t *client = slk_socket(ctx, SLK_ROUTER);
    BENCH_ASSERT(server && client);

    // Set identities for ROUTER-ROUTER pattern
    const char *client_id = "client";
    const char *server_id = "server";
    int rc = slk_setsockopt(client, SLK_ROUTING_ID, client_id, strlen(client_id));
    BENCH_CHECK(rc, "slk_setsockopt(client SLK_ROUTING_ID)");
    rc = slk_setsockopt(server, SLK_ROUTING_ID, server_id, strlen(server_id));
    BENCH_CHECK(rc, "slk_setsockopt(server SLK_ROUTING_ID)");

    rc = slk_bind(server, "tcp://127.0.0.1:15556");
    BENCH_CHECK(rc, "slk_bind");

    rc = slk_connect(client, "tcp://127.0.0.1:15556");
    BENCH_CHECK(rc, "slk_connect");

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Start echo server thread
    g_running = true;
    std::thread server_thread(echo_server, server);

    // Measure latencies
    std::vector<char> data(params.message_size, 'B');
    std::vector<double> latencies;
    latencies.reserve(params.message_count);
    char buf[65536];
    size_t server_id_len = strlen(server_id);

    // Warmup phase (discard first few measurements)
    for (int i = 0; i < 100; i++) {
        slk_send(client, server_id, server_id_len, SLK_SNDMORE);
        slk_send(client, data.data(), data.size(), 0);
        slk_recv(client, buf, sizeof(buf), 0);  // identity
        slk_recv(client, buf, sizeof(buf), 0);  // echo
    }

    // Actual benchmark
    for (int i = 0; i < params.message_count; i++) {
        stopwatch_t sw;
        sw.start();

        // Send request: [server_id][message]
        slk_send(client, server_id, server_id_len, SLK_SNDMORE);
        int send_rc = slk_send(client, data.data(), data.size(), 0);
        BENCH_ASSERT(send_rc == static_cast<int>(data.size()));

        // Receive identity
        int id_rc = slk_recv(client, buf, sizeof(buf), 0);
        BENCH_ASSERT(id_rc > 0);

        // Receive echo
        int echo_rc = slk_recv(client, buf, sizeof(buf), 0);
        BENCH_ASSERT(echo_rc == static_cast<int>(params.message_size));

        latencies.push_back(sw.elapsed_us());
    }

    print_latency_result("TCP", params, latencies);

    g_running = false;
    server_thread.join();

    slk_close(client);
    slk_close(server);
    slk_ctx_destroy(ctx);
}

// inproc latency benchmark
void bench_latency_inproc(const bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    slk_socket_t *server = slk_socket(ctx, SLK_ROUTER);
    slk_socket_t *client = slk_socket(ctx, SLK_ROUTER);
    BENCH_ASSERT(server && client);

    // Set identities for ROUTER-ROUTER pattern
    const char *client_id = "client";
    const char *server_id = "server";
    int rc = slk_setsockopt(client, SLK_ROUTING_ID, client_id, strlen(client_id));
    BENCH_CHECK(rc, "slk_setsockopt(client SLK_ROUTING_ID)");
    rc = slk_setsockopt(server, SLK_ROUTING_ID, server_id, strlen(server_id));
    BENCH_CHECK(rc, "slk_setsockopt(server SLK_ROUTING_ID)");

    rc = slk_bind(server, "inproc://latency");
    BENCH_CHECK(rc, "slk_bind");

    rc = slk_connect(client, "inproc://latency");
    BENCH_CHECK(rc, "slk_connect");

    // Start echo server thread
    g_running = true;
    std::thread server_thread(echo_server, server);

    std::vector<char> data(params.message_size, 'B');
    std::vector<double> latencies;
    latencies.reserve(params.message_count);
    char buf[65536];
    size_t server_id_len = strlen(server_id);

    // Warmup phase
    for (int i = 0; i < 100; i++) {
        slk_send(client, server_id, server_id_len, SLK_SNDMORE);
        slk_send(client, data.data(), data.size(), 0);
        slk_recv(client, buf, sizeof(buf), 0);  // identity
        slk_recv(client, buf, sizeof(buf), 0);  // echo
    }

    // Actual benchmark
    for (int i = 0; i < params.message_count; i++) {
        stopwatch_t sw;
        sw.start();

        slk_send(client, server_id, server_id_len, SLK_SNDMORE);
        slk_send(client, data.data(), data.size(), 0);
        slk_recv(client, buf, sizeof(buf), 0);  // identity
        slk_recv(client, buf, sizeof(buf), 0);  // echo

        latencies.push_back(sw.elapsed_us());
    }

    print_latency_result("inproc", params, latencies);

    g_running = false;
    server_thread.join();

    slk_close(client);
    slk_close(server);
    slk_ctx_destroy(ctx);
}

#ifdef __linux__
// IPC latency benchmark
void bench_latency_ipc(const bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    slk_socket_t *server = slk_socket(ctx, SLK_ROUTER);
    slk_socket_t *client = slk_socket(ctx, SLK_ROUTER);
    BENCH_ASSERT(server && client);

    // Set identities for ROUTER-ROUTER pattern
    const char *client_id = "client";
    const char *server_id = "server";
    int rc = slk_setsockopt(client, SLK_ROUTING_ID, client_id, strlen(client_id));
    BENCH_CHECK(rc, "slk_setsockopt(client SLK_ROUTING_ID)");
    rc = slk_setsockopt(server, SLK_ROUTING_ID, server_id, strlen(server_id));
    BENCH_CHECK(rc, "slk_setsockopt(server SLK_ROUTING_ID)");

    rc = slk_bind(server, "ipc:///tmp/bench_latency.ipc");
    BENCH_CHECK(rc, "slk_bind");

    rc = slk_connect(client, "ipc:///tmp/bench_latency.ipc");
    BENCH_CHECK(rc, "slk_connect");

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Start echo server thread
    g_running = true;
    std::thread server_thread(echo_server, server);

    std::vector<char> data(params.message_size, 'B');
    std::vector<double> latencies;
    latencies.reserve(params.message_count);
    char buf[65536];
    size_t server_id_len = strlen(server_id);

    // Warmup phase
    for (int i = 0; i < 100; i++) {
        slk_send(client, server_id, server_id_len, SLK_SNDMORE);
        slk_send(client, data.data(), data.size(), 0);
        slk_recv(client, buf, sizeof(buf), 0);  // identity
        slk_recv(client, buf, sizeof(buf), 0);  // echo
    }

    // Actual benchmark
    for (int i = 0; i < params.message_count; i++) {
        stopwatch_t sw;
        sw.start();

        slk_send(client, server_id, server_id_len, SLK_SNDMORE);
        slk_send(client, data.data(), data.size(), 0);
        slk_recv(client, buf, sizeof(buf), 0);  // identity
        slk_recv(client, buf, sizeof(buf), 0);  // echo

        latencies.push_back(sw.elapsed_us());
    }

    print_latency_result("IPC", params, latencies);

    g_running = false;
    server_thread.join();

    slk_close(client);
    slk_close(server);
    slk_ctx_destroy(ctx);

    // Cleanup IPC file
    remove("/tmp/bench_latency.ipc");
}
#endif

int main() {
    printf("\n=== ServerLink Latency Benchmark (Round-Trip Time) ===\n\n");
    printf("%-20s | %14s | %12s | %14s | %14s | %14s\n",
           "Transport", "Message Size", "Average", "p50", "p95", "p99");
    printf("------------------------------------------------------------------------------------");
    printf("----------\n");

    // Check for CI environment - use reduced iterations
    const char *ci_env = std::getenv("CI");
    const char *github_actions = std::getenv("GITHUB_ACTIONS");
    bool is_ci = (ci_env != nullptr) || (github_actions != nullptr);

    // CI mode: fewer iterations, inproc only
    int iteration_count = is_ci ? 100 : 10000;

    if (is_ci) {
        printf("CI mode: using reduced iteration counts\n\n");
    }

    // Test with various message sizes
    size_t sizes[] = {64, 1024, 8192};

    for (size_t i = 0; i < 3; i++) {
        bench_params_t tcp_params = {sizes[i], iteration_count, "tcp"};
        bench_params_t inproc_params = {sizes[i], iteration_count, "inproc"};

        bench_latency_tcp(tcp_params);
        bench_latency_inproc(inproc_params);
#ifdef __linux__
        bench_params_t ipc_params = {sizes[i], iteration_count, "ipc"};
        bench_latency_ipc(ipc_params);
#endif
        printf("\n");
    }

    printf("Benchmark completed.\n\n");
    printf("Note: Latencies shown are round-trip times (RTT).\n");
    printf("      One-way latency is approximately RTT/2.\n\n");

    return 0;
}

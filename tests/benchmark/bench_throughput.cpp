/* SPDX-License-Identifier: MPL-2.0 */

#include "bench_common.hpp"
#include <thread>

// Sender thread: sends messages as fast as possible
// For ROUTER-ROUTER pattern, sender must include receiver's identity
void run_sender(slk_socket_t *socket, const char *receiver_id, const bench_params_t &params) {
    std::vector<char> data(params.message_size, 'A');
    size_t receiver_id_len = strlen(receiver_id);

    for (int i = 0; i < params.message_count; i++) {
        // Send receiver identity frame first (ROUTER requirement)
        int rc = slk_send(socket, receiver_id, receiver_id_len, SLK_SNDMORE);
        BENCH_ASSERT(rc == static_cast<int>(receiver_id_len));

        // Send message frame
        rc = slk_send(socket, data.data(), data.size(), 0);
        BENCH_ASSERT(rc == static_cast<int>(data.size()));
    }
}

// Receiver thread: receives messages and measures throughput
void run_receiver(slk_socket_t *socket, const bench_params_t &params, double *elapsed_ms) {
    std::vector<char> buf(params.message_size + 256);  // identity + message
    stopwatch_t sw;
    sw.start();

    for (int i = 0; i < params.message_count; i++) {
        // Receive identity frame (ROUTER socket receives identity first)
        int rc = slk_recv(socket, buf.data(), buf.size(), 0);
        BENCH_ASSERT(rc > 0);

        // Receive message frame
        rc = slk_recv(socket, buf.data(), buf.size(), 0);
        BENCH_ASSERT(rc == static_cast<int>(params.message_size));
    }

    *elapsed_ms = sw.elapsed_ms();
}

// TCP throughput benchmark
void bench_throughput_tcp(const bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    slk_socket_t *receiver = slk_socket(ctx, SLK_ROUTER);
    slk_socket_t *sender = slk_socket(ctx, SLK_ROUTER);
    BENCH_ASSERT(receiver && sender);

    // Set identities for both sockets (ROUTER-ROUTER pattern)
    const char *sender_id = "sender";
    const char *receiver_id = "receiver";
    int rc = slk_setsockopt(sender, SLK_ROUTING_ID, sender_id, strlen(sender_id));
    BENCH_CHECK(rc, "slk_setsockopt(sender SLK_ROUTING_ID)");
    rc = slk_setsockopt(receiver, SLK_ROUTING_ID, receiver_id, strlen(receiver_id));
    BENCH_CHECK(rc, "slk_setsockopt(receiver SLK_ROUTING_ID)");

    rc = slk_bind(receiver, "tcp://127.0.0.1:15555");
    BENCH_CHECK(rc, "slk_bind");

    rc = slk_connect(sender, "tcp://127.0.0.1:15555");
    BENCH_CHECK(rc, "slk_connect");

    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Run benchmark in separate threads
    double elapsed_ms = 0;
    std::thread recv_thread(run_receiver, receiver, std::cref(params), &elapsed_ms);
    std::thread send_thread(run_sender, sender, receiver_id, std::cref(params));

    send_thread.join();
    recv_thread.join();

    print_throughput_result("TCP", params, elapsed_ms);

    slk_close(sender);
    slk_close(receiver);
    slk_ctx_destroy(ctx);
}

// inproc throughput benchmark
void bench_throughput_inproc(const bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    slk_socket_t *receiver = slk_socket(ctx, SLK_ROUTER);
    slk_socket_t *sender = slk_socket(ctx, SLK_ROUTER);
    BENCH_ASSERT(receiver && sender);

    // Set identities for both sockets (ROUTER-ROUTER pattern)
    const char *sender_id = "sender";
    const char *receiver_id = "receiver";
    int rc = slk_setsockopt(sender, SLK_ROUTING_ID, sender_id, strlen(sender_id));
    BENCH_CHECK(rc, "slk_setsockopt(sender SLK_ROUTING_ID)");
    rc = slk_setsockopt(receiver, SLK_ROUTING_ID, receiver_id, strlen(receiver_id));
    BENCH_CHECK(rc, "slk_setsockopt(receiver SLK_ROUTING_ID)");

    rc = slk_bind(receiver, "inproc://bench");
    BENCH_CHECK(rc, "slk_bind");

    rc = slk_connect(sender, "inproc://bench");
    BENCH_CHECK(rc, "slk_connect");

    // inproc doesn't need connection delay
    double elapsed_ms = 0;
    std::thread recv_thread(run_receiver, receiver, std::cref(params), &elapsed_ms);
    std::thread send_thread(run_sender, sender, receiver_id, std::cref(params));

    send_thread.join();
    recv_thread.join();

    print_throughput_result("inproc", params, elapsed_ms);

    slk_close(sender);
    slk_close(receiver);
    slk_ctx_destroy(ctx);
}

#ifdef __linux__
// IPC throughput benchmark (Unix domain sockets)
void bench_throughput_ipc(const bench_params_t &params) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    slk_socket_t *receiver = slk_socket(ctx, SLK_ROUTER);
    slk_socket_t *sender = slk_socket(ctx, SLK_ROUTER);
    BENCH_ASSERT(receiver && sender);

    // Set identities for both sockets (ROUTER-ROUTER pattern)
    const char *sender_id = "sender";
    const char *receiver_id = "receiver";
    int rc = slk_setsockopt(sender, SLK_ROUTING_ID, sender_id, strlen(sender_id));
    BENCH_CHECK(rc, "slk_setsockopt(sender SLK_ROUTING_ID)");
    rc = slk_setsockopt(receiver, SLK_ROUTING_ID, receiver_id, strlen(receiver_id));
    BENCH_CHECK(rc, "slk_setsockopt(receiver SLK_ROUTING_ID)");

    rc = slk_bind(receiver, "ipc:///tmp/bench_throughput.ipc");
    BENCH_CHECK(rc, "slk_bind");

    rc = slk_connect(sender, "ipc:///tmp/bench_throughput.ipc");
    BENCH_CHECK(rc, "slk_connect");

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    double elapsed_ms = 0;
    std::thread recv_thread(run_receiver, receiver, std::cref(params), &elapsed_ms);
    std::thread send_thread(run_sender, sender, receiver_id, std::cref(params));

    send_thread.join();
    recv_thread.join();

    print_throughput_result("IPC", params, elapsed_ms);

    slk_close(sender);
    slk_close(receiver);
    slk_ctx_destroy(ctx);

    // Cleanup IPC file
    remove("/tmp/bench_throughput.ipc");
}
#endif

int main() {
    printf("\n=== ServerLink Throughput Benchmark ===\n\n");
    printf("%-20s | %14s | %13s | %11s | %14s | %12s\n",
           "Transport", "Message Size", "Message Count", "Time", "Throughput", "Bandwidth");
    printf("------------------------------------------------------------------------------------");
    printf("----------\n");

    // Test with various message sizes
    // Larger messages = fewer iterations (to keep test time reasonable)
    size_t sizes[] = {64, 1024, 8192, 65536};
    int counts[] = {100000, 50000, 10000, 1000};

    for (size_t i = 0; i < 4; i++) {
        bench_params_t params = {sizes[i], counts[i], "tcp"};

        bench_throughput_tcp(params);
        bench_throughput_inproc(params);
#ifdef __linux__
        bench_throughput_ipc(params);
#endif
        printf("\n");
    }

    printf("Benchmark completed.\n\n");

    return 0;
}

/* SPDX-License-Identifier: MPL-2.0 */
/* libzmq ROUTER-ROUTER throughput benchmark - for fair comparison with ServerLink */

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

// Sender thread: sends messages as fast as possible
// For ROUTER-ROUTER pattern, sender must include receiver's identity
void run_sender(void *socket, const char *receiver_id, const bench_params_t &params) {
    std::vector<char> data(params.message_size, 'A');
    std::vector<char> buf(256);
    size_t receiver_id_len = strlen(receiver_id);

    // Wait for READY signal from receiver to ensure handshake is complete
    int rc = zmq_recv(socket, buf.data(), buf.size(), 0);  // sender_id from receiver
    BENCH_ASSERT(rc > 0);
    int more = 0;
    size_t more_size = sizeof(more);
    zmq_getsockopt(socket, ZMQ_RCVMORE, &more, &more_size);
    BENCH_ASSERT(more == 1);

    rc = zmq_recv(socket, buf.data(), buf.size(), 0);  // "READY" message
    BENCH_ASSERT(rc > 0);

    for (int i = 0; i < params.message_count; i++) {
        // Send receiver identity frame first (ROUTER requirement)
        rc = zmq_send(socket, receiver_id, receiver_id_len, ZMQ_SNDMORE);
        BENCH_ASSERT(rc == static_cast<int>(receiver_id_len));

        // Send message frame
        rc = zmq_send(socket, data.data(), data.size(), 0);
        BENCH_ASSERT(rc == static_cast<int>(data.size()));
    }
}

// Receiver thread: receives messages and measures throughput
void run_receiver(void *socket, const char *sender_id, const bench_params_t &params, double *elapsed_ms) {
    std::vector<char> buf(params.message_size + 256);  // identity + message
    size_t sender_id_len = strlen(sender_id);

    // Send READY signal to sender to complete handshake
    const char *ready = "READY";
    int rc = zmq_send(socket, sender_id, sender_id_len, ZMQ_SNDMORE);
    BENCH_ASSERT(rc == static_cast<int>(sender_id_len));
    rc = zmq_send(socket, ready, strlen(ready), 0);
    BENCH_ASSERT(rc == static_cast<int>(strlen(ready)));

    stopwatch_t sw;
    sw.start();

    for (int i = 0; i < params.message_count; i++) {
        // Receive identity frame (ROUTER socket receives identity first)
        rc = zmq_recv(socket, buf.data(), buf.size(), 0);
        BENCH_ASSERT(rc > 0);

        int more = 0;
        size_t more_size = sizeof(more);
        zmq_getsockopt(socket, ZMQ_RCVMORE, &more, &more_size);
        BENCH_ASSERT(more == 1);

        // Receive message frame
        rc = zmq_recv(socket, buf.data(), buf.size(), 0);
        BENCH_ASSERT(rc == static_cast<int>(params.message_size));
    }

    *elapsed_ms = sw.elapsed_ms();
}

// TCP throughput benchmark
void bench_throughput_tcp(const bench_params_t &params) {
    void *ctx = zmq_ctx_new();
    BENCH_ASSERT(ctx);

    void *receiver = zmq_socket(ctx, ZMQ_ROUTER);
    void *sender = zmq_socket(ctx, ZMQ_ROUTER);
    BENCH_ASSERT(receiver && sender);

    // Set identities for both sockets (ROUTER-ROUTER pattern)
    const char *sender_id = "sender";
    const char *receiver_id = "receiver";
    int rc = zmq_setsockopt(sender, ZMQ_ROUTING_ID, sender_id, strlen(sender_id));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(receiver, ZMQ_ROUTING_ID, receiver_id, strlen(receiver_id));
    BENCH_ASSERT(rc == 0);

    // Set high water mark to allow buffering many messages
    int hwm = 0;  // unlimited
    rc = zmq_setsockopt(sender, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(sender, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(receiver, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(receiver, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);

    rc = zmq_bind(receiver, "tcp://127.0.0.1:15556");
    BENCH_ASSERT(rc == 0);

    rc = zmq_connect(sender, "tcp://127.0.0.1:15556");
    BENCH_ASSERT(rc == 0);

    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Run benchmark in separate threads
    double elapsed_ms = 0;
    std::thread recv_thread(run_receiver, receiver, sender_id, std::cref(params), &elapsed_ms);
    std::thread send_thread(run_sender, sender, receiver_id, std::cref(params));

    send_thread.join();
    recv_thread.join();

    print_throughput_result("TCP", params, elapsed_ms);

    zmq_close(sender);
    zmq_close(receiver);
    zmq_ctx_destroy(ctx);
}

// inproc throughput benchmark
void bench_throughput_inproc(const bench_params_t &params) {
    void *ctx = zmq_ctx_new();
    BENCH_ASSERT(ctx);

    void *receiver = zmq_socket(ctx, ZMQ_ROUTER);
    void *sender = zmq_socket(ctx, ZMQ_ROUTER);
    BENCH_ASSERT(receiver && sender);

    // Set identities for both sockets (ROUTER-ROUTER pattern)
    const char *sender_id = "sender";
    const char *receiver_id = "receiver";
    int rc = zmq_setsockopt(sender, ZMQ_ROUTING_ID, sender_id, strlen(sender_id));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(receiver, ZMQ_ROUTING_ID, receiver_id, strlen(receiver_id));
    BENCH_ASSERT(rc == 0);

    // Set high water mark to allow buffering many messages
    int hwm = 0;  // unlimited
    rc = zmq_setsockopt(sender, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(sender, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(receiver, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(receiver, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);

    rc = zmq_bind(receiver, "inproc://bench");
    BENCH_ASSERT(rc == 0);

    rc = zmq_connect(sender, "inproc://bench");
    BENCH_ASSERT(rc == 0);

    // inproc doesn't need connection delay
    double elapsed_ms = 0;
    std::thread recv_thread(run_receiver, receiver, sender_id, std::cref(params), &elapsed_ms);
    std::thread send_thread(run_sender, sender, receiver_id, std::cref(params));

    send_thread.join();
    recv_thread.join();

    print_throughput_result("inproc", params, elapsed_ms);

    zmq_close(sender);
    zmq_close(receiver);
    zmq_ctx_destroy(ctx);
}

#ifdef __linux__
// IPC throughput benchmark (Unix domain sockets)
void bench_throughput_ipc(const bench_params_t &params) {
    void *ctx = zmq_ctx_new();
    BENCH_ASSERT(ctx);

    void *receiver = zmq_socket(ctx, ZMQ_ROUTER);
    void *sender = zmq_socket(ctx, ZMQ_ROUTER);
    BENCH_ASSERT(receiver && sender);

    // Set identities for both sockets (ROUTER-ROUTER pattern)
    const char *sender_id = "sender";
    const char *receiver_id = "receiver";
    int rc = zmq_setsockopt(sender, ZMQ_ROUTING_ID, sender_id, strlen(sender_id));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(receiver, ZMQ_ROUTING_ID, receiver_id, strlen(receiver_id));
    BENCH_ASSERT(rc == 0);

    // Set high water mark to allow buffering many messages
    int hwm = 0;  // unlimited
    rc = zmq_setsockopt(sender, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(sender, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(receiver, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);
    rc = zmq_setsockopt(receiver, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    BENCH_ASSERT(rc == 0);

    rc = zmq_bind(receiver, "ipc:///tmp/bench_zmq.ipc");
    BENCH_ASSERT(rc == 0);

    rc = zmq_connect(sender, "ipc:///tmp/bench_zmq.ipc");
    BENCH_ASSERT(rc == 0);

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    double elapsed_ms = 0;
    std::thread recv_thread(run_receiver, receiver, sender_id, std::cref(params), &elapsed_ms);
    std::thread send_thread(run_sender, sender, receiver_id, std::cref(params));

    send_thread.join();
    recv_thread.join();

    print_throughput_result("IPC", params, elapsed_ms);

    zmq_close(sender);
    zmq_close(receiver);
    zmq_ctx_destroy(ctx);

    // Cleanup IPC file
    remove("/tmp/bench_zmq.ipc");
}
#endif

int main() {
    printf("\n=== libzmq ROUTER-ROUTER Throughput Benchmark ===\n\n");
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
    int counts_ci[] = {1000, 500, 100, 50};  // ~100x faster for CI

    int *counts = is_ci ? counts_ci : counts_full;

    if (is_ci) {
        printf("CI mode: using reduced iteration counts\n\n");
    }

    for (size_t i = 0; i < 4; i++) {
        bench_params_t params = {sizes[i], counts[i], "inproc"};

        // In CI, only run inproc (faster, no port conflicts)
        // In full mode, run all transports
        if (!is_ci) {
            bench_params_t tcp_params = {sizes[i], counts[i], "tcp"};
            bench_throughput_tcp(tcp_params);
        }
        bench_throughput_inproc(params);
#ifdef __linux__
        if (!is_ci) {
            bench_params_t ipc_params = {sizes[i], counts[i], "ipc"};
            bench_throughput_ipc(ipc_params);
        }
#endif
        printf("\n");
    }

    printf("Benchmark completed.\n\n");

    return 0;
}

/* SPDX-License-Identifier: MPL-2.0 */

// Detailed profiling benchmark to identify performance bottlenecks
// Measures timing breakdowns for each operation in the hot path

#include <serverlink/config.h>
#include "bench_common.hpp"
#include <thread>
#include <chrono>
#include <atomic>

// High-resolution timing helper
class ProfileTimer {
public:
    using clock = std::chrono::high_resolution_clock;
    using time_point = clock::time_point;
    using duration = std::chrono::nanoseconds;

    ProfileTimer() : total_(0), count_(0) {}

    void start() { start_ = clock::now(); }

    void stop() {
        auto elapsed = std::chrono::duration_cast<duration>(clock::now() - start_);
        total_ += elapsed.count();
        count_++;
    }

    double avg_ns() const { return count_ > 0 ? static_cast<double>(total_) / count_ : 0.0; }
    double avg_us() const { return avg_ns() / 1000.0; }
    uint64_t total_ns() const { return total_; }
    uint64_t count() const { return count_; }

private:
    time_point start_;
    std::atomic<uint64_t> total_;
    std::atomic<uint64_t> count_;
};

// Profiling data structure
struct ProfileData {
    ProfileTimer send_routing_id;
    ProfileTimer send_message;
    ProfileTimer recv_routing_id;
    ProfileTimer recv_message;
    ProfileTimer total_iteration;
};

// Profiled sender
void profile_sender(slk_socket_t *socket, const char *receiver_id, int message_count, int message_size, ProfileData *prof) {
    std::vector<char> data(message_size, 'A');
    std::vector<char> buf(256);
    size_t receiver_id_len = strlen(receiver_id);

    // Wait for READY signal
    int rc = slk_recv(socket, buf.data(), buf.size(), 0);
    BENCH_ASSERT(rc > 0);
    rc = slk_recv(socket, buf.data(), buf.size(), 0);
    BENCH_ASSERT(rc > 0);

    for (int i = 0; i < message_count; i++) {
        prof->total_iteration.start();

        // Send routing ID
        prof->send_routing_id.start();
        rc = slk_send(socket, receiver_id, receiver_id_len, SLK_SNDMORE);
        prof->send_routing_id.stop();
        BENCH_ASSERT(rc == static_cast<int>(receiver_id_len));

        // Send message
        prof->send_message.start();
        rc = slk_send(socket, data.data(), data.size(), 0);
        prof->send_message.stop();
        BENCH_ASSERT(rc == static_cast<int>(data.size()));

        prof->total_iteration.stop();
    }
}

// Profiled receiver
void profile_receiver(slk_socket_t *socket, const char *sender_id, int message_count, int message_size, ProfileData *prof) {
    std::vector<char> buf(message_size + 256);
    size_t sender_id_len = strlen(sender_id);

    // Send READY signal
    const char *ready = "READY";
    int rc = slk_send(socket, sender_id, sender_id_len, SLK_SNDMORE);
    BENCH_ASSERT(rc == static_cast<int>(sender_id_len));
    rc = slk_send(socket, ready, strlen(ready), 0);
    BENCH_ASSERT(rc == static_cast<int>(strlen(ready)));

    for (int i = 0; i < message_count; i++) {
        prof->total_iteration.start();

        // Receive routing ID
        prof->recv_routing_id.start();
        rc = slk_recv(socket, buf.data(), buf.size(), 0);
        prof->recv_routing_id.stop();
        BENCH_ASSERT(rc > 0);

        // Receive message
        prof->recv_message.start();
        rc = slk_recv(socket, buf.data(), buf.size(), 0);
        prof->recv_message.stop();
        BENCH_ASSERT(rc == static_cast<int>(message_size));

        prof->total_iteration.stop();
    }
}

void print_profile_results(const char *label, const ProfileData &send_prof, const ProfileData &recv_prof, int message_count) {
    printf("\n=== %s Profiling Results ===\n", label);
    printf("Messages: %d\n\n", message_count);

    printf("Sender breakdown (per message):\n");
    printf("  Send routing ID:  %8.2f us\n", send_prof.send_routing_id.avg_us());
    printf("  Send message:     %8.2f us\n", send_prof.send_message.avg_us());
    printf("  Total iteration:  %8.2f us\n", send_prof.total_iteration.avg_us());
    printf("\n");

    printf("Receiver breakdown (per message):\n");
    printf("  Recv routing ID:  %8.2f us\n", recv_prof.recv_routing_id.avg_us());
    printf("  Recv message:     %8.2f us\n", recv_prof.recv_message.avg_us());
    printf("  Total iteration:  %8.2f us\n", recv_prof.total_iteration.avg_us());
    printf("\n");

    // Calculate overall throughput
    double total_time_us = std::max(send_prof.total_iteration.total_ns(), recv_prof.total_iteration.total_ns()) / 1000.0;
    double throughput = (message_count * 1000000.0) / total_time_us;
    printf("Overall throughput: %.2f msg/s (%.2f ms total)\n", throughput, total_time_us / 1000.0);
    printf("\n");
}

// Profile inproc throughput with detailed breakdowns
void profile_inproc(int message_count, int message_size) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    slk_socket_t *receiver = slk_socket(ctx, SLK_ROUTER);
    slk_socket_t *sender = slk_socket(ctx, SLK_ROUTER);
    BENCH_ASSERT(receiver && sender);

    // Set identities
    const char *sender_id = "sender";
    const char *receiver_id = "receiver";
    int rc = slk_setsockopt(sender, SLK_ROUTING_ID, sender_id, strlen(sender_id));
    BENCH_CHECK(rc, "slk_setsockopt(sender SLK_ROUTING_ID)");
    rc = slk_setsockopt(receiver, SLK_ROUTING_ID, receiver_id, strlen(receiver_id));
    BENCH_CHECK(rc, "slk_setsockopt(receiver SLK_ROUTING_ID)");

    // Set unlimited HWM
    int hwm = 0;
    slk_setsockopt(sender, SLK_SNDHWM, &hwm, sizeof(hwm));
    slk_setsockopt(sender, SLK_RCVHWM, &hwm, sizeof(hwm));
    slk_setsockopt(receiver, SLK_SNDHWM, &hwm, sizeof(hwm));
    slk_setsockopt(receiver, SLK_RCVHWM, &hwm, sizeof(hwm));

    rc = slk_bind(receiver, "inproc://profile");
    BENCH_CHECK(rc, "slk_bind");
    rc = slk_connect(sender, "inproc://profile");
    BENCH_CHECK(rc, "slk_connect");

    ProfileData send_prof, recv_prof;
    std::thread recv_thread(profile_receiver, receiver, sender_id, message_count, message_size, &recv_prof);
    std::thread send_thread(profile_sender, sender, receiver_id, message_count, message_size, &send_prof);

    send_thread.join();
    recv_thread.join();

    print_profile_results("inproc", send_prof, recv_prof, message_count);

    slk_close(sender);
    slk_close(receiver);
    slk_ctx_destroy(ctx);
}

// Profile TCP throughput with detailed breakdowns
void profile_tcp(int message_count, int message_size) {
    slk_ctx_t *ctx = slk_ctx_new();
    BENCH_ASSERT(ctx);

    slk_socket_t *receiver = slk_socket(ctx, SLK_ROUTER);
    slk_socket_t *sender = slk_socket(ctx, SLK_ROUTER);
    BENCH_ASSERT(receiver && sender);

    // Set identities
    const char *sender_id = "sender";
    const char *receiver_id = "receiver";
    int rc = slk_setsockopt(sender, SLK_ROUTING_ID, sender_id, strlen(sender_id));
    BENCH_CHECK(rc, "slk_setsockopt(sender SLK_ROUTING_ID)");
    rc = slk_setsockopt(receiver, SLK_ROUTING_ID, receiver_id, strlen(receiver_id));
    BENCH_CHECK(rc, "slk_setsockopt(receiver SLK_ROUTING_ID)");

    // Set unlimited HWM
    int hwm = 0;
    slk_setsockopt(sender, SLK_SNDHWM, &hwm, sizeof(hwm));
    slk_setsockopt(sender, SLK_RCVHWM, &hwm, sizeof(hwm));
    slk_setsockopt(receiver, SLK_SNDHWM, &hwm, sizeof(hwm));
    slk_setsockopt(receiver, SLK_RCVHWM, &hwm, sizeof(hwm));

    rc = slk_bind(receiver, "tcp://127.0.0.1:15556");
    BENCH_CHECK(rc, "slk_bind");
    rc = slk_connect(sender, "tcp://127.0.0.1:15556");
    BENCH_CHECK(rc, "slk_connect");

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ProfileData send_prof, recv_prof;
    std::thread recv_thread(profile_receiver, receiver, sender_id, message_count, message_size, &recv_prof);
    std::thread send_thread(profile_sender, sender, receiver_id, message_count, message_size, &send_prof);

    send_thread.join();
    recv_thread.join();

    print_profile_results("TCP", send_prof, recv_prof, message_count);

    slk_close(sender);
    slk_close(receiver);
    slk_ctx_destroy(ctx);
}

int main() {
    printf("\n=== ServerLink Detailed Profiling ===\n");

    // Test configurations matching the performance gap scenarios
    struct TestConfig {
        const char *name;
        int message_size;
        int message_count;
    };

    TestConfig configs[] = {
        {"64B inproc", 64, 10000},
        {"1KB TCP", 1024, 5000},
        {"64B TCP", 64, 10000}
    };

    for (const auto &config : configs) {
        printf("\n\n--- Testing: %s ---\n", config.name);

        if (strstr(config.name, "inproc")) {
            profile_inproc(config.message_count, config.message_size);
        } else {
            profile_tcp(config.message_count, config.message_size);
        }
    }

    printf("\n=== Profiling Complete ===\n\n");
    return 0;
}

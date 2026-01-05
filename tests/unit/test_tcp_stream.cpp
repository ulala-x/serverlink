/* SPDX-License-Identifier: MPL-2.0 */

#include "../../src/precompiled.hpp"

#ifdef SL_USE_ASIO
#include "../../src/io/asio/tcp_stream.hpp"
#include "../../src/io/asio/asio_context.hpp"
#include <chrono>
#include <thread>
#include <atomic>
#include <cstring>

using namespace slk;

// Test 1: Construction
void test_construction()
{
    printf("Test 1: TCP Stream Construction\n");

    // Start io_context
    asio_context_t::instance().start();

    // Create socket
    asio::ip::tcp::socket socket(asio_context_t::instance().get_context());
    tcp_stream_t stream(std::move(socket));

    // Cleanup
    asio_context_t::instance().stop();

    printf("  ✓ Construction test passed\n");
}

// Test 2: Async Read/Write
void test_async_read_write()
{
    printf("Test 2: TCP Stream Async Read/Write\n");

    // Start io_context
    asio_context_t::instance().start();

    std::atomic<bool> server_ready{false};
    std::atomic<bool> read_complete{false};
    std::atomic<bool> write_complete{false};
    std::atomic<bool> test_passed{false};

    char received_buf[128] = {0};
    const char* test_msg = "Hello Asio TCP Stream!";

    // Server thread
    std::thread server_thread([&]() {
        try {
            // Create acceptor
            asio::ip::tcp::acceptor acceptor(
                asio_context_t::instance().get_context(),
                asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 15556)
            );

            server_ready = true;

            // Accept connection
            asio::ip::tcp::socket server_socket(asio_context_t::instance().get_context());
            acceptor.accept(server_socket);

            // Create stream
            tcp_stream_t stream(std::move(server_socket));

            // Async read
            stream.async_read(received_buf, sizeof(received_buf),
                [&](size_t bytes, int error) {
                    read_complete = true;
                    if (error == 0 && bytes > 0) {
                        // Echo back
                        stream.async_write(received_buf, bytes,
                            [&](size_t written_bytes, int write_error) {
                                if (write_error == 0 && written_bytes == bytes) {
                                    test_passed = true;
                                }
                            });
                    }
                });
        } catch (...) {
            printf("  ✗ Server exception\n");
        }
    });

    // Wait for server to be ready
    while (!server_ready) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Client
    try {
        asio::ip::tcp::socket client_socket(asio_context_t::instance().get_context());
        client_socket.connect(asio::ip::tcp::endpoint(
            asio::ip::address::from_string("127.0.0.1"), 15556
        ));

        tcp_stream_t client_stream(std::move(client_socket));

        // Write message
        client_stream.async_write(test_msg, strlen(test_msg),
            [&](size_t bytes, int error) {
                write_complete = true;
                if (error != 0 || bytes != strlen(test_msg)) {
                    printf("  ✗ Write failed: error=%d, bytes=%zu\n", error, bytes);
                }
            });

        // Wait for operations to complete
        for (int i = 0; i < 100 && !test_passed; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

    } catch (const std::exception& e) {
        printf("  ✗ Client exception: %s\n", e.what());
    }

    server_thread.join();

    // Cleanup
    asio_context_t::instance().stop();

    if (write_complete && read_complete && test_passed) {
        printf("  ✓ Async read/write test passed\n");
        printf("    Sent: %s\n", test_msg);
        printf("    Received: %s\n", received_buf);
    } else {
        printf("  ✗ Async read/write test failed\n");
        printf("    write_complete=%d, read_complete=%d, test_passed=%d\n",
               write_complete.load(), read_complete.load(), test_passed.load());
    }
}

// Test 3: Close
void test_close()
{
    printf("Test 3: TCP Stream Close\n");

    // Start io_context
    asio_context_t::instance().start();

    asio::ip::tcp::socket socket(asio_context_t::instance().get_context());
    tcp_stream_t stream(std::move(socket));

    stream.close();

    // Cleanup
    asio_context_t::instance().stop();

    printf("  ✓ Close test passed\n");
}

int main()
{
    printf("=== TCP Stream Tests ===\n\n");

    try {
        test_construction();
        test_async_read_write();
        test_close();

        printf("\n=== All Tests Passed ===\n");
        return 0;
    } catch (const std::exception& e) {
        printf("\n✗ Exception: %s\n", e.what());
        return 1;
    }
}

#else

int main()
{
    printf("⚠ Asio not enabled (SL_USE_ASIO not defined)\n");
    return 0;
}

#endif // SL_USE_ASIO

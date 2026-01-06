/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Modern C++20 std::span API Example */
/* This example demonstrates the internal C++ API with std::span support */

#include "../src/msg/blob.hpp"
#include "../src/msg/msg.hpp"
#include "../src/util/config.hpp"
#include <iostream>
#include <cstring>

#if SL_HAVE_SPAN
#include <span>
#include <algorithm>
#include <numeric>
#endif

int main ()
{
#if SL_HAVE_SPAN
    std::cout << "=== ServerLink C++20 std::span API Example ===" << std::endl;

    std::cout << "\n1. blob_t with std::span:" << std::endl;
    {
        const char *text = "Hello, std::span!";
        slk::blob_t blob (reinterpret_cast<const unsigned char *> (text),
                          strlen (text));

        // Get span view
        std::span<unsigned char> blob_span = blob.span ();
        std::cout << "   Blob size: " << blob_span.size () << " bytes" << std::endl;
        std::cout << "   Content: "
                  << std::string (reinterpret_cast<char *> (blob_span.data ()),
                                  blob_span.size ())
                  << std::endl;

        // Extract subranges
        auto first_5 = blob_span.first (5);
        std::cout << "   First 5 bytes: "
                  << std::string (reinterpret_cast<char *> (first_5.data ()),
                                  first_5.size ())
                  << std::endl;

        auto last_5 = blob_span.last (5);
        std::cout << "   Last 5 bytes: "
                  << std::string (reinterpret_cast<char *> (last_5.data ()),
                                  last_5.size ())
                  << std::endl;
    }

    std::cout << "\n2. msg_t with data_span():" << std::endl;
    {
        slk::msg_t msg;
        int rc = msg.init_size (100);
        if (rc != 0) {
            std::cerr << "Failed to initialize message" << std::endl;
            return 1;
        }

        // Get span view as std::byte
        std::span<std::byte> msg_span = msg.data_span ();
        std::cout << "   Message size: " << msg_span.size () << " bytes"
                  << std::endl;

        // Fill with sequence using modern C++ algorithms
        std::byte val = std::byte{0};
        for (auto &b : msg_span) {
            b = val;
            val = static_cast<std::byte> (static_cast<unsigned char> (val) + 1);
        }

        std::cout << "   First 10 bytes: ";
        for (size_t i = 0; i < 10; ++i) {
            std::cout << static_cast<unsigned> (msg_span[i]) << " ";
        }
        std::cout << std::endl;

        msg.close ();
    }

    std::cout << "\n3. Using span for safe subrange access:" << std::endl;
    {
        slk::msg_t msg;
        const char *text = "ServerLink rocks!";
        int rc = msg.init_buffer (text, strlen (text));
        if (rc != 0) {
            std::cerr << "Failed to initialize message" << std::endl;
            return 1;
        }

        // Get const span view
        const slk::msg_t &const_msg = msg;
        std::span<const std::byte> full_span = const_msg.data_span ();

        std::cout << "   Full message: " << text << std::endl;

        // Extract subranges safely
        auto first_10 = full_span.first (10);
        std::cout << "   First 10 bytes: "
                  << std::string (reinterpret_cast<const char *> (first_10.data ()),
                                  first_10.size ())
                  << std::endl;

        auto last_6 = full_span.last (6);
        std::cout << "   Last 6 bytes: "
                  << std::string (reinterpret_cast<const char *> (last_6.data ()),
                                  last_6.size ())
                  << std::endl;

        msg.close ();
    }

    std::cout << "\n4. Using std::span with algorithms:" << std::endl;
    {
        slk::msg_t msg;
        int rc = msg.init_size (50);
        if (rc != 0) {
            std::cerr << "Failed to initialize message" << std::endl;
            return 1;
        }

        std::span<std::byte> data_span = msg.data_span ();

        // Fill first half with 0x11, second half with 0x22
        std::fill (data_span.first (25).begin (), data_span.first (25).end (),
                   std::byte{0x11});
        std::fill (data_span.last (25).begin (), data_span.last (25).end (),
                   std::byte{0x22});

        // Count occurrences
        auto count_11 = std::count (data_span.begin (), data_span.end (),
                                    std::byte{0x11});
        auto count_22 = std::count (data_span.begin (), data_span.end (),
                                    std::byte{0x22});

        std::cout << "   Count of 0x11: " << count_11 << std::endl;
        std::cout << "   Count of 0x22: " << count_22 << std::endl;

        // Find first occurrence of 0x22
        auto it = std::find (data_span.begin (), data_span.end (),
                             std::byte{0x22});
        if (it != data_span.end ()) {
            std::cout << "   First 0x22 found at index: "
                      << std::distance (data_span.begin (), it) << std::endl;
        }

        msg.close ();
    }

    std::cout << "\n5. Zero-copy message with span:" << std::endl;
    {
        unsigned char buffer[64];
        for (size_t i = 0; i < sizeof (buffer); ++i) {
            buffer[i] = static_cast<unsigned char> (i * 2);
        }

        slk::msg_t msg;
        int rc = msg.init_data (buffer, sizeof (buffer), nullptr, nullptr);
        if (rc != 0) {
            std::cerr << "Failed to initialize message" << std::endl;
            return 1;
        }

        std::span<std::byte> span = msg.data_span ();
        std::cout << "   Zero-copy message size: " << span.size () << " bytes"
                  << std::endl;

        // Verify every 10th value
        std::cout << "   Values at 0, 10, 20, 30: ";
        for (size_t i = 0; i < 40; i += 10) {
            std::cout << static_cast<unsigned> (span[i]) << " ";
        }
        std::cout << std::endl;

        msg.close ();
    }

    std::cout << "\n=== Example completed successfully ===" << std::endl;
    return 0;
#else
    std::cout << "This example requires C++20 with std::span support" << std::endl;
    return 0;
#endif
}

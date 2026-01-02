/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - C++20 std::span API test */

#include <serverlink/serverlink.h>
#include <serverlink/config.h>

// This test uses internal APIs (msg_t, blob_t) that are not exported on Windows DLL builds
// On Windows, we skip this test since internal implementation details aren't available
#if defined(_WIN32) && defined(SL_USING_DLL)
#include <cstdio>
int main()
{
    printf("test_span_api: Skipping on Windows DLL (internal API test)\n");
    return 0;
}
#elif !SL_HAVE_SPAN
// std::span not available, provide minimal test
int main()
{
    return 0;
}
#else

#include "../../src/msg/blob.hpp"
#include "../../src/msg/msg.hpp"
#include "../../src/util/config.hpp"

#include <cassert>
#include <cstring>
#include <algorithm>
#include <span>
#include <numeric>

static void test_blob_span ()
{
    // Test blob_t span() methods
    const char *data = "Hello, std::span!";
    const size_t len = strlen (data);

    slk::blob_t blob (reinterpret_cast<const unsigned char *> (data), len);

    // Test mutable span
    std::span<unsigned char> blob_span = blob.span ();
    assert (blob_span.size () == len);
    assert (blob_span.data () == blob.data ());

    // Test const span
    const slk::blob_t &const_blob = blob;
    std::span<const unsigned char> const_span = const_blob.span ();
    assert (const_span.size () == len);
    assert (const_span.data () == const_blob.data ());

    // Verify data integrity
    assert (std::equal (blob_span.begin (), blob_span.end (),
                        reinterpret_cast<const unsigned char *> (data)));

    // Test span operations
    auto first_5 = blob_span.first (5);
    assert (first_5.size () == 5);
    assert (memcmp (first_5.data (), "Hello", 5) == 0);

    auto last_5 = blob_span.last (5);
    assert (last_5.size () == 5);
    assert (memcmp (last_5.data (), "span!", 5) == 0);

    // Test subspan
    auto middle = blob_span.subspan (7, 3);
    assert (middle.size () == 3);
    assert (memcmp (middle.data (), "std", 3) == 0);
}

static void test_msg_span ()
{
    // Test msg_t data_span() methods
    const char *payload = "Message payload";
    const size_t len = strlen (payload);

    slk::msg_t msg;
    int rc = msg.init_buffer (payload, len);
    assert (rc == 0);

    // Test mutable span
    std::span<std::byte> msg_span = msg.data_span ();
    assert (msg_span.size () == len);
    assert (msg_span.size_bytes () == len);

    // Test const span
    const slk::msg_t &const_msg = msg;
    std::span<const std::byte> const_span = const_msg.data_span ();
    assert (const_span.size () == len);

    // Verify data integrity through span
    for (size_t i = 0; i < len; ++i) {
        assert (static_cast<unsigned char> (msg_span[i]) == payload[i]);
    }

    // Test span-based iteration
    size_t idx = 0;
    for (std::byte b : const_span) {
        assert (static_cast<unsigned char> (b) == payload[idx++]);
    }

    // Test span operations with std::byte
    auto first_7 = msg_span.first (7);
    assert (first_7.size () == 7);
    assert (memcmp (first_7.data (), "Message", 7) == 0);

    // Test modification through span
    slk::msg_t msg2;
    rc = msg2.init_size (8);
    assert (rc == 0);

    std::span<std::byte> mut_span = msg2.data_span ();
    assert (mut_span.size () == 8);

    // Fill with pattern using span
    std::byte pattern = static_cast<std::byte> (0xAB);
    std::fill (mut_span.begin (), mut_span.end (), pattern);

    // Verify pattern
    for (std::byte b : mut_span) {
        assert (b == pattern);
    }

    msg.close ();
    msg2.close ();
}

static void test_span_zero_copy ()
{
    // Test span with zero-copy message (type_cmsg)
    unsigned char buffer[256];
    for (size_t i = 0; i < sizeof (buffer); ++i) {
        buffer[i] = static_cast<unsigned char> (i);
    }

    slk::msg_t msg;
    int rc = msg.init_data (buffer, sizeof (buffer), nullptr, nullptr);
    assert (rc == 0);

    std::span<std::byte> span = msg.data_span ();
    assert (span.size () == sizeof (buffer));

    // Verify incrementing sequence
    for (size_t i = 0; i < span.size (); ++i) {
        assert (static_cast<unsigned char> (span[i]) == i);
    }

    msg.close ();
}

static void test_span_vsm_and_lmsg ()
{
    // Test VSM (very small message)
    {
        slk::msg_t vsm;
        int rc = vsm.init_size (16); // Small enough for VSM
        assert (rc == 0);

        std::span<std::byte> vsm_span = vsm.data_span ();
        assert (vsm_span.size () == 16);

        // Fill with index pattern
        for (size_t i = 0; i < vsm_span.size (); ++i) {
            vsm_span[i] = static_cast<std::byte> (i);
        }

        // Verify
        for (size_t i = 0; i < vsm_span.size (); ++i) {
            assert (static_cast<unsigned char> (vsm_span[i]) == i);
        }

        vsm.close ();
    }

    // Test LMSG (large message)
    {
        slk::msg_t lmsg;
        int rc = lmsg.init_size (4096); // Large enough for LMSG
        assert (rc == 0);

        std::span<std::byte> lmsg_span = lmsg.data_span ();
        assert (lmsg_span.size () == 4096);

        // Fill first and last bytes
        lmsg_span.front () = static_cast<std::byte> (0xFF);
        lmsg_span.back () = static_cast<std::byte> (0xEE);

        assert (static_cast<unsigned char> (lmsg_span.front ()) == 0xFF);
        assert (static_cast<unsigned char> (lmsg_span.back ()) == 0xEE);

        lmsg.close ();
    }
}

static void test_span_algorithms ()
{
    // Demonstrate modern C++ algorithm usage with span
    slk::msg_t msg;
    int rc = msg.init_size (100);
    assert (rc == 0);

    std::span<std::byte> span = msg.data_span ();

    // Use std::iota to fill with sequence
    std::byte val = std::byte{0};
    for (auto &b : span) {
        b = val;
        val = static_cast<std::byte> (static_cast<unsigned char> (val) + 1);
    }

    // Verify with std::all_of
    bool is_sorted = true;
    for (size_t i = 1; i < span.size (); ++i) {
        if (span[i] < span[i - 1]) {
            is_sorted = false;
            break;
        }
    }
    assert (is_sorted);

    // Test std::find
    auto it = std::find (span.begin (), span.end (), std::byte{50});
    assert (it != span.end ());
    assert (*it == std::byte{50});
    assert (std::distance (span.begin (), it) == 50);

    // Test std::count
    std::fill (span.begin (), span.begin () + 10, std::byte{0xAA});
    auto count = std::count (span.begin (), span.end (), std::byte{0xAA});
    assert (count == 10);

    msg.close ();
}

int main ()
{
    test_blob_span ();
    test_msg_span ();
    test_span_zero_copy ();
    test_span_vsm_and_lmsg ();
    test_span_algorithms ();

    return 0;
}

#endif  // SL_HAVE_SPAN

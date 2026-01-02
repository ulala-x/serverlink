/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - C++20 std::format integration test */

#include <serverlink/config.h>

// Check for C++20 std::format support
// Note: MSVC uses _MSVC_LANG instead of __cplusplus for proper C++ version detection
#if SL_HAVE_STD_FORMAT && (__cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L))

#include "../../src/util/err.hpp"
#include "../../src/util/macros.hpp"
#include <cassert>
#include <iostream>
#include <sstream>

// Test std::format-based assertion helper
void test_format_assertion_helper() {
    // Test that the helper compiles and can be used
    // Note: We don't actually trigger the assertion since that would abort

    // This tests compilation only - slk_assert_fmt would abort if condition fails
    int x = 42;
    slk_assert_fmt(x > 0, "Value check: x={}", x);

    std::cout << "PASS: std::format assertion helper compiles correctly\n";
}

// Test debug logging macro (if enabled)
void test_debug_log_macro() {
#ifdef SL_ENABLE_DEBUG_LOG
    // This would output to stderr if SL_ENABLE_DEBUG_LOG is defined
    SL_DEBUG_LOG("Debug test: value={}\n", 123);
    std::cout << "PASS: Debug log macro works\n";
#else
    std::cout << "PASS: Debug log macro disabled (as expected)\n";
#endif
}

// Test that std::format works in general
void test_std_format_basic() {
    std::string result = std::format("Hello {}, value={}", "World", 42);
    assert(result == "Hello World, value=42");
    std::cout << "PASS: std::format basic functionality works\n";
}

int main() {
    std::cout << "=== Testing C++20 std::format Integration ===\n";

    test_std_format_basic();
    test_format_assertion_helper();
    test_debug_log_macro();

    std::cout << "\nAll std::format integration tests passed!\n";
    return 0;
}

#else  // !SL_HAVE_STD_FORMAT or !C++20

#include <iostream>

int main() {
    std::cout << "std::format not available or not C++20, skipping tests\n";
    return 0;
}

#endif  // SL_HAVE_STD_FORMAT && C++20

/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_ERR_HPP_INCLUDED
#define SL_ERR_HPP_INCLUDED

#include <cassert>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <netdb.h>
#endif

#include <serverlink/config.h>

// C++20 std::format for error messages (non-critical path)
#if SL_HAVE_STD_FORMAT
#include <format>
#include <iostream>
#endif

#include "likely.hpp"

// EPROTO is not used by OpenBSD and maybe other platforms.
// Keep as macro since it's used with system errno
#ifndef EPROTO
#define EPROTO 0
#endif

namespace slk {

// ServerLink-specific error codes (C++20 inline constexpr)
// These are inside slk namespace for type-safe usage
inline constexpr int SL_EFSM = 156;
inline constexpr int SL_ENOCOMPATPROTO = 157;
inline constexpr int SL_ETERM = 158;
inline constexpr int SL_EMTHREAD = 159;

}  // namespace slk

// Macros for global errno compatibility (outside namespace)
// These allow error codes to be used in switch cases and with errno
#define SL_EFSM slk::SL_EFSM
#define SL_ENOCOMPATPROTO slk::SL_ENOCOMPATPROTO
#define SL_ETERM slk::SL_ETERM
#define SL_EMTHREAD slk::SL_EMTHREAD
#define ETERM SL_ETERM

namespace slk {

const char *errno_to_string(int errno_);

#if defined __clang__
#if __has_feature(attribute_analyzer_noreturn)
void slk_abort(const char *errmsg_) __attribute__((analyzer_noreturn));
#else
void slk_abort(const char *errmsg_);
#endif
#elif defined __MSCVER__
__declspec(noreturn) void slk_abort(const char *errmsg_);
#else
void slk_abort(const char *errmsg_);
#endif

void print_backtrace();

}  // namespace slk

#ifdef _WIN32

namespace slk {

const char *wsa_error();
const char *wsa_error_no(int no_, const char *wsae_wouldblock_string_ = "Operation would block");
void win_error(char *buffer_, size_t buffer_size_);
int wsa_error_to_errno(int errcode_);

}  // namespace slk

// Provides convenient way to check WSA-style errors on Windows.
#define wsa_assert(x) \
    do { \
        if (!(x)) SL_UNLIKELY_ATTR { \
            const char *errstr = slk::wsa_error(); \
            if (errstr != nullptr) SL_UNLIKELY_ATTR { \
                fprintf(stderr, "Assertion failed: %s [%i] (%s:%d)\n", \
                        errstr, WSAGetLastError(), __FILE__, __LINE__); \
                fflush(stderr); \
                slk::slk_abort(errstr); \
            } \
        } \
    } while (false)

// Provides convenient way to assert on WSA-style errors on Windows.
#define wsa_assert_no(no) \
    do { \
        const char *errstr = slk::wsa_error_no(no); \
        if (errstr != nullptr) { \
            fprintf(stderr, "Assertion failed: %s (%s:%d)\n", errstr, \
                    __FILE__, __LINE__); \
            fflush(stderr); \
            slk::slk_abort(errstr); \
        } \
    } while (false)

// Provides convenient way to check GetLastError-style errors on Windows.
#define win_assert(x) \
    do { \
        if (!(x)) SL_UNLIKELY_ATTR { \
            char errstr[256]; \
            slk::win_error(errstr, 256); \
            fprintf(stderr, "Assertion failed: %s (%s:%d)\n", errstr, \
                    __FILE__, __LINE__); \
            fflush(stderr); \
            slk::slk_abort(errstr); \
        } \
    } while (false)

#endif

// This macro works in exactly the same way as the normal assert.
#define slk_assert(x) \
    do { \
        if (!(x)) SL_UNLIKELY_ATTR { \
            fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #x, __FILE__, \
                    __LINE__); \
            fflush(stderr); \
            slk::slk_abort(#x); \
        } \
    } while (false)

// Provides convenient way to check for errno-style errors.
#define errno_assert(x) \
    do { \
        if (!(x)) SL_UNLIKELY_ATTR { \
            const char *errstr = strerror(errno); \
            fprintf(stderr, "%s (%s:%d)\n", errstr, __FILE__, __LINE__); \
            fflush(stderr); \
            slk::slk_abort(errstr); \
        } \
    } while (false)

// Provides convenient way to check for POSIX errors.
#define posix_assert(x) \
    do { \
        if (x) SL_UNLIKELY_ATTR { \
            const char *errstr = strerror(x); \
            fprintf(stderr, "%s (%s:%d)\n", errstr, __FILE__, __LINE__); \
            fflush(stderr); \
            slk::slk_abort(errstr); \
        } \
    } while (false)

// Provides convenient way to check for errors from getaddrinfo.
#define gai_assert(x) \
    do { \
        if (x) SL_UNLIKELY_ATTR { \
            const char *errstr = gai_strerror(x); \
            fprintf(stderr, "%s (%s:%d)\n", errstr, __FILE__, __LINE__); \
            fflush(stderr); \
            slk::slk_abort(errstr); \
        } \
    } while (false)

// Provides convenient way to check whether memory allocation have succeeded.
#define alloc_assert(x) \
    do { \
        if (!(x)) SL_UNLIKELY_ATTR { \
            fprintf(stderr, "FATAL ERROR: OUT OF MEMORY (%s:%d)\n", __FILE__, \
                    __LINE__); \
            fflush(stderr); \
            slk::slk_abort("FATAL ERROR: OUT OF MEMORY"); \
        } \
    } while (false)

// C++20 std::format-based assertion helpers (optional, for cleaner code)
// These are only used in error paths (non-performance-critical)
#if SL_HAVE_STD_FORMAT && defined(__cplusplus) && __cplusplus >= 202002L

namespace slk {

// Helper for formatted assertion messages
template<typename... Args>
inline void assert_fail_formatted(std::format_string<Args...> fmt, Args&&... args) {
    std::cerr << std::format(fmt, std::forward<Args>(args)...) << std::endl;
    std::cerr.flush();
}

}  // namespace slk

// Modern C++20 formatted assertion (for new code)
// Example: slk_assert_fmt(x > 0, "Invalid value: x={}", x);
#define slk_assert_fmt(condition, ...) \
    do { \
        if (!(condition)) SL_UNLIKELY_ATTR { \
            ::slk::assert_fail_formatted(__VA_ARGS__); \
            ::slk::slk_abort(#condition); \
        } \
    } while (false)

#endif  // SL_HAVE_STD_FORMAT

#endif

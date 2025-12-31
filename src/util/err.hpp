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

#include "likely.hpp"

// EPROTO is not used by OpenBSD and maybe other platforms.
#ifndef EPROTO
#define EPROTO 0
#endif

// ServerLink-specific error codes
#define SL_EFSM          156
#define SL_ENOCOMPATPROTO 157
#define SL_ETERM         158
#define SL_EMTHREAD      159

// Backward compatibility aliases (for internal use)
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
        if (unlikely(!(x))) { \
            const char *errstr = slk::wsa_error(); \
            if (errstr != nullptr) { \
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
        if (unlikely(!(x))) { \
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
        if (unlikely(!(x))) { \
            fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #x, __FILE__, \
                    __LINE__); \
            fflush(stderr); \
            slk::slk_abort(#x); \
        } \
    } while (false)

// Provides convenient way to check for errno-style errors.
#define errno_assert(x) \
    do { \
        if (unlikely(!(x))) { \
            const char *errstr = strerror(errno); \
            fprintf(stderr, "%s (%s:%d)\n", errstr, __FILE__, __LINE__); \
            fflush(stderr); \
            slk::slk_abort(errstr); \
        } \
    } while (false)

// Provides convenient way to check for POSIX errors.
#define posix_assert(x) \
    do { \
        if (unlikely(x)) { \
            const char *errstr = strerror(x); \
            fprintf(stderr, "%s (%s:%d)\n", errstr, __FILE__, __LINE__); \
            fflush(stderr); \
            slk::slk_abort(errstr); \
        } \
    } while (false)

// Provides convenient way to check for errors from getaddrinfo.
#define gai_assert(x) \
    do { \
        if (unlikely(x)) { \
            const char *errstr = gai_strerror(x); \
            fprintf(stderr, "%s (%s:%d)\n", errstr, __FILE__, __LINE__); \
            fflush(stderr); \
            slk::slk_abort(errstr); \
        } \
    } while (false)

// Provides convenient way to check whether memory allocation have succeeded.
#define alloc_assert(x) \
    do { \
        if (unlikely(!(x))) { \
            fprintf(stderr, "FATAL ERROR: OUT OF MEMORY (%s:%d)\n", __FILE__, \
                    __LINE__); \
            fflush(stderr); \
            slk::slk_abort("FATAL ERROR: OUT OF MEMORY"); \
        } \
    } while (false)

#endif

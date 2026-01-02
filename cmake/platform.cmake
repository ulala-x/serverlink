# Platform detection and configuration for ServerLink
# This file detects platform-specific features and sets appropriate flags

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

# =============================================================================
# C++20 Feature Detection
# =============================================================================

# Check for C++20 concepts support
check_cxx_source_compiles("
#include <concepts>
template<typename T>
concept Addable = requires(T a, T b) { a + b; };
int main() { return 0; }
" SL_HAVE_CONCEPTS)
if(SL_HAVE_CONCEPTS)
    message(STATUS "C++20 concepts support detected")
else()
    message(WARNING "C++20 concepts NOT available")
endif()

# Check for C++20 ranges support
check_cxx_source_compiles("
#include <ranges>
#include <vector>
int main() {
    std::vector<int> v{1,2,3};
    auto r = v | std::views::filter([](int i){ return i > 1; });
    return 0;
}
" SL_HAVE_RANGES)
if(SL_HAVE_RANGES)
    message(STATUS "C++20 ranges support detected")
else()
    message(WARNING "C++20 ranges NOT available")
endif()

# Check for C++20 std::span support
check_cxx_source_compiles("
#include <span>
int main() {
    int arr[] = {1, 2, 3};
    std::span<int> s(arr);
    return s.size();
}
" SL_HAVE_SPAN)
if(SL_HAVE_SPAN)
    message(STATUS "C++20 std::span support detected")
else()
    message(WARNING "C++20 std::span NOT available")
endif()

# Check for C++20 std::format support
check_cxx_source_compiles("
#include <format>
#include <string>
int main() {
    std::string s = std::format(\"{}\", 42);
    return 0;
}
" SL_HAVE_STD_FORMAT)
if(SL_HAVE_STD_FORMAT)
    message(STATUS "C++20 std::format support detected")
else()
    message(STATUS "C++20 std::format NOT available (will use fallback)")
endif()

# Check for C++20 source_location support
check_cxx_source_compiles("
#include <source_location>
int main() {
    auto loc = std::source_location::current();
    return loc.line();
}
" SL_HAVE_SOURCE_LOCATION)
if(SL_HAVE_SOURCE_LOCATION)
    message(STATUS "C++20 source_location support detected")
endif()

# Check for C++20 three-way comparison (spaceship operator)
check_cxx_source_compiles("
#include <compare>
struct X {
    int value;
    auto operator<=>(const X&) const = default;
};
int main() {
    X a{1}, b{2};
    return (a <=> b) < 0 ? 0 : 1;
}
" SL_HAVE_THREE_WAY_COMPARISON)
if(SL_HAVE_THREE_WAY_COMPARISON)
    message(STATUS "C++20 three-way comparison support detected")
endif()

# Check for C++20 [[likely]]/[[unlikely]] attributes
check_cxx_source_compiles("
int main() {
    bool condition = true;
    if (condition) [[likely]] {
        return 0;
    } else [[unlikely]] {
        return 1;
    }
}
" SL_HAVE_LIKELY)
if(SL_HAVE_LIKELY)
    message(STATUS "C++20 [[likely]]/[[unlikely]] attributes support detected")
else()
    message(STATUS "C++20 [[likely]]/[[unlikely]] NOT available (will use __builtin_expect)")
endif()

# Check for C++20 consteval/constinit support
check_cxx_source_compiles("
#include <atomic>
consteval int square(int n) { return n * n; }
constinit int x = square(5);
constinit std::atomic<int> counter{0};
int main() { return x - 25; }
" SL_HAVE_CONSTEVAL)
if(SL_HAVE_CONSTEVAL)
    message(STATUS "C++20 consteval/constinit support detected")
else()
    message(STATUS "C++20 consteval/constinit NOT available (will use constexpr)")
endif()

# =============================================================================

# Detect Windows platform
if(WIN32)
    set(SL_HAVE_WINDOWS 1)
    message(STATUS "Windows platform detected")
else()
    set(SL_HAVE_WINDOWS 0)
endif()

# Detect Windows event polling (WSAEventSelect-based)
if(WIN32)
    set(SL_HAVE_WEPOLL 1)
    message(STATUS "Windows event polling (wepoll) support detected")
else()
    set(SL_HAVE_WEPOLL 0)
endif()

# Detect epoll (Linux)
check_include_file("sys/epoll.h" HAVE_EPOLL)
if(HAVE_EPOLL)
    set(SL_HAVE_EPOLL 1)
    message(STATUS "epoll support detected")
else()
    set(SL_HAVE_EPOLL 0)
endif()

# Detect kqueue (BSD, macOS)
check_include_file("sys/event.h" HAVE_KQUEUE)
if(HAVE_KQUEUE)
    set(SL_HAVE_KQUEUE 1)
    message(STATUS "kqueue support detected")
else()
    set(SL_HAVE_KQUEUE 0)
endif()

# Select is always available as fallback
set(SL_HAVE_SELECT 1)

# Determine which poller to use and set name
# Priority order: wepoll (Windows) > epoll (Linux) > kqueue (BSD/macOS) > select (fallback)
if(SL_HAVE_WEPOLL)
    set(SL_POLLER_NAME "wepoll")
elseif(SL_HAVE_EPOLL)
    set(SL_POLLER_NAME "epoll")
elseif(SL_HAVE_KQUEUE)
    set(SL_POLLER_NAME "kqueue")
else()
    set(SL_POLLER_NAME "select")
endif()

# Detect eventfd (Linux)
check_include_file("sys/eventfd.h" HAVE_EVENTFD)
if(HAVE_EVENTFD)
    set(SL_HAVE_EVENTFD 1)
    message(STATUS "eventfd support detected")
else()
    set(SL_HAVE_EVENTFD 0)
endif()

# Detect TCP keepalive options
check_symbol_exists(TCP_KEEPIDLE "netinet/tcp.h" HAVE_TCP_KEEPIDLE)
if(HAVE_TCP_KEEPIDLE)
    set(SL_HAVE_TCP_KEEPIDLE 1)
else()
    set(SL_HAVE_TCP_KEEPIDLE 0)
endif()

check_symbol_exists(TCP_KEEPINTVL "netinet/tcp.h" HAVE_TCP_KEEPINTVL)
if(HAVE_TCP_KEEPINTVL)
    set(SL_HAVE_TCP_KEEPINTVL 1)
else()
    set(SL_HAVE_TCP_KEEPINTVL 0)
endif()

check_symbol_exists(TCP_KEEPCNT "netinet/tcp.h" HAVE_TCP_KEEPCNT)
if(HAVE_TCP_KEEPCNT)
    set(SL_HAVE_TCP_KEEPCNT 1)
else()
    set(SL_HAVE_TCP_KEEPCNT 0)
endif()

# Windows-specific TCP keepalive
if(WIN32)
    set(SL_HAVE_TCP_KEEPALIVE_VALS 1)
else()
    set(SL_HAVE_TCP_KEEPALIVE_VALS 0)
endif()

# Detect SO_NOSIGPIPE (BSD, macOS)
check_symbol_exists(SO_NOSIGPIPE "sys/socket.h" HAVE_SO_NOSIGPIPE)
if(HAVE_SO_NOSIGPIPE)
    set(SL_HAVE_SO_NOSIGPIPE 1)
else()
    set(SL_HAVE_SO_NOSIGPIPE 0)
endif()

# Detect MSG_NOSIGNAL (Linux)
check_symbol_exists(MSG_NOSIGNAL "sys/socket.h" HAVE_MSG_NOSIGNAL)
if(HAVE_MSG_NOSIGNAL)
    set(SL_HAVE_MSG_NOSIGNAL 1)
else()
    set(SL_HAVE_MSG_NOSIGNAL 0)
endif()

# Detect IPC support (Unix Domain Sockets)
# Available on all Unix-like systems, not on Windows
if(NOT WIN32)
    check_include_file("sys/un.h" HAVE_SYS_UN_H)
    if(HAVE_SYS_UN_H)
        set(SL_HAVE_IPC 1)
        message(STATUS "IPC (Unix Domain Sockets) support detected")
    else()
        set(SL_HAVE_IPC 0)
    endif()
else()
    set(SL_HAVE_IPC 0)
endif()

# Platform-specific compiler flags
if(MSVC)
    # MSVC-specific flags
    add_compile_options(/W4)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
else()
    # GCC/Clang flags
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
    endif()
endif()

# Thread support
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Platform-specific link libraries
if(WIN32)
    set(PLATFORM_LIBS ws2_32 wsock32)
else()
    set(PLATFORM_LIBS ${CMAKE_THREAD_LIBS_INIT})
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND PLATFORM_LIBS rt)
    endif()
endif()

# Summary
message(STATUS "Platform configuration summary:")
message(STATUS "  I/O Poller:      ${SL_POLLER_NAME}")
message(STATUS "  wepoll:          ${SL_HAVE_WEPOLL}")
message(STATUS "  epoll:           ${SL_HAVE_EPOLL}")
message(STATUS "  kqueue:          ${SL_HAVE_KQUEUE}")
message(STATUS "  select:          ${SL_HAVE_SELECT}")
message(STATUS "  eventfd:         ${SL_HAVE_EVENTFD}")
message(STATUS "  IPC:             ${SL_HAVE_IPC}")
message(STATUS "  TCP_KEEPIDLE:    ${SL_HAVE_TCP_KEEPIDLE}")
message(STATUS "  SO_NOSIGPIPE:    ${SL_HAVE_SO_NOSIGPIPE}")
message(STATUS "  MSG_NOSIGNAL:    ${SL_HAVE_MSG_NOSIGNAL}")

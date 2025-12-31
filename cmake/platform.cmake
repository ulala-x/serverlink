# Platform detection and configuration for ServerLink
# This file detects platform-specific features and sets appropriate flags

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceCompiles)

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
if(SL_HAVE_EPOLL)
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
message(STATUS "  epoll:           ${SL_HAVE_EPOLL}")
message(STATUS "  kqueue:          ${SL_HAVE_KQUEUE}")
message(STATUS "  select:          ${SL_HAVE_SELECT}")
message(STATUS "  eventfd:         ${SL_HAVE_EVENTFD}")
message(STATUS "  TCP_KEEPIDLE:    ${SL_HAVE_TCP_KEEPIDLE}")
message(STATUS "  SO_NOSIGPIPE:    ${SL_HAVE_SO_NOSIGPIPE}")
message(STATUS "  MSG_NOSIGNAL:    ${SL_HAVE_MSG_NOSIGNAL}")

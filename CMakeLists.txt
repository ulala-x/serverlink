cmake_minimum_required(VERSION 3.14)

project(serverlink
    VERSION 0.1.0
    DESCRIPTION "High-performance message routing library with ZMQ ROUTER-like semantics"
    LANGUAGES C CXX
)

# Project options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform detection and configuration
include(cmake/platform.cmake)

# Generate configuration header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/serverlink/config.h"
    @ONLY
)

# Library source files
set(SERVERLINK_SOURCES
    # Public C API
    src/serverlink.cpp

    # Utility sources
    src/util/err.cpp
    src/util/clock.cpp
    src/util/random.cpp
    src/util/thread.cpp

    # Message sources
    src/msg/metadata.cpp
    src/msg/msg.cpp

    # Pipe sources
    src/pipe/pipe.cpp
    src/pipe/fq.cpp

    # Core sources
    src/core/options.cpp
    src/core/object.cpp
    src/core/own.cpp
    src/core/ctx.cpp
    src/core/endpoint.cpp
    src/core/socket_base.cpp
    src/core/router.cpp

    # I/O sources
    src/io/ip.cpp
    src/io/polling_util.cpp
    src/io/signaler.cpp
    src/io/mailbox.cpp
    src/io/mailbox_safe.cpp
    src/io/poller_base.cpp
    src/io/io_thread.cpp
    src/io/io_object.cpp
    src/io/reaper.cpp

    # Core additional sources
    src/core/session_base.cpp

    # Authentication sources
    src/auth/mechanism.cpp
    src/auth/mechanism_base.cpp
    src/auth/null_mechanism.cpp

    # Transport sources
    src/transport/address.cpp
    src/transport/tcp_address.cpp
    src/transport/ip_resolver.cpp
    src/transport/tcp.cpp
    src/transport/stream_connecter_base.cpp
    src/transport/stream_listener_base.cpp
    src/transport/tcp_connecter.cpp
    src/transport/tcp_listener.cpp

    # Protocol sources
    src/protocol/decoder_allocators.cpp
    src/protocol/v2_decoder.cpp
    src/protocol/v2_encoder.cpp
    src/protocol/v3_1_encoder.cpp
    src/protocol/raw_encoder.cpp
    src/protocol/raw_decoder.cpp
    src/protocol/stream_engine_base.cpp
    src/protocol/zmtp_engine.cpp

    # Monitoring sources
    src/monitor/connection_manager.cpp
    src/monitor/event_dispatcher.cpp
    src/monitor/heartbeat.cpp
)

# Platform-specific I/O sources
if(SL_HAVE_EPOLL)
    list(APPEND SERVERLINK_SOURCES src/io/epoll.cpp)
endif()

# For now, create an interface library since we have no source files yet
# This will be changed to add_library once we add actual source files
if(SERVERLINK_SOURCES)
    add_library(serverlink ${SERVERLINK_SOURCES})

    target_include_directories(serverlink
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(serverlink
        PRIVATE
            ${PLATFORM_LIBS}
    )

    # Export symbols for shared library
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(serverlink
            PRIVATE SL_BUILDING_DLL
            INTERFACE SL_USING_DLL
        )
    endif()

    # Set library properties
    set_target_properties(serverlink PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "include/serverlink/serverlink.h;include/serverlink/serverlink_export.h"
    )
else()
    # Placeholder interface library for Phase 1
    add_library(serverlink INTERFACE)

    target_include_directories(serverlink INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
include(GNUInstallDirs)

if(SERVERLINK_SOURCES)
    install(TARGETS serverlink
        EXPORT serverlinkTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/serverlink
    )
endif()

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/serverlink/serverlink.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/serverlink/serverlink_export.h
    ${CMAKE_CURRENT_BINARY_DIR}/include/serverlink/config.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/serverlink
)

# Export targets (only if we have actual library sources)
if(SERVERLINK_SOURCES)
    install(EXPORT serverlinkTargets
        FILE serverlinkTargets.cmake
        NAMESPACE serverlink::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/serverlink
    )

    # Create config file
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/serverlinkConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/serverlinkConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/serverlinkConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/serverlink
    )

    # Install config files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/serverlinkConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/serverlinkConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/serverlink
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "ServerLink Configuration Summary:")
message(STATUS "  Version:         ${PROJECT_VERSION}")
message(STATUS "  Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared libs:     ${BUILD_SHARED_LIBS}")
message(STATUS "  Build tests:     ${BUILD_TESTS}")
message(STATUS "  Build examples:  ${BUILD_EXAMPLES}")
message(STATUS "  C++ standard:    C++${CMAKE_CXX_STANDARD}")
message(STATUS "  I/O poller:      ${SL_POLLER_NAME}")
message(STATUS "  Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")

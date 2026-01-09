# ServerLink Test Suite

enable_testing()

# Common include directories for all tests
set(TEST_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

# Helper function to create a test executable
function(add_serverlink_test test_name source_file)
    add_executable(${test_name} ${source_file})

    target_include_directories(${test_name} PRIVATE ${TEST_INCLUDE_DIRS})

    target_link_libraries(${test_name} PRIVATE serverlink)

    # Add as a CTest
    add_test(NAME ${test_name} COMMAND ${test_name})

    # Set test properties
    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 30
        LABELS "${ARGN}"
    )
endfunction()

# Unit Tests
message(STATUS "Adding unit tests...")
add_serverlink_test(test_msg unit/test_msg.cpp "unit")
add_serverlink_test(test_ctx unit/test_ctx.cpp "unit")
add_serverlink_test(test_ctx_options unit/test_ctx_options.cpp "unit")
add_serverlink_test(test_hwm unit/test_hwm.cpp "unit")
add_serverlink_test(test_sockopt_hwm unit/test_sockopt_hwm.cpp "unit")
add_serverlink_test(test_last_endpoint unit/test_last_endpoint.cpp "unit")
add_serverlink_test(test_pair unit/test_pair.cpp "unit")
add_serverlink_test(test_tcp_keepalive unit/test_tcp_keepalive.cpp "unit")
add_serverlink_test(test_error_handling unit/test_error_handling.cpp "unit")
add_serverlink_test(test_span_api unit/test_span_api.cpp "unit")
add_serverlink_test(test_format_helpers unit/test_format_helpers.cpp "unit")

# Asio Integration Test (only if SL_USE_ASIO is enabled)
if(SL_USE_ASIO)
    message(STATUS "Adding Asio integration test...")
    add_serverlink_test(test_asio_integration unit/test_asio_integration.cpp "unit;asio")
    target_compile_definitions(test_asio_integration PRIVATE SL_USE_ASIO)
    target_include_directories(test_asio_integration PRIVATE ${asio_SOURCE_DIR}/asio/include)

    message(STATUS "Adding TCP stream test...")
    add_serverlink_test(test_tcp_stream unit/test_tcp_stream.cpp "unit;asio")
    target_compile_definitions(test_tcp_stream PRIVATE SL_USE_ASIO)
    target_include_directories(test_tcp_stream PRIVATE ${asio_SOURCE_DIR}/asio/include)
endif()

# Utility Tests
message(STATUS "Adding utility tests...")
add_serverlink_test(test_atomics util/test_atomics.cpp "util")
add_serverlink_test(test_timers util/test_timers.cpp "util")
add_serverlink_test(test_stopwatch util/test_stopwatch.cpp "util")
add_serverlink_test(test_has util/test_has.cpp "util")

# Pattern Tests
message(STATUS "Adding pattern tests...")
add_serverlink_test(test_glob_pattern pattern/test_glob_pattern.cpp "pattern")
add_serverlink_test(test_pattern_trie pattern/test_pattern_trie.cpp "pattern")

# Router Tests
message(STATUS "Adding router tests...")
add_serverlink_test(test_router_basic router/test_router_basic.cpp "router")
add_serverlink_test(test_router_mandatory router/test_router_mandatory.cpp "router")
add_serverlink_test(test_router_handover router/test_router_handover.cpp "router")
add_serverlink_test(test_router_notify router/test_router_notify.cpp "router")
add_serverlink_test(test_router_mandatory_hwm router/test_router_mandatory_hwm.cpp "router")
add_serverlink_test(test_spec_router router/test_spec_router.cpp "router")
add_serverlink_test(test_connect_rid router/test_connect_rid.cpp "router")
add_serverlink_test(test_probe_router router/test_probe_router.cpp "router")

# Integration Tests
message(STATUS "Adding integration tests...")
add_serverlink_test(test_router_to_router integration/test_router_to_router.cpp "integration")
# add_serverlink_test(test_xpub_simple integration/test_xpub_simple.cpp "integration")  # TODO: Create this test

# Monitor Tests
message(STATUS "Adding monitor tests...")
add_serverlink_test(test_peer_stats monitor/test_peer_stats.cpp "monitor")
add_serverlink_test(test_heartbeat monitor/test_heartbeat.cpp "monitor")

# SPOT PUB/SUB Tests
message(STATUS "Adding spot tests...")
add_serverlink_test(test_spot_basic spot/test_spot_basic.cpp "spot")
add_serverlink_test(test_spot_local spot/test_spot_local.cpp "spot")
add_serverlink_test(test_spot_remote spot/test_spot_remote.cpp "spot")
add_serverlink_test(test_spot_cluster spot/test_spot_cluster.cpp "spot")
add_serverlink_test(test_spot_mixed spot/test_spot_mixed.cpp "spot")

# Transport Tests
message(STATUS "Adding transport tests...")
add_serverlink_test(test_bind_after_connect transport/test_bind_after_connect.cpp "transport")
add_serverlink_test(test_inproc_connect transport/test_inproc_connect.cpp "transport")
add_serverlink_test(test_reconnect_ivl transport/test_reconnect_ivl.cpp "transport")
add_serverlink_test(test_ipc_basic transport/test_ipc_basic.cpp "transport")

# Windows-specific Tests
if(WIN32)
    message(STATUS "Adding Windows-specific tests...")
    add_serverlink_test(test_wsastartup_order windows/test_wsastartup_order.cpp "windows")
endif()

# Poller Tests
message(STATUS "Adding poller tests...")
add_serverlink_test(test_poller poller/test_poller.cpp "poller")

# Proxy Tests
message(STATUS "Adding proxy tests...")
# add_serverlink_test(test_proxy_simple proxy/test_proxy_simple.cpp "proxy")
# Full proxy test with threading (complex, may timeout)
# add_serverlink_test(test_proxy proxy/test_proxy.cpp "proxy")

# Custom test targets
add_custom_target(test-unit
    COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
    DEPENDS test_msg test_ctx test_ctx_options test_hwm test_sockopt_hwm test_last_endpoint test_span_api
    COMMENT "Running unit tests"
)

add_custom_target(test-router
    COMMAND ${CMAKE_CTEST_COMMAND} -L router --output-on-failure
    DEPENDS test_router_basic test_router_mandatory test_router_handover
            test_router_notify test_router_mandatory_hwm test_spec_router
            test_connect_rid test_probe_router
    COMMENT "Running router tests"
)

add_custom_target(test-integration
    COMMAND ${CMAKE_CTEST_COMMAND} -L integration --output-on-failure
    DEPENDS test_router_to_router
    COMMENT "Running integration tests"
)

add_custom_target(test-monitor
    COMMAND ${CMAKE_CTEST_COMMAND} -L monitor --output-on-failure
    DEPENDS test_peer_stats
    COMMENT "Running monitor tests"
)

add_custom_target(test-spot
    COMMAND ${CMAKE_CTEST_COMMAND} -L spot --output-on-failure
    DEPENDS test_spot_basic test_spot_local test_spot_remote test_spot_cluster test_spot_mixed
    COMMENT "Running SPOT PUB/SUB tests"
)

add_custom_target(test-transport
    COMMAND ${CMAKE_CTEST_COMMAND} -L transport --output-on-failure
    DEPENDS test_bind_after_connect test_inproc_connect test_reconnect_ivl test_ipc_basic
    COMMENT "Running transport tests"
)

add_custom_target(test-poller
    COMMAND ${CMAKE_CTEST_COMMAND} -L poller --output-on-failure
    DEPENDS test_poller
    COMMENT "Running poller tests"
)

add_custom_target(test-proxy
    COMMAND ${CMAKE_CTEST_COMMAND} -L proxy --output-on-failure
    DEPENDS # test_proxy_simple
    COMMENT "Running proxy tests"
)

add_custom_target(test-util
    COMMAND ${CMAKE_CTEST_COMMAND} -L util --output-on-failure
    DEPENDS test_atomics test_timers test_stopwatch test_has
    COMMENT "Running utility tests"
)

add_custom_target(test-pattern
    COMMAND ${CMAKE_CTEST_COMMAND} -L pattern --output-on-failure
    DEPENDS test_glob_pattern test_pattern_trie
    COMMENT "Running pattern matching tests"
)

add_custom_target(test-all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS
        test_msg
        test_ctx
        test_ctx_options
        test_hwm
        test_sockopt_hwm
        test_span_api
        test_router_basic
        test_router_mandatory
        test_router_handover
        test_router_notify
        test_router_mandatory_hwm
        test_spec_router
        test_connect_rid
        test_probe_router
        test_router_to_router
        test_peer_stats
        test_bind_after_connect
        test_inproc_connect
        test_reconnect_ivl
        test_ipc_basic
        test_poller
        #        test_proxy_simple
        test_atomics
        test_timers
        test_stopwatch
        test_has
        test_glob_pattern
        test_pattern_trie
        test_spot_basic
        test_spot_local
        test_spot_remote
        test_spot_cluster
        test_spot_mixed
    COMMENT "Running all tests"
)

# Benchmarks (performance tests, not included in CTest)
add_subdirectory(benchmark)

# Print summary
message(STATUS "")
message(STATUS "Test Suite Configuration:")
message(STATUS "  Unit tests:        test_msg, test_ctx, test_ctx_options, test_hwm, test_sockopt_hwm")
message(STATUS "  Router tests:      test_router_basic, test_router_mandatory, test_router_handover,")
message(STATUS "                     test_router_notify, test_router_mandatory_hwm, test_spec_router,")
message(STATUS "                     test_connect_rid, test_probe_router")
message(STATUS "  Integration tests: test_router_to_router")
message(STATUS "  Monitor tests:     test_peer_stats")
message(STATUS "  Transport tests:   test_bind_after_connect, test_inproc_connect, test_reconnect_ivl, test_ipc_basic")
message(STATUS "  Poller tests:      test_poller")
message(STATUS "  SPOT tests:        test_spot_basic, test_spot_local, test_spot_remote,")
message(STATUS "                     test_spot_cluster, test_spot_mixed")
message(STATUS "")
message(STATUS "Run tests with:")
message(STATUS "  make test           - Run all tests with CTest")
message(STATUS "  make test-unit      - Run unit tests only")
message(STATUS "  make test-util      - Run utility tests only")
message(STATUS "  make test-router    - Run router tests only")
message(STATUS "  make test-spot      - Run SPOT PUB/SUB tests only")
message(STATUS "  make test-transport - Run transport tests only")
message(STATUS "  make test-poller    - Run poller tests only")
message(STATUS "  make test-all       - Run all tests with detailed output")
message(STATUS "")
add_executable(test_ctx_concurrency unit/test_ctx_concurrency.cpp)
target_link_libraries(test_ctx_concurrency PRIVATE serverlink)
add_test(NAME test_ctx_concurrency COMMAND test_ctx_concurrency)
add_executable(bench_inproc_relay benchmark/bench_inproc_relay.cpp)
target_link_libraries(bench_inproc_relay PRIVATE serverlink)
add_executable(test_dealer pattern/test_dealer.cpp)
target_link_libraries(test_dealer PRIVATE serverlink)
add_test(NAME test_dealer COMMAND test_dealer)
add_executable(bench_dealer_router_thr benchmark/bench_dealer_router_thr.cpp)
target_link_libraries(bench_dealer_router_thr PRIVATE serverlink)
add_executable(bench_dealer_router_lat benchmark/bench_dealer_router_lat.cpp)
target_link_libraries(bench_dealer_router_lat PRIVATE serverlink)
add_executable(bench_all_types benchmark/bench_all_types.cpp)
target_link_libraries(bench_all_types PRIVATE serverlink)
add_executable(bench_slk_perf benchmark/bench_slk_perf.cpp)
target_link_libraries(bench_slk_perf PRIVATE serverlink)

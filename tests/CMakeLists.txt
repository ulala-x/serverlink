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
add_serverlink_test(test_xpub_simple integration/test_xpub_simple.cpp "integration")

# Monitor Tests
message(STATUS "Adding monitor tests...")
add_serverlink_test(test_peer_stats monitor/test_peer_stats.cpp "monitor")

# PubSub Tests
message(STATUS "Adding pubsub tests...")
add_serverlink_test(test_auth_pubsub pubsub/test_auth_pubsub.cpp "pubsub")
add_serverlink_test(test_hwm_pubsub pubsub/test_hwm_pubsub.cpp "pubsub")
add_serverlink_test(test_sub_forward pubsub/test_sub_forward.cpp "pubsub")
add_serverlink_test(test_pubsub_topics_count pubsub/test_pubsub_topics_count.cpp "pubsub")
add_serverlink_test(test_xpub_verbose pubsub/test_xpub_verbose.cpp "pubsub")
add_serverlink_test(test_xpub_manual pubsub/test_xpub_manual.cpp "pubsub")
add_serverlink_test(test_xpub_nodrop pubsub/test_xpub_nodrop.cpp "pubsub")
add_serverlink_test(test_psubscribe pubsub/test_psubscribe.cpp "pubsub")
add_serverlink_test(test_introspection pubsub/test_introspection.cpp "pubsub")
add_serverlink_test(test_sharded_pubsub pubsub/test_sharded_pubsub.cpp "pubsub")
add_serverlink_test(test_pubsub_broker pubsub/test_pubsub_broker.cpp "pubsub")
add_serverlink_test(test_pubsub_cluster pubsub/test_pubsub_cluster.cpp "pubsub")
# TODO: Fix concurrency test to properly use XPUB with async draining
# add_serverlink_test(test_registry_concurrent pubsub/test_registry_concurrent.cpp "pubsub")

# Debug test (not part of regular test suite)
add_executable(debug_xpub_manual debug_xpub_manual.cpp)
target_include_directories(debug_xpub_manual PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(debug_xpub_manual PRIVATE serverlink)

# Transport Tests
message(STATUS "Adding transport tests...")
add_serverlink_test(test_bind_after_connect transport/test_bind_after_connect.cpp "transport")
add_serverlink_test(test_inproc_connect transport/test_inproc_connect.cpp "transport")
add_serverlink_test(test_reconnect_ivl transport/test_reconnect_ivl.cpp "transport")

# Poller Tests
message(STATUS "Adding poller tests...")
add_serverlink_test(test_poller poller/test_poller.cpp "poller")

# Proxy Tests
message(STATUS "Adding proxy tests...")
add_serverlink_test(test_proxy_simple proxy/test_proxy_simple.cpp "proxy")
# Full proxy test with threading (complex, may timeout)
# add_serverlink_test(test_proxy proxy/test_proxy.cpp "proxy")

# Custom test targets
add_custom_target(test-unit
    COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
    DEPENDS test_msg test_ctx test_ctx_options test_hwm test_sockopt_hwm test_last_endpoint
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

add_custom_target(test-pubsub
    COMMAND ${CMAKE_CTEST_COMMAND} -L pubsub --output-on-failure
    DEPENDS test_auth_pubsub test_hwm_pubsub test_sub_forward test_pubsub_topics_count
            test_xpub_verbose test_xpub_manual test_xpub_nodrop test_psubscribe
            test_introspection test_sharded_pubsub test_pubsub_broker test_pubsub_cluster
    COMMENT "Running pubsub tests"
)

add_custom_target(test-transport
    COMMAND ${CMAKE_CTEST_COMMAND} -L transport --output-on-failure
    DEPENDS test_bind_after_connect test_inproc_connect test_reconnect_ivl
    COMMENT "Running transport tests"
)

add_custom_target(test-poller
    COMMAND ${CMAKE_CTEST_COMMAND} -L poller --output-on-failure
    DEPENDS test_poller
    COMMENT "Running poller tests"
)

add_custom_target(test-proxy
    COMMAND ${CMAKE_CTEST_COMMAND} -L proxy --output-on-failure
    DEPENDS test_proxy_simple
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
        test_auth_pubsub
        test_hwm_pubsub
        test_sub_forward
        test_pubsub_topics_count
        test_xpub_verbose
        test_xpub_manual
        test_xpub_nodrop
        test_psubscribe
        test_introspection
        test_sharded_pubsub
        test_pubsub_broker
        test_pubsub_cluster
        test_bind_after_connect
        test_inproc_connect
        test_reconnect_ivl
        test_poller
        test_proxy_simple
        test_atomics
        test_timers
        test_stopwatch
        test_has
        test_glob_pattern
        test_pattern_trie
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
message(STATUS "  PubSub tests:      test_auth_pubsub, test_hwm_pubsub, test_sub_forward,")
message(STATUS "                     test_pubsub_topics_count, test_xpub_verbose,")
message(STATUS "                     test_xpub_manual, test_xpub_nodrop")
message(STATUS "  Transport tests:   test_bind_after_connect, test_inproc_connect, test_reconnect_ivl")
message(STATUS "  Poller tests:      test_poller")
message(STATUS "")
message(STATUS "Run tests with:")
message(STATUS "  make test           - Run all tests with CTest")
message(STATUS "  make test-unit      - Run unit tests only")
message(STATUS "  make test-util      - Run utility tests only")
message(STATUS "  make test-router    - Run router tests only")
message(STATUS "  make test-pubsub    - Run pubsub tests only")
message(STATUS "  make test-transport - Run transport tests only")
message(STATUS "  make test-poller    - Run poller tests only")
message(STATUS "  make test-all       - Run all tests with detailed output")
message(STATUS "")

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
add_serverlink_test(test_hwm unit/test_hwm.cpp "unit")
add_serverlink_test(test_sockopt_hwm unit/test_sockopt_hwm.cpp "unit")

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

# Debug test (not part of regular test suite)
add_executable(debug_xpub_manual debug_xpub_manual.cpp)
target_include_directories(debug_xpub_manual PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(debug_xpub_manual PRIVATE serverlink)

# Transport Tests
message(STATUS "Adding transport tests...")
add_serverlink_test(test_bind_after_connect transport/test_bind_after_connect.cpp "transport")
add_serverlink_test(test_inproc_connect transport/test_inproc_connect.cpp "transport")
add_serverlink_test(test_reconnect_ivl transport/test_reconnect_ivl.cpp "transport")

# Custom test targets
add_custom_target(test-unit
    COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
    DEPENDS test_msg test_ctx test_hwm test_sockopt_hwm
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
            test_xpub_verbose test_xpub_manual test_xpub_nodrop
    COMMENT "Running pubsub tests"
)

add_custom_target(test-transport
    COMMAND ${CMAKE_CTEST_COMMAND} -L transport --output-on-failure
    DEPENDS test_bind_after_connect test_inproc_connect test_reconnect_ivl
    COMMENT "Running transport tests"
)

add_custom_target(test-all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS
        test_msg
        test_ctx
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
        test_bind_after_connect
        test_inproc_connect
        test_reconnect_ivl
    COMMENT "Running all tests"
)

# Benchmarks (performance tests, not included in CTest)
add_subdirectory(benchmark)

# Print summary
message(STATUS "")
message(STATUS "Test Suite Configuration:")
message(STATUS "  Unit tests:        test_msg, test_ctx, test_hwm, test_sockopt_hwm")
message(STATUS "  Router tests:      test_router_basic, test_router_mandatory, test_router_handover,")
message(STATUS "                     test_router_notify, test_router_mandatory_hwm, test_spec_router,")
message(STATUS "                     test_connect_rid, test_probe_router")
message(STATUS "  Integration tests: test_router_to_router")
message(STATUS "  Monitor tests:     test_peer_stats")
message(STATUS "  PubSub tests:      test_auth_pubsub, test_hwm_pubsub, test_sub_forward,")
message(STATUS "                     test_pubsub_topics_count, test_xpub_verbose,")
message(STATUS "                     test_xpub_manual, test_xpub_nodrop")
message(STATUS "  Transport tests:   test_bind_after_connect, test_inproc_connect, test_reconnect_ivl")
message(STATUS "")
message(STATUS "Run tests with:")
message(STATUS "  make test           - Run all tests with CTest")
message(STATUS "  make test-unit      - Run unit tests only")
message(STATUS "  make test-router    - Run router tests only")
message(STATUS "  make test-pubsub    - Run pubsub tests only")
message(STATUS "  make test-transport - Run transport tests only")
message(STATUS "  make test-all       - Run all tests with detailed output")
message(STATUS "")

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

# Router Tests
message(STATUS "Adding router tests...")
add_serverlink_test(test_router_basic router/test_router_basic.cpp "router")
add_serverlink_test(test_router_mandatory router/test_router_mandatory.cpp "router")
add_serverlink_test(test_router_handover router/test_router_handover.cpp "router")

# Integration Tests
message(STATUS "Adding integration tests...")
add_serverlink_test(test_router_to_router integration/test_router_to_router.cpp "integration")

# Monitor Tests
message(STATUS "Adding monitor tests...")
add_serverlink_test(test_peer_stats monitor/test_peer_stats.cpp "monitor")

# Custom test targets
add_custom_target(test-unit
    COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
    DEPENDS test_msg test_ctx
    COMMENT "Running unit tests"
)

add_custom_target(test-router
    COMMAND ${CMAKE_CTEST_COMMAND} -L router --output-on-failure
    DEPENDS test_router_basic test_router_mandatory test_router_handover
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

add_custom_target(test-all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS
        test_msg
        test_ctx
        test_router_basic
        test_router_mandatory
        test_router_handover
        test_router_to_router
        test_peer_stats
    COMMENT "Running all tests"
)

# Print summary
message(STATUS "")
message(STATUS "Test Suite Configuration:")
message(STATUS "  Unit tests:        test_msg, test_ctx")
message(STATUS "  Router tests:      test_router_basic, test_router_mandatory, test_router_handover")
message(STATUS "  Integration tests: test_router_to_router")
message(STATUS "  Monitor tests:     test_peer_stats")
message(STATUS "")
message(STATUS "Run tests with:")
message(STATUS "  make test          - Run all tests with CTest")
message(STATUS "  make test-unit     - Run unit tests only")
message(STATUS "  make test-router   - Run router tests only")
message(STATUS "  make test-all      - Run all tests with detailed output")
message(STATUS "")

# ServerLink Benchmark Suite

# Benchmark executables are not run as part of CTest
# They are for performance testing and profiling

# Throughput benchmark
add_executable(bench_throughput bench_throughput.cpp)
target_link_libraries(bench_throughput PRIVATE serverlink)
target_include_directories(bench_throughput PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Latency benchmark
add_executable(bench_latency bench_latency.cpp)
target_link_libraries(bench_latency PRIVATE serverlink)
target_include_directories(bench_latency PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Set C++11 for chrono and threads
set_target_properties(bench_throughput bench_latency PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Link pthread on Unix platforms
if(UNIX)
    target_link_libraries(bench_throughput PRIVATE pthread)
    target_link_libraries(bench_latency PRIVATE pthread)
endif()

# Add custom target to run all benchmarks
add_custom_target(benchmark
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bench_throughput
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bench_latency
    DEPENDS bench_throughput bench_latency
    COMMENT "Running performance benchmarks"
    VERBATIM
)

message(STATUS "")
message(STATUS "Benchmark Configuration:")
message(STATUS "  Throughput benchmark: bench_throughput")
message(STATUS "  Latency benchmark:    bench_latency")
message(STATUS "")
message(STATUS "Run benchmarks with:")
message(STATUS "  make benchmark         - Run all benchmarks")
message(STATUS "  ./bench_throughput     - Run throughput benchmark only")
message(STATUS "  ./bench_latency        - Run latency benchmark only")
message(STATUS "")

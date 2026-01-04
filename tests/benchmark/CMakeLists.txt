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

# PUB/SUB benchmark
add_executable(bench_pubsub bench_pubsub.cpp)
target_link_libraries(bench_pubsub PRIVATE serverlink)
target_include_directories(bench_pubsub PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Profiling benchmark
add_executable(bench_profile bench_profile.cpp)
target_link_libraries(bench_profile PRIVATE serverlink)
target_include_directories(bench_profile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# SPOT PUB/SUB throughput benchmark
add_executable(bench_spot_throughput bench_spot_throughput.cpp)
target_link_libraries(bench_spot_throughput PRIVATE serverlink)
target_include_directories(bench_spot_throughput PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# SPOT PUB/SUB latency benchmark
add_executable(bench_spot_latency bench_spot_latency.cpp)
target_link_libraries(bench_spot_latency PRIVATE serverlink)
target_include_directories(bench_spot_latency PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# SPOT PUB/SUB scalability benchmark
add_executable(bench_spot_scalability bench_spot_scalability.cpp)
target_link_libraries(bench_spot_scalability PRIVATE serverlink)
target_include_directories(bench_spot_scalability PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Set C++11 for chrono and threads
set_target_properties(
    bench_throughput bench_latency bench_pubsub bench_profile
    bench_spot_throughput bench_spot_latency bench_spot_scalability
    PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Link pthread on Unix platforms
if(UNIX)
    target_link_libraries(bench_throughput PRIVATE pthread)
    target_link_libraries(bench_latency PRIVATE pthread)
    target_link_libraries(bench_pubsub PRIVATE pthread)
    target_link_libraries(bench_profile PRIVATE pthread)
    target_link_libraries(bench_spot_throughput PRIVATE pthread)
    target_link_libraries(bench_spot_latency PRIVATE pthread)
    target_link_libraries(bench_spot_scalability PRIVATE pthread)
endif()

# Add custom target to run all benchmarks
add_custom_target(benchmark
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bench_throughput
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bench_latency
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bench_pubsub
    DEPENDS bench_throughput bench_latency bench_pubsub
    COMMENT "Running performance benchmarks"
    VERBATIM
)

# Add custom target to run SPOT benchmarks
add_custom_target(benchmark_spot
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bench_spot_throughput
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bench_spot_latency
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bench_spot_scalability
    DEPENDS bench_spot_throughput bench_spot_latency bench_spot_scalability
    COMMENT "Running SPOT PUB/SUB benchmarks"
    VERBATIM
)

message(STATUS "")
message(STATUS "Benchmark Configuration:")
message(STATUS "  Throughput benchmark:        bench_throughput")
message(STATUS "  Latency benchmark:           bench_latency")
message(STATUS "  PUB/SUB benchmark:           bench_pubsub")
message(STATUS "  SPOT Throughput benchmark:   bench_spot_throughput")
message(STATUS "  SPOT Latency benchmark:      bench_spot_latency")
message(STATUS "  SPOT Scalability benchmark:  bench_spot_scalability")
message(STATUS "")
message(STATUS "Run benchmarks with:")
message(STATUS "  make benchmark              - Run all core benchmarks")
message(STATUS "  make benchmark_spot         - Run all SPOT benchmarks")
message(STATUS "  ./bench_spot_throughput     - Run SPOT throughput only")
message(STATUS "  ./bench_spot_latency        - Run SPOT latency only")
message(STATUS "  ./bench_spot_scalability    - Run SPOT scalability only")
message(STATUS "")

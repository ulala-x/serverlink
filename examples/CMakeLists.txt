# ServerLink Examples

# Simple C API Test
add_executable(test_simple test_simple.c)
target_link_libraries(test_simple serverlink)

# Full C API Test
add_executable(test_c_api test_c_api.c)
target_link_libraries(test_c_api serverlink)

# Router-to-Router Simple Example
add_executable(router_to_router_simple router_to_router_simple.c)
target_link_libraries(router_to_router_simple serverlink)

# Inproc Timing Test
add_executable(test_inproc_timing test_inproc_timing.c)
target_link_libraries(test_inproc_timing serverlink)

message(STATUS "Examples built: test_simple, test_c_api, router_to_router_simple, test_inproc_timing")

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

# ========================================
# Redis-Style Pub/Sub Examples
# ========================================

# Pattern Subscription Example
add_executable(psubscribe_example pubsub/psubscribe_example.c)
target_link_libraries(psubscribe_example serverlink)

# Broker Example
add_executable(broker_example pubsub/broker_example.c)
target_link_libraries(broker_example serverlink pthread)

# Sharded Pub/Sub Example
add_executable(sharded_example pubsub/sharded_example.c)
target_link_libraries(sharded_example serverlink pthread)

# Cluster Pub/Sub Example
add_executable(cluster_example pubsub/cluster_example.c)
target_link_libraries(cluster_example serverlink pthread)

# Mesh Topology Example (MMORPG Cell Pattern)
add_executable(mesh_topology_example pubsub/mesh_topology_example.c)
target_link_libraries(mesh_topology_example serverlink pthread)

message(STATUS "Examples built: test_simple, test_c_api, router_to_router_simple, test_inproc_timing")
message(STATUS "Pub/Sub examples: psubscribe_example, broker_example, sharded_example, cluster_example, mesh_topology_example")

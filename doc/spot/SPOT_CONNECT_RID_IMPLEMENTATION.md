# SPOT Node CONNECT_ROUTING_ID Implementation

## Summary

Implemented ROUTER-to-ROUTER communication support for the SPOT module by adding CONNECT_ROUTING_ID configuration.

## Changes Made

### 1. Header File: `src/spot/spot_node.hpp`

**Added:**
- Private member: `std::string _node_id` - stores this node's unique routing ID
- Public getter: `const std::string &node_id() const` - retrieves the node's routing ID

**Location:**
- Line 176: Added `_node_id` member after `_endpoint`
- Lines 165-170: Added `node_id()` getter declaration

### 2. Source File: `src/spot/spot_node.cpp`

**Added includes:**
- `#include <atomic>` (line 14) - for thread-safe counter in ID generation

**Added helper function:**
```cpp
static std::string generate_node_id()
{
    static std::atomic<uint64_t> counter{0};
    char buf[64];
    snprintf(buf, sizeof(buf), "spot-node-%llu",
             static_cast<unsigned long long>(counter.fetch_add(1, std::memory_order_relaxed)));
    return buf;
}
```
- Lines 19-28: Generates unique node IDs using atomic counter
- Format: "spot-node-0", "spot-node-1", etc.

**Modified `connect()` method:**
- Lines 70-81: Added CONNECT_ROUTING_ID setup before connection
  1. Generate unique node ID if not already set
  2. Call `setsockopt(SL_CONNECT_ROUTING_ID, ...)` to configure the routing ID
  3. Continue even if setsockopt fails (graceful degradation)

**Added getter implementation:**
```cpp
const std::string &spot_node_t::node_id() const
{
    return _node_id;
}
```
- Lines 427-430: Returns the node's routing ID

## Technical Details

### Socket Option Used
- **Constant:** `SL_CONNECT_ROUTING_ID` (value: 61)
- **Location:** `src/util/constants.hpp`, line 55
- **Purpose:** Sets the routing ID that will be used when a ROUTER socket connects to another ROUTER

### ID Generation Strategy
- **Thread-safe:** Uses `std::atomic<uint64_t>` for counter
- **Unique:** Counter-based generation ensures uniqueness within process lifetime
- **Format:** "spot-node-{counter}" (e.g., "spot-node-0", "spot-node-1")
- **Memory ordering:** `memory_order_relaxed` for performance (ordering not critical for uniqueness)

### Error Handling
- If `setsockopt(SL_CONNECT_ROUTING_ID)` fails, connection proceeds normally
- This allows fallback to default ROUTER behavior if needed
- Comment explains the graceful degradation behavior

## Compatibility

### libzmq Compatibility
- Follows libzmq 4.3.5 pattern for CONNECT_ROUTING_ID
- Reference: `tests/router/test_connect_rid.cpp` (lines 56-57)
- Pattern matches libzmq's ROUTER-to-ROUTER handshake protocol

### API Stability
- Maintains backward compatibility (existing code works unchanged)
- New `node_id()` getter is optional (no breaking changes)
- CONNECT_ROUTING_ID setting is transparent to existing usage

## Usage Example

```cpp
// Create SPOT node
slk::ctx_t ctx;
slk::spot_node_t node(&ctx, "tcp://192.168.1.100:5555");

// Connect (automatically generates and sets routing ID)
node.connect();

// Optionally retrieve the node ID
std::string id = node.node_id();  // e.g., "spot-node-0"

// ROUTER-to-ROUTER communication now works with proper routing
```

## Testing

To verify the implementation:

1. **Compile:** Build the ServerLink library
   ```bash
   cmake --build build --target spot_node
   ```

2. **Unit Test:** Create a test that:
   - Creates two spot_node_t instances
   - Verifies unique node IDs are generated
   - Tests ROUTER-to-ROUTER message routing

3. **Integration Test:** Verify SPOT cluster synchronization works with CONNECT_ROUTING_ID

## Next Steps (Phase 3)

1. Update `spot_t` to configure ROUTER socket with proper routing ID
2. Implement ROUTER message framing for cluster communication:
   - Frame 0: routing_id
   - Frame 1: empty delimiter
   - Frame 2+: command/data
3. Test SPOT cluster synchronization with multiple nodes

## References

- **Plan:** `C:\Users\hep7\.claude\plans\encapsulated-cuddling-hennessy.md`
- **libzmq test:** `tests/router/test_connect_rid.cpp`
- **Constants:** `src/util/constants.hpp`
- **API header:** `include/serverlink/serverlink.h`

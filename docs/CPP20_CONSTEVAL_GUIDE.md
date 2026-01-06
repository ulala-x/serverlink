# C++20 consteval/constinit Developer Guide

## Quick Reference

### When to Use What

| Use Case | Solution | Example |
|----------|----------|---------|
| Compile-time constant | `inline constexpr` | `inline constexpr size_t buffer_size = 256;` |
| Compile-time function | `consteval` | `consteval size_t align(size_t n) { ... }` |
| Global initialization | `constinit` | `constinit std::atomic<int> counter{0};` |
| Macro replacement | `inline constexpr` | Replace `#define SIZE 16` with `inline constexpr size_t size = 16;` |

## consteval - Immediate Functions

### What is consteval?

`consteval` declares an **immediate function** that:
- MUST be evaluated at compile time
- Cannot be called at runtime
- Produces compile errors if it can't be evaluated at compile time

### When to Use consteval

✅ **Good Use Cases:**
```cpp
// Mathematical computations
consteval int factorial(int n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}

// Size calculations
consteval size_t buffer_slots(size_t msg_size, size_t slot_size) {
    return (msg_size + slot_size - 1) / slot_size;
}

// Validation functions
consteval bool is_valid_port(int port) {
    return port > 0 && port <= 65535;
}

// Bit manipulation
consteval uint32_t make_flags(bool a, bool b, bool c) {
    return (a ? 0x1 : 0) | (b ? 0x2 : 0) | (c ? 0x4 : 0);
}
```

❌ **Bad Use Cases:**
```cpp
// Runtime I/O - NOT possible
consteval int read_file() {
    // Error: can't do I/O at compile time
    return read_from_disk();
}

// Dynamic allocation - NOT possible
consteval void* allocate() {
    // Error: can't allocate at compile time
    return new int[100];
}
```

### consteval vs constexpr

| Feature | consteval | constexpr |
|---------|-----------|-----------|
| Compile-time only | YES | Can be runtime too |
| Must be evaluated at compile time | YES | No, can be runtime |
| Can call non-constexpr functions | NO | Only at runtime |
| Use for guarantees | YES | Use for flexibility |

**Example:**
```cpp
consteval int square_eval(int n) {
    return n * n;
}

constexpr int square_expr(int n) {
    return n * n;
}

// Usage:
constexpr int a = square_eval(5);  // ✓ OK: compile-time
constexpr int b = square_expr(5);  // ✓ OK: compile-time

int x = 5;
int c = square_eval(x);  // ✗ ERROR: must be compile-time constant
int d = square_expr(x);  // ✓ OK: evaluated at runtime
```

## inline constexpr - Type-Safe Constants

### Why Replace Macros?

**Before (C-style macros):**
```cpp
#define BUFFER_SIZE 256
#define MAX_CLIENTS 1000
```

**Problems:**
- No type safety
- No namespace
- No debugging info
- Preprocessor errors are cryptic
- Can't take address

**After (C++20 style):**
```cpp
inline constexpr size_t buffer_size = 256;
inline constexpr int max_clients = 1000;
```

**Benefits:**
- ✓ Type safe
- ✓ Scoped to namespace
- ✓ Better error messages
- ✓ Can take address
- ✓ Works with templates
- ✓ Debugger friendly

### Examples from ServerLink

#### Protocol Constants
```cpp
// Old way (enum)
enum {
    ZMTP_1_0 = 0,
    ZMTP_2_0 = 1,
    ZMTP_3_x = 3
};

// New way (inline constexpr)
inline constexpr int ZMTP_1_0 = 0;
inline constexpr int ZMTP_2_0 = 1;
inline constexpr int ZMTP_3_x = 3;
```

#### Size Constants
```cpp
// Old way (macro)
#define MESSAGE_PIPE_GRANULARITY 256
#define COMMAND_PIPE_GRANULARITY 16

// New way (inline constexpr with validation)
inline constexpr size_t message_pipe_granularity = 256;
inline constexpr size_t command_pipe_granularity = 16;

static_assert(message_pipe_granularity > 0);
static_assert(command_pipe_granularity > 0);
```

#### Error Codes
```cpp
namespace slk {
    inline constexpr int SL_EFSM = 156;
    inline constexpr int SL_ENOCOMPATPROTO = 157;
    inline constexpr int SL_ETERM = 158;
    inline constexpr int SL_EMTHREAD = 159;
}
```

## constinit - Compile-Time Initialization

### What is constinit?

`constinit` ensures a variable is initialized at compile time:
- Guarantees static initialization (no runtime cost)
- Prevents "static initialization order fiasco"
- Can be used with non-const globals

### When to Use constinit

✅ **Good Use Cases:**
```cpp
// Global atomic initialized at compile time
constinit std::atomic<int> global_counter{0};

// Configuration loaded at startup
constinit const char* config_path = "/etc/app/config.ini";

// Performance counters
constinit std::atomic<uint64_t> messages_sent{0};
constinit std::atomic<uint64_t> messages_recv{0};
```

### constinit vs constexpr

| Feature | constinit | constexpr |
|---------|-----------|-----------|
| Must be initialized at compile time | YES | YES |
| Value is const | NO | YES |
| Can modify at runtime | YES | NO |
| Use for mutable globals | YES | NO |

**Example:**
```cpp
constexpr int max_size = 1024;        // Constant, never changes
constinit int current_size = 0;       // Initialized at compile time, can change

void resize(int new_size) {
    // max_size = new_size;   // ✗ ERROR: constexpr is const
    current_size = new_size;  // ✓ OK: constinit is mutable
}
```

## Practical Examples

### Compile-Time Size Calculations

```cpp
namespace slk {

// Alignment helpers
consteval size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

consteval bool is_aligned(size_t size, size_t alignment) {
    return (size % alignment) == 0;
}

// Cache-line aligned buffer
inline constexpr size_t cache_line_size = 64;
inline constexpr size_t buffer_size = align_up(256, cache_line_size);

static_assert(is_aligned(buffer_size, cache_line_size));

}  // namespace slk
```

### Compile-Time Validation

```cpp
namespace slk {

// Protocol version validation
consteval bool is_valid_zmtp_version(int version) {
    return version == ZMTP_1_0 ||
           version == ZMTP_2_0 ||
           version == ZMTP_3_x;
}

// Port number validation
consteval bool is_valid_port(int port) {
    return port > 0 && port <= 65535;
}

// Usage in configuration
inline constexpr int default_port = 5555;
static_assert(is_valid_port(default_port), "Invalid default port");

}  // namespace slk
```

### Power-of-2 Checks

```cpp
namespace slk {

consteval bool is_power_of_2(size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

consteval size_t next_power_of_2(size_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if constexpr (sizeof(size_t) > 4) {
        n |= n >> 32;
    }
    return n + 1;
}

// Queue sizes should be power of 2 for optimal performance
inline constexpr size_t queue_size = 256;
static_assert(is_power_of_2(queue_size), "Queue size must be power of 2");

}  // namespace slk
```

### Compile-Time String Validation

```cpp
namespace slk {

consteval bool is_valid_endpoint(std::string_view endpoint) {
    if (endpoint.empty()) return false;
    if (endpoint.size() > 256) return false;

    // Check for valid transport prefix
    return endpoint.starts_with("tcp://") ||
           endpoint.starts_with("ipc://") ||
           endpoint.starts_with("inproc://");
}

// This will fail at compile time if endpoint is invalid
inline constexpr std::string_view default_endpoint = "tcp://127.0.0.1:5555";
static_assert(is_valid_endpoint(default_endpoint));

}  // namespace slk
```

## Common Patterns

### Pattern 1: Configuration Constants

```cpp
namespace slk::config {

// Network configuration
inline constexpr size_t max_message_size = 64 * 1024;  // 64 KB
inline constexpr int default_hwm = 1000;
inline constexpr int io_threads = 4;

// Timing configuration (microseconds)
inline constexpr int64_t heartbeat_interval = 1'000'000;  // 1 second
inline constexpr int64_t reconnect_interval = 100'000;   // 100 ms

// Buffer sizes (cache-aligned)
inline constexpr size_t message_buffer_size = align_up(4096, 64);
inline constexpr size_t command_buffer_size = align_up(256, 64);

static_assert(max_message_size > 0);
static_assert(default_hwm > 0);
static_assert(io_threads > 0);

}  // namespace slk::config
```

### Pattern 2: Bit Flags and Masks

```cpp
namespace slk {

// Socket option flags
inline constexpr uint32_t SOCK_DONTWAIT = 0x01;
inline constexpr uint32_t SOCK_SNDMORE  = 0x02;
inline constexpr uint32_t SOCK_PROBE    = 0x04;

// Compile-time flag combination
consteval uint32_t make_flags(bool dontwait, bool sndmore) {
    return (dontwait ? SOCK_DONTWAIT : 0) |
           (sndmore ? SOCK_SNDMORE : 0);
}

// Predefined combinations
inline constexpr uint32_t flags_normal = make_flags(false, false);
inline constexpr uint32_t flags_multipart = make_flags(false, true);
inline constexpr uint32_t flags_nonblock = make_flags(true, false);

}  // namespace slk
```

### Pattern 3: Type Traits and Metaprogramming

```cpp
namespace slk {

// Compile-time type checking
template<typename T>
consteval bool is_valid_message_type() {
    return std::is_trivially_copyable_v<T> &&
           std::is_standard_layout_v<T>;
}

template<typename T>
inline constexpr bool is_valid_message_type_v = is_valid_message_type<T>();

// Usage
struct Message {
    int id;
    char data[256];
};

static_assert(is_valid_message_type_v<Message>);

}  // namespace slk
```

## Compiler Support and Fallbacks

### Feature Detection

```cpp
#include <serverlink/config.h>

#if SL_HAVE_CONSTEVAL
    // Use consteval
    consteval int compute() { return 42; }
#else
    // Fallback to constexpr
    constexpr int compute() { return 42; }
#endif
```

### Portable Macros

```cpp
// In a common header
#if SL_HAVE_CONSTEVAL
    #define SL_CONSTEVAL consteval
    #define SL_CONSTINIT constinit
#else
    #define SL_CONSTEVAL constexpr
    #define SL_CONSTINIT constexpr
#endif

// Usage
SL_CONSTEVAL int square(int n) {
    return n * n;
}
```

## Best Practices

### DO ✓

1. **Use consteval for guaranteed compile-time evaluation:**
   ```cpp
   consteval size_t buffer_slots() { return 256; }
   ```

2. **Use inline constexpr for constants:**
   ```cpp
   inline constexpr int max_clients = 1000;
   ```

3. **Add static_assert for validation:**
   ```cpp
   static_assert(buffer_size > 0, "Buffer size must be positive");
   ```

4. **Provide fallbacks for older compilers:**
   ```cpp
   #if SL_HAVE_CONSTEVAL
   consteval auto compute() { ... }
   #else
   constexpr auto compute() { ... }
   #endif
   ```

### DON'T ✗

1. **Don't use consteval for runtime-dependent operations:**
   ```cpp
   // ✗ BAD: Can't read files at compile time
   consteval int read_config() {
       return read_from_file();
   }
   ```

2. **Don't mix macros and constexpr unnecessarily:**
   ```cpp
   // ✗ BAD: Inconsistent style
   #define SIZE1 256
   inline constexpr size_t size2 = 512;

   // ✓ GOOD: Consistent style
   inline constexpr size_t size1 = 256;
   inline constexpr size_t size2 = 512;
   ```

3. **Don't forget static_assert for important invariants:**
   ```cpp
   // ✗ BAD: No validation
   inline constexpr size_t alignment = 64;

   // ✓ GOOD: Validated at compile time
   inline constexpr size_t alignment = 64;
   static_assert(is_power_of_2(alignment));
   ```

## Performance Tips

### Compile-Time vs Runtime

```cpp
// Compile-time: zero runtime cost
consteval int factorial_ct(int n) {
    return n <= 1 ? 1 : n * factorial_ct(n - 1);
}

inline constexpr int fact_10 = factorial_ct(10);  // Computed at compile time

// This generates just: mov eax, 3628800
int get_factorial() {
    return fact_10;  // No computation at runtime
}
```

### Cache-Line Alignment

```cpp
// Align to cache line for performance
inline constexpr size_t cache_line = 64;

struct alignas(cache_line) CacheAligned {
    std::atomic<int> counter;
    // Prevent false sharing
};
```

### Compile-Time Optimization

```cpp
// Compiler can optimize this completely
consteval size_t compute_buffer_size(size_t msg_size) {
    size_t slots = (msg_size + 63) / 64;
    return slots * 64;
}

inline constexpr size_t buffer_size = compute_buffer_size(1000);
// Results in: inline constexpr size_t buffer_size = 1024;
```

## Testing consteval Functions

```cpp
// Test at compile time using static_assert
consteval int add(int a, int b) {
    return a + b;
}

static_assert(add(2, 3) == 5);
static_assert(add(0, 0) == 0);
static_assert(add(-1, 1) == 0);

// This would fail at compile time:
// static_assert(add(2, 3) == 6);  // ✗ ERROR: assertion failed
```

## Debugging Tips

### Compiler Errors

If you get a consteval error:
```
error: call to consteval function 'compute' is not a constant expression
```

Check:
1. Are all parameters compile-time constants?
2. Does the function do anything that can't be done at compile time?
3. Are you calling other consteval functions correctly?

### Viewing Computed Values

```cpp
// Technique: Force compiler to show value
template<auto V>
struct ShowValue {
    static_assert(V != V, "Value is:");  // Shows V in error message
};

consteval int compute() { return 42; }
// ShowValue<compute()> show;  // Uncomment to see value in error
```

## Summary

- **consteval:** Functions that MUST run at compile time (zero runtime cost)
- **inline constexpr:** Type-safe constants (replaces macros)
- **constinit:** Compile-time initialization for mutable globals
- **static_assert:** Compile-time validation

Use these features to:
- Eliminate runtime overhead
- Catch errors at compile time
- Improve type safety
- Enable better optimizations
- Make code more maintainable

---

For more examples, see:
- `/home/ulalax/project/ulalax/serverlink/src/util/consteval_helpers.hpp`
- `/home/ulalax/project/ulalax/serverlink/PHASE6_CONSTEVAL_SUMMARY.md`

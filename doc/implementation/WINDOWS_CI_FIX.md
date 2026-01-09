# Windows CI Build Fix

## Summary

Fixed Windows CI build failures by adding proper Windows header includes for socket types and Win32 API functions.

## Issues Fixed

### 1. `tcp_address.hpp:9` - Missing socket headers
**Error:** Cannot open include file: 'sys/socket.h'

**Root Cause:** The file used `#if !defined SL_HAVE_WINDOWS` to exclude Windows from POSIX headers, but didn't provide Windows alternatives for `sockaddr` and `socklen_t` types used in the class interface.

**Fix:**
```cpp
#if defined SL_HAVE_WINDOWS
#include "../io/windows.hpp"  // For winsock2.h and sockaddr types
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif
```

### 2. `ip_resolver.hpp` - Missing socket headers
**Error:** Similar to tcp_address.hpp - uses `sockaddr_in`, `sockaddr_in6` without proper Windows includes.

**Fix:**
```cpp
#include "address.hpp"

#if defined SL_HAVE_WINDOWS
#include "../io/windows.hpp"  // For winsock2.h and socket types
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
```

**Note:** Moved `#include "address.hpp"` before the conditional block to ensure proper header ordering.

### 3. `random.cpp:16` - Missing Windows.h
**Error:** 'GetCurrentProcessId': identifier not found

**Root Cause:** The code uses `GetCurrentProcessId()` on line 16 but only included `<unistd.h>` for non-Windows builds. Windows.h was not included.

**Fix:**
```cpp
#ifdef _WIN32
#include "../io/windows.hpp"  // For GetCurrentProcessId()
#else
#include <unistd.h>
#endif
```

## Why windows.hpp?

All three fixes use `#include "../io/windows.hpp"` instead of directly including Windows SDK headers because:

1. **Header Order Protection**: windows.hpp ensures correct include order:
   - Prevents winsock v1 inclusion (`#define _WINSOCKAPI_`)
   - Includes winsock2.h before windows.h
   - Defines necessary macros (NOMINMAX, WIN32_LEAN_AND_MEAN)

2. **Consistency**: This is the established pattern in the codebase (see precompiled.hpp)

3. **Maintainability**: Centralizes Windows platform configuration

## Files Modified

- `/home/ulalax/project/ulalax/serverlink/src/transport/tcp_address.hpp`
- `/home/ulalax/project/ulalax/serverlink/src/transport/ip_resolver.hpp`
- `/home/ulalax/project/ulalax/serverlink/src/util/random.cpp`

## Verification

### Linux Build (Control)
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
# Result: SUCCESS ✓
```

### Windows CI
The changes are now pushed to trigger Windows CI verification.

## Key Patterns

### Pattern 1: Header files using socket types
For `.hpp` files that use `sockaddr`, `socklen_t`, etc.:
```cpp
#if defined SL_HAVE_WINDOWS
#include "../io/windows.hpp"
#else
#include <sys/socket.h>
// ... other POSIX headers
#endif
```

### Pattern 2: Source files using Win32 API
For `.cpp` files using Windows-specific functions:
```cpp
#ifdef _WIN32
#include "../io/windows.hpp"
#else
#include <unistd.h>  // or other POSIX headers
#endif
```

### Pattern 3: Source files with precompiled headers
Files that `#include "precompiled.hpp"` don't need explicit windows.hpp includes as precompiled.hpp already handles it for MSVC builds.

## Related Files

The following files were checked and found to be correct:
- `src/io/ip.cpp` - Uses direct winsock2.h include (acceptable for .cpp)
- `src/transport/tcp.cpp` - Uses precompiled.hpp
- `src/transport/tcp_connecter.cpp` - Uses precompiled.hpp
- `src/transport/address.hpp` - Already has correct Windows includes

## Commit

```
commit 84ce71d
fix: Include Windows headers for socket types and GetCurrentProcessId

Fix Windows CI build errors by adding proper Windows header includes:
- tcp_address.hpp: Include windows.hpp for sockaddr/socklen_t types
- ip_resolver.hpp: Include windows.hpp for socket types (sockaddr_in, etc.)
- random.cpp: Include windows.hpp for GetCurrentProcessId() function
```

---
**Date:** 2026-01-03
**Status:** Committed and pushed to main

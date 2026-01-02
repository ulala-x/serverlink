# Windows WSAStartup Debugging Guide

## Current Status

WSAStartup initialization issue on Windows x64 where all socket-related tests fail with:
```
Assertion failed: Successful WSASTARTUP not yet performed [10093]
```

## Diagnostic Code Added

Comprehensive diagnostic output has been added to `src/io/ip.cpp` to trace the execution flow:

1. **Compile-time check**: `#pragma message` to verify `SL_BUILDING_DLL` is defined
2. **DllMain entry**: Logs when DllMain is called and the reason
3. **initialize_network()**: Logs when called and the return value
4. **do_initialize_network()**: Logs WSAStartup call and result

## Build Instructions

### Prerequisites
- Visual Studio 2022 (or compatible)
- CMake 3.14+

### Build Steps

```powershell
# Navigate to project directory
cd D:\project\ulalax\serverlink

# Configure CMake for shared library build
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=ON

# Build in Release mode
cmake --build build --config Release

# Or build in Debug mode for more detailed debugging
cmake --build build --config Debug
```

## Test Execution

### Run All Tests
```powershell
cd build
ctest -C Release --output-on-failure
```

### Run Specific Diagnostic Test
```powershell
# WSAStartup initialization test
.\build\tests\Release\test_wsastartup_order.exe

# Basic context test (first to fail)
.\build\tests\Release\test_ctx.exe
```

## Expected Diagnostic Output

If DllMain is working correctly, you should see:
```
[COMPILE TIME] DllMain will be included in this build (SL_BUILDING_DLL is defined)
[SERVERLINK] DllMain called, reason=1
[SERVERLINK] DLL_PROCESS_ATTACH: calling initialize_network()
[SERVERLINK] initialize_network() called
[SERVERLINK] do_initialize_network called
[SERVERLINK] WSAStartup returned 0
[SERVERLINK] initialization SUCCESS
[SERVERLINK] initialize_network() returning TRUE
[SERVERLINK] initialize_network() returned TRUE
```

If DllMain is NOT being called:
```
[SERVERLINK] initialize_network() called
[SERVERLINK] do_initialize_network called
[SERVERLINK] WSAStartup returned 0
[SERVERLINK] initialization SUCCESS
[SERVERLINK] initialize_network() returning TRUE
```
(DllMain messages will be missing)

## Debugging Scenarios

### Scenario 1: No DllMain Messages
**Problem**: `SL_BUILDING_DLL` is not defined during compilation
**Solution**:
1. Check CMakeLists.txt line 179: `target_compile_definitions(serverlink PRIVATE SL_BUILDING_DLL)`
2. Verify BUILD_SHARED_LIBS=ON in cmake command
3. Check build output for the pragma message

### Scenario 2: DllMain Called but WSAStartup Fails
**Problem**: WSAStartup returns non-zero error code
**Solution**:
1. Check the error code from diagnostic output
2. Common errors:
   - WSASYSNOTREADY (10091): Network subsystem not ready
   - WSAVERNOTSUPPORTED (10092): Requested version not supported
   - WSAEINPROGRESS (10036): Blocking operation in progress

### Scenario 3: DllMain Called After First Socket Use
**Problem**: Timing issue - socket created before DLL_PROCESS_ATTACH
**Solution**: This shouldn't happen as DllMain runs before global constructors

### Scenario 4: Multiple WSAStartup Calls
**Problem**: Function-local static not working correctly
**Solution**: Check that only ONE call to do_initialize_network() occurs

## Verification Steps

1. **Compile-time verification**:
   ```
   Look for: [COMPILE TIME] DllMain will be included in this build
   ```

2. **Runtime verification**:
   ```powershell
   # Check that serverlink.dll is actually a DLL
   dumpbin /headers build\Release\serverlink.dll | findstr "DLL"
   ```

3. **Symbol verification**:
   ```powershell
   # Verify DllMain is exported
   dumpbin /exports build\Release\serverlink.dll | findstr "DllMain"
   ```

## Clean Build

If you need to start fresh:
```powershell
# Remove build directory
Remove-Item -Recurse -Force build

# Reconfigure and build
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=ON
cmake --build build --config Release
```

## Alternative: Static Library Build

To test if the issue is DLL-specific:
```powershell
cmake -B build-static -S . -G "Visual Studio 17 2022" -A x64 -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=ON
cmake --build build-static --config Release
cd build-static
ctest -C Release --output-on-failure
```

In static build, DllMain won't be used. WSAStartup will be called on first socket operation via function-local static.

## Next Steps After Testing

1. **If DllMain is called and WSAStartup succeeds**: The diagnostic output will help identify timing issues
2. **If DllMain is NOT called**: Need to investigate CMake configuration or DLL loading
3. **If WSAStartup fails**: Need to investigate Windows networking setup

## Removing Diagnostic Code

Once the issue is identified and fixed, remove diagnostic fprintf statements:
1. Remove all `fprintf(stderr, ...)` and `fflush(stderr)` calls
2. Keep the `#pragma message` or remove if desired
3. Rebuild and verify tests still pass

## Contact

Document created: 2026-01-03
Last updated: 2026-01-03

# IOCP Duplicate Socket Registration Bug Analysis

## Problem Summary

Tests are failing with duplicate socket registration error:
```
[IOCP] add_fd ENTRY: fd=316
[IOCP] CreateIoCompletionPort: rc=0000000000000000, error=87
Assertion failed: (iocp.cpp:206)
```

Error 87 = ERROR_INVALID_PARAMETER means attempting to register a socket with IOCP that's already associated with an IOCP completion port.

## Evidence from Logs

From test_router_handover failure:
```
[IOCP] add_fd ENTRY: fd=316, events=000001D6DE43ED90
[IOCP] CreateIoCompletionPort: rc=0000000000000108, error=0  ← Success, IOCP handle 0x108

[IOCP] add_fd ENTRY: fd=316, events=000001D6DE43ED90  ← DUPLICATE!
[IOCP] CreateIoCompletionPort: rc=00000000000000F8, error=0  ← Success, DIFFERENT handle 0xF8

[IOCP] add_fd ENTRY: fd=420, events=...
[IOCP] CreateIoCompletionPort: rc=0000000000000124, error=0

[IOCP] add_fd ENTRY: fd=420, events=...  ← DUPLICATE!
[IOCP] CreateIoCompletionPort: rc=0000000000000000, error=87  ← FAIL!
```

## Key Observations

1. **Same fd registered twice**: Socket fd=316 has `add_fd()` called twice
2. **Same events pointer**: Both calls use same events pointer (same tcp_connecter object)
3. **Different IOCP handles**: First returns 0x108, second returns 0xF8
4. **Inconsistent behavior**: First duplicate (fd=316) succeeds with different handle, second duplicate (fd=420) fails

## Analysis

### Hypothesis 1: Multiple IOCP Instances
Different IOCP handles (0x108 vs 0xF8) suggest two different IOCP completion ports. This happens when:
- Multiple io_threads exist (each has its own IOCP)
- Socket is registered with IOCP A, then registration attempted with IOCP B

### Hypothesis 2: tcp_connecter Calling add_fd Twice
File: `src/transport/tcp_connecter.cpp` line 110-149

```cpp
void tcp_connecter_t::start_connecting()
{
    int rc = open();

    if (rc == 0) {
        _handle = add_fd(_s);  // Line 117
        out_event();
    }
    else if (rc == -1 && errno == EINPROGRESS) {
        _handle = add_fd(_s);  // Line 123
        ...
    }
}
```

These two add_fd calls are in mutually exclusive branches, so they shouldn't both execute for the same socket.

### Hypothesis 3: Connection Retry Without Cleanup
If `start_connecting()` is called multiple times without properly cleaning up the previous attempt:
1. First call: `add_fd(socket)` → registered with IOCP A
2. Socket not removed via `rm_fd()`
3. Second call: `add_fd(socket)` → tries to register again → ERROR!

Potential code paths:
- Reconnection after timeout (line 172: `stream_connecter_base_t::timer_event()` → `start_connecting()`)
- Connection handover scenario (test_router_handover creates/closes/recreates connections)

## Fix Implementation

Added duplicate detection in `src/io/iocp.cpp` line 179-194:

```cpp
iocp_t::handle_t iocp_t::add_fd (fd_t fd_, i_poll_events *events_)
{
    check_thread ();

    // Check if this fd is already registered
    for (iocp_entry_t *existing : _entries) {
        if (existing && existing->fd == fd_) {
            fprintf(stderr, "[IOCP] add_fd ERROR: fd=%llu already registered with entry=%p\n",
                    (uint64_t)fd_, existing);
            slk_assert (false && "Attempting to register same socket with IOCP twice");
            return nullptr;
        }
    }

    // Create new entry and register...
}
```

**This fix detects the bug but doesn't prevent it.** It will catch duplicate registrations within the SAME IOCP instance, but won't catch cross-IOCP duplicates.

## Root Cause Investigation Needed

1. **Check if rm_handle() is called before re-registration**
   - File: `src/transport/tcp_connecter.cpp`
   - Look for connection retry paths

2. **Verify socket cleanup in connection handover**
   - Test: `tests/router/test_router_handover.cpp`
   - Check if socket is properly closed before creating new one

3. **Check for io_thread migration**
   - File: `src/io/io_object.cpp` line 27-34 (`unplug()`)
   - Verify if objects are moved between threads without proper cleanup

## Recommended Fix

**Option 1: Prevent duplicate registration**
- Ensure `rm_handle()` or `rm_fd()` is called before any `add_fd()`
- Add cleanup in connection retry paths

**Option 2: Handle duplicate registration gracefully**
- If fd already registered with THIS IOCP → return existing handle
- If fd already registered with DIFFERENT IOCP → remove from old IOCP first

**Option 3: Track socket ownership**
- Mark socket as "registered with IOCP" at socket level
- Check before attempting registration

## Status

- [x] Bug identified and analyzed
- [x] Duplicate detection added (asserts on duplicate)
- [ ] Need to rebuild DLL to test detection
- [ ] Root cause still under investigation
- [ ] Proper fix not yet implemented

## Next Steps

1. Rebuild serverlink.dll with duplicate detection code
2. Run test_router_handover to get better error message showing WHERE duplicate occurs
3. Investigate the specific code path that triggers duplicate registration
4. Implement proper fix based on root cause

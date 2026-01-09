# Phase 4: Reaper Mailbox IOCP Integration - Fix Implementation

## Problem Identified

**Test Hang**: `test_ctx_socket` hangs after 15 seconds waiting for IOCP completions.

**Root Cause Analysis** (from debug logs):
```
Line 65-66: I/O thread signaler: set_iocp() called → _iocp set ✅
Line 100-101: Reaper signaler: _iocp=NULL → Uses socket path ❌
Line 104-105: Socket signaler: _iocp=NULL → Uses socket path ❌
```

**The Problem**:
- I/O thread's mailbox signaler was correctly configured with IOCP (`set_iocp()` called)
- **Reaper's mailbox signaler was NOT configured with IOCP**
- When reaper sends commands via `mailbox.send()`:
  - Signaler checks `_iocp` → finds NULL
  - Falls back to socket-based signaling (write to socketpair)
  - IOCP loop does NOT monitor socketpair → **hang**

## Solution

Apply the same IOCP configuration pattern to Reaper that io_thread uses:
1. Configure mailbox signaler with `set_iocp()`
2. Register reaper as mailbox handler with `set_mailbox_handler()`
3. Use `adjust_mailbox_load()` instead of `add_fd()`
4. Update cleanup paths in `process_stop()` and `process_reaped()`

## Implementation Details

### File Modified: `src/io/reaper.cpp`

#### 1. Added IOCP Header Include
```cpp
#ifdef SL_USE_IOCP
#include "iocp.hpp"
#endif
```

#### 2. Constructor - IOCP Mailbox Configuration
**Before** (lines 22-25):
```cpp
if (_mailbox.get_fd () != retired_fd) {
    _mailbox_handle = _poller->add_fd (_mailbox.get_fd (), this);
    _poller->set_pollin (_mailbox_handle);
}
```

**After** (lines 26-59):
```cpp
#ifdef SL_USE_IOCP
    // For IOCP, configure mailbox signaler to use PostQueuedCompletionStatus
    // for wakeup instead of socket-based signaling (same as io_thread)
    iocp_t *iocp_poller = static_cast<iocp_t *> (_poller);

    signaler_t *signaler = _mailbox.get_signaler ();
    if (signaler) {
        signaler->set_iocp (iocp_poller);
    }

    // Register this reaper as the mailbox handler for SIGNALER_KEY events
    iocp_poller->set_mailbox_handler (this);

    // Don't register mailbox fd with IOCP - we use PostQueuedCompletionStatus instead
    // However, we still need to increment load count for the mailbox
    iocp_poller->adjust_mailbox_load (1);
#else
    // For non-IOCP pollers (epoll, kqueue, select), register mailbox fd
    if (_mailbox.get_fd () != retired_fd) {
        _mailbox_handle = _poller->add_fd (_mailbox.get_fd (), this);
        _poller->set_pollin (_mailbox_handle);
    }
#endif
```

#### 3. process_stop() - IOCP Cleanup
**Before** (lines 85-89):
```cpp
if (_sockets == 0) {
    send_done ();
    _poller->rm_fd (_mailbox_handle);
    _poller->stop ();
}
```

**After** (lines 119-130):
```cpp
if (_sockets == 0) {
    send_done ();
#ifdef SL_USE_IOCP
    // For IOCP, we don't register mailbox fd, so nothing to remove
    // However, we need to decrement load count to match the increment in constructor
    iocp_t *iocp_poller = static_cast<iocp_t *> (_poller);
    iocp_poller->adjust_mailbox_load (-1);
#else
    _poller->rm_fd (_mailbox_handle);
#endif
    _poller->stop ();
}
```

#### 4. process_reaped() - IOCP Cleanup
**Before** (lines 106-109):
```cpp
if (!_sockets && _terminating) {
    send_done ();
    _poller->rm_fd (_mailbox_handle);
    _poller->stop ();
}
```

**After** (lines 147-158):
```cpp
if (!_sockets && _terminating) {
    send_done ();
#ifdef SL_USE_IOCP
    // For IOCP, we don't register mailbox fd, so nothing to remove
    // However, we need to decrement load count to match the increment in constructor
    iocp_t *iocp_poller = static_cast<iocp_t *> (_poller);
    iocp_poller->adjust_mailbox_load (-1);
#else
    _poller->rm_fd (_mailbox_handle);
#endif
    _poller->stop ();
}
```

## Key Design Points

### 1. Each Thread Has Separate IOCP Instance

- **Reaper**: Creates its own `poller_t` (iocp_t) instance
- **Each I/O Thread**: Creates its own `poller_t` (iocp_t) instance
- Each IOCP instance has its own:
  - Completion port handle (`_iocp`)
  - Mailbox handler (`_mailbox_handler`)
  - SIGNALER_KEY processing

**No conflicts** between reaper and io_thread mailbox handlers!

### 2. SIGNALER_KEY Flow (Per Thread)

**Reaper Thread**:
1. Some thread calls `reaper->send_stop()` → `mailbox.send()`
2. Mailbox detects pipe not flushed → calls `signaler.send()`
3. Signaler sees `_iocp` set → calls `iocp->send_signal()`
4. IOCP posts SIGNALER_KEY to **reaper's completion port**
5. Reaper's IOCP loop receives SIGNALER_KEY
6. Calls `_mailbox_handler->in_event()` (reaper)
7. Reaper processes commands

**I/O Thread** (same pattern):
1. Command sent to io_thread mailbox
2. Signaler posts SIGNALER_KEY to **io_thread's completion port**
3. I/O thread's IOCP loop processes SIGNALER_KEY
4. Calls io_thread's `in_event()`

### 3. Load Counting

Both reaper and io_thread now use `adjust_mailbox_load()`:
- **Constructor**: `adjust_mailbox_load(1)` - increment load
- **Cleanup**: `adjust_mailbox_load(-1)` - decrement load

This ensures IOCP loop doesn't exit prematurely when load reaches 0.

### 4. Platform Consistency

**IOCP (Windows)**:
- Mailbox fd NOT registered with completion port
- Uses PostQueuedCompletionStatus for wakeup
- Same pattern for reaper and io_thread

**Non-IOCP (Linux/macOS)**:
- Mailbox fd registered with poller (epoll/kqueue/select)
- Uses socket signaling for wakeup
- Same pattern for reaper and io_thread

## Expected Behavior After Fix

### Normal Operation:
1. **Reaper signaler**: `_iocp` is set during constructor ✅
2. **I/O thread signaler**: `_iocp` is set during constructor ✅
3. **Socket signaler**: `_iocp` remains NULL (sockets use add_fd path) ✅

### Debug Log (Expected):
```
[reaper] IOCP mode: configuring mailbox signaler
[reaper] Calling signaler->set_iocp()
[signaler_t::set_iocp] ENTER: this=XXX, iocp=YYY
[signaler_t::set_iocp] EXIT: _iocp=YYY

[mailbox_t::send] Pipe flush returned false - calling signaler.send()
[signaler_t::send] this=XXX, _iocp=YYY
[signaler_t::send] Using IOCP path - calling send_signal()  ← CORRECT!
[iocp_t::send_signal] PostQueuedCompletionStatus result: rc=1, error=0

[IOCP] SIGNALER_KEY received! _mailbox_handler=ZZZ
[IOCP] Calling _mailbox_handler->in_event()
```

## Testing

### Build and Run
```batch
build_and_test_debug.bat
```

### Expected Result
- ✅ test_ctx_create_destroy: PASSED
- ✅ test_ctx_socket: PASSED (no more 15s timeout!)
- All debug logs show IOCP path being used for both reaper and io_thread

### Verification Points
1. Reaper signaler shows `_iocp` is set (not NULL)
2. Reaper `mailbox.send()` uses IOCP path
3. PostQueuedCompletionStatus succeeds (rc=1)
4. SIGNALER_KEY events are received by IOCP loop
5. Test completes without timeout

## Files Modified

1. `src/io/reaper.cpp` - Added IOCP mailbox configuration (mirrors io_thread pattern)

## Related Documentation

- `IOCP_SIGNALER_DEBUG_TRACE.md` - Debug logging implementation
- `PHASE7_IOCP_SIGNALER_IMPLEMENTATION.md` - Original signaler design (io_thread only)

## Next Steps

1. Build and test to verify fix works
2. If successful, remove debug logging (or conditionalize with DEBUG flag)
3. Run full test suite to ensure no regressions
4. Document final IOCP implementation status

---
**Issue Identified**: 2026-01-05
**Fix Implemented**: 2026-01-05
**Status**: ✅ Fix Complete - Ready for Testing

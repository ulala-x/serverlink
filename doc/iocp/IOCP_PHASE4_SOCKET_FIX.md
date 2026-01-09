# Phase 4 Final: Socket Mailbox IOCP Integration

## Problem Identified

After fixing Reaper mailbox IOCP configuration, Socket mailbox signaler still had issues:

```
[signaler_t::send] this=0000025A37A0B7A8, _iocp=0000000000000000
[signaler_t::send] IOCP not set - using socket path  ❌
```

**Root Cause**: Socket mailbox signaler was not configured with IOCP, causing it to fall back to socket-based signaling which IOCP doesn't monitor.

## Solution

Configure socket mailbox signalers with IOCP during socket construction by:
1. Adding `get_io_thread_by_tid()` helper function to ctx
2. Using it to retrieve the I/O thread the socket belongs to
3. Getting the IOCP poller from that I/O thread
4. Configuring the socket's mailbox signaler with IOCP

## Implementation Details

### 1. Added ctx_t::get_io_thread_by_tid() (ctx.hpp + ctx.cpp)

**Purpose**: Get the I/O thread for a specific thread ID

**Header Declaration** (ctx.hpp):
```cpp
// Returns the I/O thread for a given thread ID
// Returns NULL if tid does not correspond to an I/O thread
slk::io_thread_t *get_io_thread_by_tid (uint32_t tid_) const;
```

**Implementation** (ctx.cpp):
```cpp
slk::io_thread_t *slk::ctx_t::get_io_thread_by_tid (uint32_t tid_) const
{
    // I/O threads start at tid = reaper_tid + 1
    // _io_threads[0] corresponds to tid = reaper_tid + 1
    if (tid_ <= reaper_tid)
        return NULL;

    const uint32_t io_thread_index = tid_ - reaper_tid - 1;
    if (io_thread_index >= _io_threads.size ())
        return NULL;

    return _io_threads[io_thread_index];
}
```

**TID Mapping**:
- `term_tid = 0` → Not an I/O thread
- `reaper_tid = 1` → Not an I/O thread
- `tid = 2` → `_io_threads[0]` (first I/O thread)
- `tid = 3` → `_io_threads[1]` (second I/O thread)
- ...

### 2. Socket Mailbox IOCP Configuration (socket_base.cpp)

**Added IOCP Header**:
```cpp
#ifdef SL_USE_IOCP
#include "../io/iocp.hpp"
#endif
```

**Constructor - After Mailbox Creation**:
```cpp
#ifdef SL_USE_IOCP
        // For IOCP, configure mailbox signaler to use PostQueuedCompletionStatus
        // Get the I/O thread this socket belongs to
        io_thread_t *io_thread = parent_->get_io_thread_by_tid (tid_);
        if (io_thread && _mailbox) {
            poller_t *poller = io_thread->get_poller ();
            iocp_t *iocp_poller = static_cast<iocp_t *> (poller);

            signaler_t *signaler = m->get_signaler ();
            if (signaler) {
                signaler->set_iocp (iocp_poller);
            }
        }
#endif
```

**Flow**:
1. Socket is created with `tid_` (thread ID it belongs to)
2. Get I/O thread for this tid using `parent_->get_io_thread_by_tid(tid_)`
3. Get IOCP poller from I/O thread
4. Configure mailbox signaler with IOCP pointer
5. Now `signaler.send()` uses PostQueuedCompletionStatus!

## Key Design Points

### 1. Socket-to-Thread Mapping

Sockets inherit from `object_t` which has a `tid_`:
- Sockets are created with a specific thread ID
- This tid corresponds to an I/O thread (tid >= 2)
- Socket processes commands in the context of its I/O thread

### 2. Cross-Thread Communication

Socket mailbox is used for **cross-thread communication**:
- User thread sends command to socket via `socket->mailbox.send()`
- Socket's I/O thread receives command via `mailbox.recv()` in `process_commands()`
- Signaler wakes up the socket's I/O thread when command arrives

### 3. IOCP Signaler Flow

**Before Fix**:
```
User thread: mailbox.send() → signaler.send()
  → _iocp is NULL → socket write → IOCP doesn't monitor → HANG
```

**After Fix**:
```
User thread: mailbox.send() → signaler.send()
  → _iocp is set → PostQueuedCompletionStatus(SIGNALER_KEY)
  → Socket's I/O thread IOCP loop receives SIGNALER_KEY
  → Calls in_event() → process_commands() → SUCCESS
```

## Complete IOCP Mailbox Integration

All three mailbox types now configured with IOCP:

### 1. Reaper Mailbox (reaper.cpp)
```cpp
signaler->set_iocp(iocp_poller);
iocp_poller->set_mailbox_handler(this);
iocp_poller->adjust_mailbox_load(1);
```

### 2. I/O Thread Mailbox (io_thread.cpp)
```cpp
signaler->set_iocp(iocp_poller);
iocp_poller->set_mailbox_handler(this);
iocp_poller->adjust_mailbox_load(1);
```

### 3. Socket Mailbox (socket_base.cpp) - NEW!
```cpp
io_thread = parent_->get_io_thread_by_tid(tid_);
iocp_poller = io_thread->get_poller();
signaler->set_iocp(iocp_poller);
// Note: No set_mailbox_handler() or adjust_mailbox_load() needed!
// Socket's I/O thread already has its own mailbox handler set
```

**Important Difference**: Socket mailbox doesn't need `set_mailbox_handler()` because:
- The socket's I/O thread already has its mailbox handler registered
- Socket mailbox signaler just posts to the I/O thread's completion port
- The I/O thread's IOCP loop already handles SIGNALER_KEY events

## Expected Behavior

### Debug Log (Expected):
```
[socket_base_t] IOCP mode: configuring mailbox signaler (tid=2)
[socket_base_t] Found io_thread=XXX for tid=2
[socket_base_t] Got poller=YYY, iocp_poller=YYY
[socket_base_t] _mailbox.get_signaler() returned ZZZ
[socket_base_t] Calling signaler->set_iocp()
[signaler_t::set_iocp] _iocp=YYY  ✅
[socket_base_t] Socket mailbox IOCP configuration complete

... later when command is sent ...

[mailbox_t::send] Pipe flush returned false - calling signaler.send()
[signaler_t::send] this=ZZZ, _iocp=YYY
[signaler_t::send] Using IOCP path - calling send_signal()  ✅
[iocp_t::send_signal] PostQueuedCompletionStatus result: rc=1
```

## Files Modified

1. `src/core/ctx.hpp` - Added `get_io_thread_by_tid()` declaration
2. `src/core/ctx.cpp` - Implemented `get_io_thread_by_tid()`
3. `src/core/socket_base.cpp` - Added IOCP mailbox configuration in constructor

## Testing

```batch
build_and_test_debug.bat
```

**Expected Result**:
- ✅ All three signaler types (Reaper, I/O Thread, Socket) use IOCP path
- ✅ No more "IOCP not set - using socket path" errors
- ✅ test_ctx_socket passes without timeout
- ✅ All tests pass

## Related Documentation

- `IOCP_PHASE4_REAPER_FIX.md` - Reaper mailbox fix
- `IOCP_SIGNALER_DEBUG_TRACE.md` - Debug logging
- `PHASE7_IOCP_SIGNALER_IMPLEMENTATION.md` - Original I/O thread design

---
**Issue Identified**: 2026-01-05
**Fix Implemented**: 2026-01-05
**Status**: ✅ Complete - Ready for Testing

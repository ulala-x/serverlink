# IOCP Signaler Debug Trace Implementation

## Problem

test_ctx_socket hangs after 15 seconds with IOCP. Log shows:
- Socket creation completed
- IOCP loop started
- Reaper completion received
- **Hung while waiting for I/O thread mailbox events**

## Debug Logging Added

### 1. signaler_t::set_iocp() (signaler.cpp)
```cpp
[signaler_t::set_iocp] ENTER: this=%p, iocp=%p
[signaler_t::set_iocp] EXIT: _iocp=%p
```
**Purpose**: Verify IOCP pointer is being set correctly

### 2. signaler_t::send() (signaler.cpp)
```cpp
[signaler_t::send] this=%p, _iocp=%p
[signaler_t::send] Using IOCP path - calling send_signal()
[signaler_t::send] send_signal() returned
// OR
[signaler_t::send] IOCP not set - using socket path
```
**Purpose**: Verify which signaling path is taken (IOCP vs socket)

### 3. iocp_t::send_signal() (iocp.cpp)
```cpp
[iocp_t::send_signal] ENTER: this=%p, _iocp=%p
[iocp_t::send_signal] PostQueuedCompletionStatus result: rc=%d, error=%lu
[iocp_t::send_signal] EXIT
```
**Purpose**: Verify PostQueuedCompletionStatus is called and succeeds

### 4. iocp_t::set_mailbox_handler() (iocp.cpp)
```cpp
[iocp_t::set_mailbox_handler] this=%p, handler=%p
[iocp_t::set_mailbox_handler] _mailbox_handler set to %p
```
**Purpose**: Verify mailbox handler is registered

### 5. IOCP loop SIGNALER_KEY handling (iocp.cpp)
```cpp
[IOCP] SIGNALER_KEY received! _mailbox_handler=%p
[IOCP] Calling _mailbox_handler->in_event()
[IOCP] _mailbox_handler->in_event() returned
// OR
[IOCP] WARNING: SIGNALER_KEY received but _mailbox_handler is NULL!
```
**Purpose**: Verify SIGNALER_KEY events are received and handled

### 6. io_thread constructor (io_thread.cpp)
```cpp
[io_thread] IOCP mode: configuring mailbox signaler
[io_thread] iocp_poller=%p, this=%p
[io_thread] _mailbox.get_signaler() returned %p
[io_thread] Calling signaler->set_iocp()
[io_thread] signaler->set_iocp() completed
[io_thread] Calling iocp_poller->set_mailbox_handler(this)
[io_thread] Calling adjust_mailbox_load(1)
[io_thread] IOCP mailbox configuration complete
```
**Purpose**: Verify io_thread IOCP initialization sequence

### 7. mailbox_t::send() (mailbox.cpp)
```cpp
[mailbox_t::send] Pipe flush returned false - calling signaler.send()
[mailbox_t::send] signaler.send() completed
// OR
[mailbox_t::send] Pipe flush returned true - NOT calling signaler.send()
```
**Purpose**: Verify when signaler.send() is actually called

## Expected Flow

### Normal Operation:
1. io_thread constructor calls set_iocp() on signaler
2. io_thread constructor calls set_mailbox_handler()
3. Some thread sends command via mailbox.send()
4. mailbox detects pipe not flushed → calls signaler.send()
5. signaler.send() sees _iocp set → calls send_signal()
6. send_signal() calls PostQueuedCompletionStatus(SIGNALER_KEY)
7. IOCP loop receives SIGNALER_KEY completion
8. IOCP loop calls _mailbox_handler->in_event()
9. io_thread processes commands

### Potential Issues to Detect:

**Issue A: Signaler not configured**
- Log shows: `[signaler_t::send] IOCP not set - using socket path`
- Root cause: set_iocp() not called or failed
- Fix: Verify io_thread constructor IOCP path

**Issue B: Mailbox handler not registered**
- Log shows: `[IOCP] SIGNALER_KEY received but _mailbox_handler is NULL!`
- Root cause: set_mailbox_handler() not called
- Fix: Verify io_thread constructor sets handler

**Issue C: Signaler never called**
- Log shows: `[mailbox_t::send] Pipe flush returned true - NOT calling signaler.send()`
- Root cause: ypipe flush succeeds, no signal needed
- Fix: This is actually correct behavior if pipe already active

**Issue D: PostQueuedCompletionStatus fails**
- Log shows: `[iocp_t::send_signal] PostQueuedCompletionStatus result: rc=0, error=...`
- Root cause: Invalid IOCP handle or completion port closed
- Fix: Verify IOCP initialization

**Issue E: SIGNALER_KEY never received**
- Log shows IOCP waiting but no SIGNALER_KEY line
- Root cause: PostQueuedCompletionStatus not posting to correct IOCP
- Fix: Verify _iocp handle is correct completion port

## Build and Test

```batch
build_and_test_debug.bat
```

This will:
1. Build with debug logging
2. Run test_ctx_socket
3. Display complete log output
4. Show exit code

## Files Modified

1. `src/io/signaler.hpp` - No changes (already has set_iocp declaration)
2. `src/io/signaler.cpp` - Added logging to set_iocp() and send()
3. `src/io/iocp.cpp` - Added logging to send_signal(), set_mailbox_handler(), and SIGNALER_KEY handling
4. `src/io/io_thread.cpp` - Added logging to IOCP configuration sequence
5. `src/io/mailbox.cpp` - Added logging to send() to track when signaler is called

## Next Steps

1. Run `build_and_test_debug.bat`
2. Analyze log output to identify which step fails
3. Based on failure point, implement targeted fix
4. Re-test to verify fix

---
**Created**: 2026-01-05
**Status**: Debug logging implemented, ready for testing

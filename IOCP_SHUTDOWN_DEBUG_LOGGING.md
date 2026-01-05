# IOCP Shutdown Debug Logging

## Problem

Context destruction hangs with IOCP loop continuing to run:
```
[TEST] test_ctx_socket: destroying context
[IOCP] Waiting for completions, timeout=0
(hang - loop continues running)
```

## Debug Logging Added

### 1. IOCP Loop Entry/Exit (iocp.cpp)

**Loop Entry:**
```cpp
fprintf(stderr, "[IOCP] loop() ENTER: load=%d, _stopping=%d\n", get_load(), _stopping);
```

**Loop Iteration:**
```cpp
fprintf(stderr, "[IOCP] loop iteration: _stopping=%d, load=%d\n", _stopping, get_load());
```

**Load Check:**
```cpp
if (get_load () == 0) {
    fprintf(stderr, "[IOCP] load=0, timeout=%llu - checking exit condition\n", timeout);
    if (timeout == 0) {
        fprintf(stderr, "[IOCP] load=0 and timeout=0 - breaking loop\n");
        break;
    }
    fprintf(stderr, "[IOCP] load=0 but timeout=%llu - continuing\n", timeout);
    continue;
}
```

**Loop Exit:**
```cpp
fprintf(stderr, "[IOCP] loop() EXIT: _stopping=%d, load=%d\n", _stopping, get_load());
// ... cleanup ...
fprintf(stderr, "[IOCP] loop() COMPLETE\n");
```

### 2. Completion Packet Processing (iocp.cpp)

**Each Packet:**
```cpp
fprintf(stderr, "[IOCP] Processing completion packet %lu/%lu: key=%p\n",
        i + 1, count, (void*)entry.lpCompletionKey);
```

**SHUTDOWN_KEY:**
```cpp
if (entry.lpCompletionKey == SHUTDOWN_KEY) {
    fprintf(stderr, "[IOCP] SHUTDOWN_KEY received! Setting _stopping=true and breaking\n");
    _stopping = true;
    break;
}
```

### 3. I/O Thread Stop (io_thread.cpp)

**process_stop() Entry:**
```cpp
fprintf(stderr, "[io_thread::process_stop] ENTER: this=%p\n", this);
```

**Load Adjustment:**
```cpp
fprintf(stderr, "[io_thread::process_stop] IOCP mode: adjusting mailbox load -1\n");
iocp_t *iocp_poller = static_cast<iocp_t *> (_poller);
iocp_poller->adjust_mailbox_load (-1);
fprintf(stderr, "[io_thread::process_stop] Load after adjustment: %d\n", _poller->get_load());
```

**Calling stop():**
```cpp
fprintf(stderr, "[io_thread::process_stop] Calling _poller->stop()\n");
_poller->stop ();
fprintf(stderr, "[io_thread::process_stop] EXIT\n");
```

### 4. Reaper Stop (reaper.cpp)

**process_stop() Entry:**
```cpp
fprintf(stderr, "[reaper::process_stop] ENTER: _sockets=%d, _terminating=%d\n", _sockets, _terminating);
```

**If No Sockets:**
```cpp
if (_sockets == 0) {
    fprintf(stderr, "[reaper::process_stop] No sockets pending - calling send_done()\n");
    send_done ();
    // ... IOCP adjustments ...
    fprintf(stderr, "[reaper::process_stop] Calling _poller->stop()\n");
    _poller->stop ();
    fprintf(stderr, "[reaper::process_stop] EXIT (stopped)\n");
} else {
    fprintf(stderr, "[reaper::process_stop] EXIT (waiting for %d sockets)\n", _sockets);
}
```

## Expected Flow (Normal Shutdown)

### Context Destruction Sequence:

1. **User calls slk_close()**
2. **Context termination initiated**
3. **Reaper receives stop command:**
   ```
   [reaper::process_stop] ENTER: _sockets=0, _terminating=0
   [reaper::process_stop] No sockets pending - calling send_done()
   [reaper::process_stop] IOCP mode: adjusting mailbox load -1
   [reaper::process_stop] Load after adjustment: 0
   [reaper::process_stop] Calling _poller->stop()
   [IOCP] stop() called, posting SHUTDOWN_KEY
   [IOCP] SHUTDOWN_KEY posted: rc=1
   ```

4. **Reaper IOCP loop receives SHUTDOWN_KEY:**
   ```
   [IOCP] Received 1 completions (ok=1, error=0)
   [IOCP] Processing completion packet 1/1: key=0xDEADBEEF
   [IOCP] SHUTDOWN_KEY received! Setting _stopping=true and breaking
   [IOCP] loop() EXIT: _stopping=1, load=0
   [IOCP] loop() COMPLETE
   ```

5. **I/O threads receive stop command:**
   ```
   [io_thread::process_stop] ENTER: this=XXX
   [io_thread::process_stop] IOCP mode: adjusting mailbox load -1
   [io_thread::process_stop] Load after adjustment: 0
   [io_thread::process_stop] Calling _poller->stop()
   [IOCP] stop() called, posting SHUTDOWN_KEY
   [IOCP] SHUTDOWN_KEY posted: rc=1
   ```

6. **I/O thread IOCP loops receive SHUTDOWN_KEY:**
   ```
   [IOCP] Received 1 completions (ok=1, error=0)
   [IOCP] Processing completion packet 1/1: key=0xDEADBEEF
   [IOCP] SHUTDOWN_KEY received! Setting _stopping=true and breaking
   [IOCP] loop() EXIT: _stopping=1, load=0
   [IOCP] loop() COMPLETE
   ```

## Potential Issues to Diagnose

### Issue 1: stop() Not Called
**Symptom:**
```
[IOCP] Waiting for completions, timeout=0
(no stop() log message)
```
**Cause**: process_stop() not being called
**Fix**: Check command delivery to thread

### Issue 2: SHUTDOWN_KEY Not Posted
**Symptom:**
```
[reaper::process_stop] Calling _poller->stop()
[IOCP] stop() called, posting SHUTDOWN_KEY
[IOCP] SHUTDOWN_KEY posted: rc=0  ← FAILED!
```
**Cause**: PostQueuedCompletionStatus failed
**Fix**: Check IOCP handle validity

### Issue 3: SHUTDOWN_KEY Not Received
**Symptom:**
```
[IOCP] stop() called, posting SHUTDOWN_KEY
[IOCP] SHUTDOWN_KEY posted: rc=1
[IOCP] Waiting for completions, timeout=0
(no SHUTDOWN_KEY received message)
```
**Cause**: GetQueuedCompletionStatusEx not receiving packet
**Fix**: Check IOCP handle, check for infinite timeout blocking

### Issue 4: Load Not Zero
**Symptom:**
```
[io_thread::process_stop] Load after adjustment: 1  ← Should be 0!
[IOCP] loop iteration: _stopping=0, load=1
[IOCP] Waiting for completions, timeout=0
```
**Cause**: Load count mismatch (increment without decrement)
**Fix**: Verify all add_fd() have matching rm_fd() or adjust_load(-1)

### Issue 5: _stopping Not Set
**Symptom:**
```
[IOCP] SHUTDOWN_KEY received! Setting _stopping=true and breaking
[IOCP] loop iteration: _stopping=0, load=0  ← _stopping still 0!
```
**Cause**: _stopping flag corruption or threading issue
**Fix**: Check _stopping is properly volatile/atomic

## Diagnosis Steps

1. **Check if process_stop() is called:**
   - Look for `[reaper::process_stop]` and `[io_thread::process_stop]` logs
   - If missing: Command not delivered to thread

2. **Check if stop() is called:**
   - Look for `[IOCP] stop() called` log
   - If missing: process_stop() not calling _poller->stop()

3. **Check if SHUTDOWN_KEY is posted:**
   - Look for `[IOCP] SHUTDOWN_KEY posted: rc=1` log
   - If rc=0: PostQueuedCompletionStatus failed

4. **Check if SHUTDOWN_KEY is received:**
   - Look for `[IOCP] Processing completion packet` with key=0xDEADBEEF
   - If missing: IOCP not receiving posted packet

5. **Check load count:**
   - Look for `Load after adjustment: 0`
   - If not 0: Load count mismatch preventing loop exit

6. **Check _stopping flag:**
   - After SHUTDOWN_KEY, next iteration should show `_stopping=1`
   - If still 0: Flag not being set correctly

## Files Modified

1. `src/io/iocp.cpp` - Added loop iteration, packet processing, and shutdown logging
2. `src/io/io_thread.cpp` - Added process_stop() detailed logging
3. `src/io/reaper.cpp` - Added process_stop() detailed logging

## Testing

```batch
build_and_test_debug.bat
```

Analyze output for shutdown sequence and identify where it hangs.

---
**Created**: 2026-01-05
**Purpose**: Diagnose IOCP loop not exiting during context destruction

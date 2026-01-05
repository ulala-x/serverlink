/* SPDX-License-Identifier: MPL-2.0 */

#include "../util/config.hpp"
#if defined SL_USE_IOCP

#include "iocp.hpp"
#include "i_poll_events.hpp"
#include "../util/err.hpp"
#include "../util/macros.hpp"

#include <algorithm>

namespace slk
{
//=============================================================================
// overlapped_ex_t implementation
//=============================================================================

overlapped_ex_t::overlapped_ex_t ()
{
    // Zero-initialize WSAOVERLAPPED part
    memset (static_cast<WSAOVERLAPPED *> (this), 0, sizeof (WSAOVERLAPPED));

    type = OP_READ;
    socket = retired_fd;
    entry = nullptr;

    wsabuf.buf = reinterpret_cast<char *> (buffer);
    wsabuf.len = BUF_SIZE;

    pending.store (false, std::memory_order_relaxed);
    cancelled.store (false, std::memory_order_relaxed);
}

//=============================================================================
// iocp_error_action classification
//=============================================================================

iocp_error_action classify_error (DWORD error_)
{
    switch (error_) {
        // Success cases
        case ERROR_SUCCESS:
        case WSA_IO_PENDING:
            return iocp_error_action::IOCP_IGNORE;

        // Retry cases - temporary errors
        case WSAEWOULDBLOCK:
        case WSAEINTR:
        case WSAEINPROGRESS:
            return iocp_error_action::IOCP_RETRY;

        // Close cases - connection errors
        case WSAECONNRESET:
        case WSAECONNABORTED:
        case WSAENETRESET:
        case WSAESHUTDOWN:
        case WSAENOTCONN:
        case WSAETIMEDOUT:
        case WSAEHOSTUNREACH:
        case WSAENETUNREACH:
        case ERROR_NETNAME_DELETED:
        case ERROR_CONNECTION_ABORTED:
            return iocp_error_action::IOCP_CLOSE;

        // Fatal cases - programming errors or system failures
        case WSAENOTSOCK:
        case WSAEINVAL:
        case WSAEFAULT:
        case WSAEBADF:
        case ERROR_INVALID_HANDLE:
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return iocp_error_action::IOCP_FATAL;

        // Operation aborted (usually from CancelIoEx)
        case ERROR_OPERATION_ABORTED:
            return iocp_error_action::IOCP_CLOSE;

        // Default: treat unknown errors as close-worthy
        default:
            return iocp_error_action::IOCP_CLOSE;
    }
}

//=============================================================================
// iocp_entry_t implementation
//=============================================================================

iocp_entry_t::iocp_entry_t (fd_t fd_, i_poll_events *events_)
    : fd (fd_), events (events_)
{
    // Initialize SRW lock
    InitializeSRWLock (&lock);

    // Create OVERLAPPED structures for read and write operations
    // Simplified IOCP: BSD socket connect, IOCP I/O only
    read_ovl = std::make_unique<overlapped_ex_t> ();
    write_ovl = std::make_unique<overlapped_ex_t> ();

    read_ovl->type = overlapped_ex_t::OP_READ;
    read_ovl->socket = fd_;
    read_ovl->entry = this;

    write_ovl->type = overlapped_ex_t::OP_WRITE;
    write_ovl->socket = fd_;
    write_ovl->entry = this;

    want_pollin.store (false, std::memory_order_relaxed);
    want_pollout.store (false, std::memory_order_relaxed);
    pending_count.store (0, std::memory_order_relaxed);
    retired.store (false, std::memory_order_relaxed);
}

iocp_entry_t::~iocp_entry_t ()
{
    // OVERLAPPED structures are automatically cleaned up by unique_ptr
}

//=============================================================================
// iocp_t implementation
//=============================================================================

iocp_t::iocp_t (ctx_t *ctx_)
    : worker_poller_base_t (ctx_),
      _iocp (NULL),
      _mailbox_handler (nullptr),
      _select_entries ()
{
    // Simplified IOCP: BSD socket connect, IOCP I/O only
    // AcceptEx/ConnectEx support removed for simplicity

    // Create I/O Completion Port
    // Parameters:
    //   INVALID_HANDLE_VALUE: create new IOCP (not associating with existing handle)
    //   NULL: no existing IOCP to associate with
    //   0: completion key (unused in creation)
    //   0: number of concurrent threads (0 = number of processors)
    _iocp = CreateIoCompletionPort (INVALID_HANDLE_VALUE, NULL, 0, 0);
    win_assert (_iocp != NULL);
}

iocp_t::~iocp_t ()
{
    // Wait till the worker thread exits
    stop_worker ();

    // Clean up all entries
    for (iocp_entry_t *entry : _entries) {
        if (entry) {
            delete entry;
        }
    }
    _entries.clear ();

    for (iocp_entry_t *entry : _retired) {
        if (entry) {
            delete entry;
        }
    }
    _retired.clear ();

    // Clean up select entries
    for (select_entry_t *entry : _select_entries) {
        if (entry) {
            delete entry;
        }
    }
    _select_entries.clear ();

    // Close IOCP handle
    if (_iocp) {
        BOOL rc = CloseHandle (_iocp);
        win_assert (rc != 0);
    }
}

iocp_t::handle_t iocp_t::add_fd (fd_t fd_, i_poll_events *events_)
{
    fprintf(stderr, "[IOCP] add_fd ENTRY: fd=%llu, events=%p\n", (uint64_t)fd_, events_);

    check_thread ();

    // Check if this fd is already registered
    // This would be a programming error - each socket should only be registered once
    for (iocp_entry_t *existing : _entries) {
        if (existing && existing->fd == fd_) {
            fprintf(stderr, "[IOCP] add_fd ERROR: fd=%llu already registered with entry=%p\n",
                    (uint64_t)fd_, existing);
            slk_assert (false && "Attempting to register same socket with IOCP twice");
            return nullptr;
        }
    }

    fprintf(stderr, "[IOCP] add_fd: creating entry\n");

    // Create new entry
    iocp_entry_t *entry = new (std::nothrow) iocp_entry_t (fd_, events_);
    alloc_assert (entry);

    fprintf(stderr, "[IOCP] add_fd: entry created=%p, calling CreateIoCompletionPort\n", entry);

    // Associate socket with IOCP
    // Parameters:
    //   fd_: the socket handle to associate
    //   _iocp: the completion port to associate with
    //   (ULONG_PTR)entry: completion key (pointer to our entry)
    //   0: number of concurrent threads (ignored for existing IOCP)
    HANDLE ret =
      CreateIoCompletionPort (reinterpret_cast<HANDLE> (fd_), _iocp,
                              reinterpret_cast<ULONG_PTR> (entry), 0);

    fprintf(stderr, "[IOCP] CreateIoCompletionPort: rc=%p, error=%lu\n",
            ret, GetLastError());

    win_assert (ret == _iocp);

    fprintf(stderr, "[IOCP] add_fd: adding to entries list\n");
    _entries.push_back (entry);

    // Increase the load metric
    adjust_load (1);

    fprintf(stderr, "[IOCP] add_fd EXIT: returning entry=%p\n", entry);
    return entry;
}

void iocp_t::rm_fd (handle_t handle_)
{
    check_thread ();

    iocp_entry_t *entry = handle_;
    slk_assert (entry);

    // Mark as retired
    entry->retired.store (true, std::memory_order_release);

    // Cancel all pending I/O operations
    // CancelIoEx cancels all pending I/O operations for this socket
    // The cancelled operations will complete with ERROR_OPERATION_ABORTED
    CancelIoEx (reinterpret_cast<HANDLE> (entry->fd), NULL);

    // Move to retired list
    _retired.push_back (entry);

    // Remove from active entries
    auto it = std::find (_entries.begin (), _entries.end (), entry);
    if (it != _entries.end ()) {
        _entries.erase (it);
    }

    // Decrease the load metric
    adjust_load (-1);
}

void iocp_t::set_pollin (handle_t handle_)
{
    check_thread ();

    iocp_entry_t *entry = handle_;
    slk_assert (entry);

    fprintf(stderr, "[IOCP] set_pollin: entry=%p\n", handle_);

    bool expected = false;
    if (entry->want_pollin.compare_exchange_strong (expected, true,
                                                     std::memory_order_acq_rel)) {
        // Start async read operation if not already pending
        start_async_recv (entry);
    }
}

void iocp_t::reset_pollin (handle_t handle_)
{
    check_thread ();

    iocp_entry_t *entry = handle_;
    slk_assert (entry);

    entry->want_pollin.store (false, std::memory_order_release);
}

void iocp_t::set_pollout (handle_t handle_)
{
    check_thread ();

    iocp_entry_t *entry = handle_;
    slk_assert (entry);

    fprintf(stderr, "[IOCP] set_pollout: entry=%p\n", handle_);

    bool expected = false;
    if (entry->want_pollout.compare_exchange_strong (
          expected, true, std::memory_order_acq_rel)) {
        // Start async write operation if not already pending
        start_async_send (entry);
    }
}

void iocp_t::reset_pollout (handle_t handle_)
{
    check_thread ();

    iocp_entry_t *entry = handle_;
    slk_assert (entry);

    entry->want_pollout.store (false, std::memory_order_release);
}

void iocp_t::stop ()
{
    check_thread ();
    _stopping = true;

    fprintf(stderr, "[IOCP] stop() called, posting SHUTDOWN_KEY\n");

    // Post a special completion packet to wake up the worker thread
    BOOL rc = PostQueuedCompletionStatus (_iocp, 0, SHUTDOWN_KEY, NULL);

    fprintf(stderr, "[IOCP] SHUTDOWN_KEY posted: rc=%d\n", rc);

    win_assert (rc != 0);
}

void iocp_t::send_signal ()
{
    fprintf(stderr, "[iocp_t::send_signal] ENTER: this=%p, _iocp=%p\n", this, _iocp);

    // Post a signaler completion packet to wake up the I/O thread
    // This is thread-safe and can be called from any thread
    BOOL rc = PostQueuedCompletionStatus (_iocp, 0, SIGNALER_KEY, NULL);

    fprintf(stderr, "[iocp_t::send_signal] PostQueuedCompletionStatus result: rc=%d, error=%lu\n",
            rc, GetLastError());

    win_assert (rc != 0);

    fprintf(stderr, "[iocp_t::send_signal] EXIT\n");
}

void iocp_t::set_mailbox_handler (i_poll_events *handler_)
{
    fprintf(stderr, "[iocp_t::set_mailbox_handler] this=%p, handler=%p\n", this, handler_);
    _mailbox_handler = handler_;
    fprintf(stderr, "[iocp_t::set_mailbox_handler] _mailbox_handler set to %p\n", _mailbox_handler);
}

void iocp_t::adjust_mailbox_load (int amount_)
{
    adjust_load (amount_);
}

int iocp_t::max_fds ()
{
    // IOCP doesn't have a hard limit like select's FD_SETSIZE
    // Return a large value to indicate effectively unlimited
    return 65536;
}

iocp_t::handle_t iocp_t::add_fd_select (fd_t fd_, i_poll_events *events_)
{
    fprintf(stderr, "[IOCP] add_fd_select: fd=%llu, events=%p (select-only, no IOCP registration)\n",
            (uint64_t)fd_, events_);

    check_thread ();

    // Create select entry (not registered with IOCP)
    select_entry_t *entry = new (std::nothrow) select_entry_t;
    alloc_assert (entry);

    entry->fd = fd_;
    entry->events = events_;
    entry->want_pollout = false;

    _select_entries.push_back (entry);

    // Increase the load metric
    adjust_load (1);

    // Return entry pointer cast to handle_t (compatible with iocp_entry_t*)
    return reinterpret_cast<handle_t> (entry);
}

void iocp_t::rm_fd_select (handle_t handle_)
{
    fprintf(stderr, "[IOCP] rm_fd_select: handle=%p\n", handle_);

    check_thread ();

    select_entry_t *entry = reinterpret_cast<select_entry_t *> (handle_);
    slk_assert (entry);

    // Remove from select entries
    auto it = std::find (_select_entries.begin (), _select_entries.end (), entry);
    if (it != _select_entries.end ()) {
        _select_entries.erase (it);
    }

    // Decrease the load metric
    adjust_load (-1);

    // Delete entry
    delete entry;
}

void iocp_t::set_pollout_select (handle_t handle_)
{
    fprintf(stderr, "[IOCP] set_pollout_select: handle=%p\n", handle_);

    check_thread ();

    select_entry_t *entry = reinterpret_cast<select_entry_t *> (handle_);
    slk_assert (entry);

    entry->want_pollout = true;
}

void iocp_t::loop ()
{
    fprintf(stderr, "[IOCP] loop() ENTER: load=%d, _stopping=%d\n", get_load(), _stopping);

    OVERLAPPED_ENTRY entries[MAX_COMPLETIONS];

    while (!_stopping) {
        fprintf(stderr, "[IOCP] loop iteration: _stopping=%d, load=%d\n", _stopping, get_load());

        // Execute any due timers
        const uint64_t timeout = execute_timers ();

        if (get_load () == 0) {
            fprintf(stderr, "[IOCP] load=0, timeout=%llu - checking exit condition\n", timeout);
            if (timeout == 0) {
                fprintf(stderr, "[IOCP] load=0 and timeout=0 - breaking loop\n");
                break;
            }
            fprintf(stderr, "[IOCP] load=0 but timeout=%llu - continuing\n", timeout);
            continue;
        }

        // Check select entries first (connector sockets)
        if (!_select_entries.empty ()) {
            fd_set write_fds;
            FD_ZERO (&write_fds);
            fd_t max_fd = 0;

            for (select_entry_t *entry : _select_entries) {
                if (entry->want_pollout) {
                    FD_SET (entry->fd, &write_fds);
                    if (entry->fd > max_fd)
                        max_fd = entry->fd;
                }
            }

            if (max_fd > 0) {
                // Use minimal timeout for select (0 = non-blocking poll)
                struct timeval tv = {0, 0};
                int rc = select (static_cast<int> (max_fd + 1), NULL, &write_fds, NULL, &tv);

                if (rc > 0) {
                    // Some sockets are ready
                    for (select_entry_t *entry : _select_entries) {
                        if (entry->want_pollout && FD_ISSET (entry->fd, &write_fds)) {
                            fprintf(stderr, "[IOCP] select: fd=%llu ready for write\n", (uint64_t)entry->fd);
                            entry->want_pollout = false;
                            entry->events->out_event ();
                        }
                    }
                }
            }
        }

        fprintf(stderr, "[IOCP] Waiting for completions, timeout=%llu\n", timeout);

        // Wait for I/O completion events
        ULONG count = 0;
        BOOL ok = GetQueuedCompletionStatusEx (
          _iocp, entries, MAX_COMPLETIONS, &count,
          timeout > 0 ? static_cast<DWORD> (timeout) : INFINITE, FALSE);

        fprintf(stderr, "[IOCP] Received %lu completions (ok=%d, error=%lu)\n",
                count, ok, GetLastError());

        if (!ok) {
            // GetQueuedCompletionStatusEx failed
            DWORD err = GetLastError ();
            if (err == WAIT_TIMEOUT) {
                // Timeout is normal, just continue
                continue;
            }
            // Other errors are unexpected - assert
            // Format: win_assert(condition) - will print error details
            slk_assert (false);
        }

        // Process all completion packets
        for (ULONG i = 0; i < count; ++i) {
            OVERLAPPED_ENTRY &entry = entries[i];

            fprintf(stderr, "[IOCP] Processing completion packet %lu/%lu: key=%p\n",
                    i + 1, count, (void*)entry.lpCompletionKey);

            // Check for shutdown signal
            if (entry.lpCompletionKey == SHUTDOWN_KEY) {
                fprintf(stderr, "[IOCP] SHUTDOWN_KEY received! Setting _stopping=true and breaking\n");
                _stopping = true;
                break;
            }

            // Check for signaler (mailbox wakeup)
            if (entry.lpCompletionKey == SIGNALER_KEY) {
                fprintf(stderr, "[IOCP] SIGNALER_KEY received! _mailbox_handler=%p\n", _mailbox_handler);
                // Mailbox has commands to process
                // Call the registered mailbox handler's in_event()
                if (_mailbox_handler) {
                    fprintf(stderr, "[IOCP] Calling _mailbox_handler->in_event()\n");
                    _mailbox_handler->in_event ();
                    fprintf(stderr, "[IOCP] _mailbox_handler->in_event() returned\n");
                } else {
                    fprintf(stderr, "[IOCP] WARNING: SIGNALER_KEY received but _mailbox_handler is NULL!\n");
                }
                continue;
            }

            // Extract entry and overlapped structure
            iocp_entry_t *iocp_entry =
              reinterpret_cast<iocp_entry_t *> (entry.lpCompletionKey);
            overlapped_ex_t *ovl =
              static_cast<overlapped_ex_t *> (entry.lpOverlapped);

            if (!iocp_entry || !ovl)
                continue;

            // Get completion status
            DWORD bytes = entry.dwNumberOfBytesTransferred;
            DWORD error = ERROR_SUCCESS;

            // For GetQueuedCompletionStatusEx, we need to extract the error
            // from OVERLAPPED using GetOverlappedResult or check Internal field
            // The Internal field contains NTSTATUS which needs conversion
            // But for simplicity, we check if bytes is 0 and operation failed
            // A more robust approach is to use GetOverlappedResult
            BOOL ovl_result = TRUE;
            DWORD ovl_bytes = 0;
            if (bytes == 0 || ovl->Internal != 0) {
                // Get detailed error status
                ovl_result =
                  GetOverlappedResult (reinterpret_cast<HANDLE> (ovl->socket),
                                       ovl, &ovl_bytes, FALSE);
                if (!ovl_result) {
                    error = GetLastError ();
                }
            }

            // Process based on operation type
            // Simplified IOCP: Only READ/WRITE operations supported
            if (ovl->type == overlapped_ex_t::OP_READ) {
                handle_read_completion (iocp_entry, bytes, error);
            } else if (ovl->type == overlapped_ex_t::OP_WRITE) {
                handle_write_completion (iocp_entry, bytes, error);
            }
        }

        // Clean up retired entries with no pending I/O
        cleanup_retired ();
    }

    fprintf(stderr, "[IOCP] loop() EXIT: _stopping=%d, load=%d\n", _stopping, get_load());

    // Final cleanup
    cleanup_retired ();

    fprintf(stderr, "[IOCP] loop() COMPLETE\n");
}

void iocp_t::start_async_recv (iocp_entry_t *entry_)
{
    overlapped_ex_t *ovl = entry_->read_ovl.get ();
    slk_assert (ovl);

    fprintf(stderr, "[IOCP] WSARecv: entry=%p, fd=%llu\n",
            entry_, (uint64_t)entry_->fd);

    // Check if already pending
    bool expected = false;
    if (!ovl->pending.compare_exchange_strong (expected, true,
                                               std::memory_order_acq_rel)) {
        // Already pending, don't start another operation
        return;
    }

    // Check if entry is retired
    if (entry_->retired.load (std::memory_order_acquire)) {
        ovl->pending.store (false, std::memory_order_release);
        return;
    }

    // Reset OVERLAPPED structure (keep type, socket, entry)
    ovl->Internal = 0;
    ovl->InternalHigh = 0;
    ovl->Offset = 0;
    ovl->OffsetHigh = 0;
    ovl->hEvent = NULL;
    ovl->cancelled.store (false, std::memory_order_relaxed);

    // Start async receive
    DWORD flags = 0;
    DWORD bytes_received = 0;
    int rc = WSARecv (entry_->fd, &ovl->wsabuf, 1, &bytes_received, &flags,
                      static_cast<LPWSAOVERLAPPED> (ovl), NULL);

    fprintf(stderr, "[IOCP] WSARecv result: rc=%d, error=%lu, pending=%d\n",
            rc, WSAGetLastError(), ovl->pending.load());

    if (rc == SOCKET_ERROR) {
        DWORD error = WSAGetLastError ();
        if (error != WSA_IO_PENDING) {
            // Operation failed immediately
            ovl->pending.store (false, std::memory_order_release);

            // Classify error and handle
            iocp_error_action action = classify_error (error);
            if (action == iocp_error_action::IOCP_RETRY) {
                // Retry on next iteration
                return;
            } else if (action == iocp_error_action::IOCP_CLOSE ||
                       action == iocp_error_action::IOCP_FATAL) {
                // Notify error through in_event
                entry_->events->in_event ();
            }
            return;
        }
    }

    // Operation is pending (WSA_IO_PENDING) or completed immediately
    // Increment pending count
    entry_->pending_count.fetch_add (1, std::memory_order_release);
}

void iocp_t::start_async_send (iocp_entry_t *entry_)
{
    overlapped_ex_t *ovl = entry_->write_ovl.get ();
    slk_assert (ovl);

    fprintf(stderr, "[IOCP] WSASend: entry=%p, fd=%llu\n",
            entry_, (uint64_t)entry_->fd);

    // Check if already pending
    bool expected = false;
    if (!ovl->pending.compare_exchange_strong (expected, true,
                                               std::memory_order_acq_rel)) {
        // Already pending, don't start another operation
        return;
    }

    // Check if entry is retired
    if (entry_->retired.load (std::memory_order_acquire)) {
        ovl->pending.store (false, std::memory_order_release);
        return;
    }

    // Reset OVERLAPPED structure (keep type, socket, entry)
    ovl->Internal = 0;
    ovl->InternalHigh = 0;
    ovl->Offset = 0;
    ovl->OffsetHigh = 0;
    ovl->hEvent = NULL;
    ovl->cancelled.store (false, std::memory_order_relaxed);

    // For send operations, we use a dummy buffer (actual data is sent via send())
    // This is just to get write readiness notification
    DWORD bytes_sent = 0;
    int rc = WSASend (entry_->fd, &ovl->wsabuf, 1, &bytes_sent, 0,
                      static_cast<LPWSAOVERLAPPED> (ovl), NULL);

    fprintf(stderr, "[IOCP] WSASend result: rc=%d, error=%lu, pending=%d\n",
            rc, WSAGetLastError(), ovl->pending.load());

    if (rc == SOCKET_ERROR) {
        DWORD error = WSAGetLastError ();
        if (error != WSA_IO_PENDING) {
            // Operation failed immediately
            ovl->pending.store (false, std::memory_order_release);

            // Classify error and handle
            iocp_error_action action = classify_error (error);
            if (action == iocp_error_action::IOCP_RETRY) {
                // Retry on next iteration
                return;
            } else if (action == iocp_error_action::IOCP_CLOSE ||
                       action == iocp_error_action::IOCP_FATAL) {
                // Notify error through out_event
                entry_->events->out_event ();
            }
            return;
        }
    }

    // Operation is pending (WSA_IO_PENDING) or completed immediately
    // Increment pending count
    entry_->pending_count.fetch_add (1, std::memory_order_release);
}

void iocp_t::handle_read_completion (iocp_entry_t *entry_, DWORD bytes_,
                                     DWORD error_)
{
    fprintf(stderr, "[IOCP] Read completion: entry=%p, bytes=%lu, error=%lu\n",
            entry_, bytes_, error_);

    overlapped_ex_t *ovl = entry_->read_ovl.get ();
    slk_assert (ovl);

    // Clear pending flag
    ovl->pending.store (false, std::memory_order_release);

    // Decrement pending count
    entry_->pending_count.fetch_sub (1, std::memory_order_release);

    // Check if entry is retired or cancelled
    if (entry_->retired.load (std::memory_order_acquire) ||
        ovl->cancelled.load (std::memory_order_acquire)) {
        // Entry is being removed, don't process
        return;
    }

    // Classify error
    iocp_error_action action = classify_error (error_);

    if (action == iocp_error_action::IOCP_IGNORE) {
        // Success - notify the event handler with completion data
        // This enables Direct Engine pattern: data is passed directly
        // to the handler without additional recv() system call
        entry_->events->in_completed (ovl->buffer, static_cast<size_t> (bytes_),
                                      0);

        // If still want_pollin, start next read
        if (entry_->want_pollin.load (std::memory_order_acquire) &&
            !entry_->retired.load (std::memory_order_acquire)) {
            start_async_recv (entry_);
        }
    } else if (action == iocp_error_action::IOCP_RETRY) {
        // Temporary error - retry if still interested
        if (entry_->want_pollin.load (std::memory_order_acquire) &&
            !entry_->retired.load (std::memory_order_acquire)) {
            start_async_recv (entry_);
        }
    } else if (action == iocp_error_action::IOCP_CLOSE ||
               action == iocp_error_action::IOCP_FATAL) {
        // Connection error or fatal error - notify handler with error code
        // The handler will typically close the socket
        entry_->events->in_completed (nullptr, 0,
                                      static_cast<int> (error_));
    }
}

void iocp_t::handle_write_completion (iocp_entry_t *entry_, DWORD bytes_,
                                      DWORD error_)
{
    fprintf(stderr, "[IOCP] Write completion: entry=%p, bytes=%lu, error=%lu\n",
            entry_, bytes_, error_);

    overlapped_ex_t *ovl = entry_->write_ovl.get ();
    slk_assert (ovl);

    // Clear pending flag
    ovl->pending.store (false, std::memory_order_release);

    // Decrement pending count
    entry_->pending_count.fetch_sub (1, std::memory_order_release);

    // Check if entry is retired or cancelled
    if (entry_->retired.load (std::memory_order_acquire) ||
        ovl->cancelled.load (std::memory_order_acquire)) {
        // Entry is being removed, don't process
        return;
    }

    // Classify error
    iocp_error_action action = classify_error (error_);

    if (action == iocp_error_action::IOCP_IGNORE) {
        // Success - notify the event handler with bytes sent
        // This enables Direct Engine pattern: actual bytes sent is provided
        // to the handler for precise flow control
        entry_->events->out_completed (static_cast<size_t> (bytes_), 0);

        // If still want_pollout, start next write
        if (entry_->want_pollout.load (std::memory_order_acquire) &&
            !entry_->retired.load (std::memory_order_acquire)) {
            start_async_send (entry_);
        }
    } else if (action == iocp_error_action::IOCP_RETRY) {
        // Temporary error - retry if still interested
        if (entry_->want_pollout.load (std::memory_order_acquire) &&
            !entry_->retired.load (std::memory_order_acquire)) {
            start_async_send (entry_);
        }
    } else if (action == iocp_error_action::IOCP_CLOSE ||
               action == iocp_error_action::IOCP_FATAL) {
        // Connection error or fatal error - notify handler with error code
        // The handler will typically close the socket
        entry_->events->out_completed (0, static_cast<int> (error_));
    }
}

void iocp_t::cleanup_retired ()
{
    // Clean up retired entries that have no pending I/O
    auto it = _retired.begin ();
    while (it != _retired.end ()) {
        iocp_entry_t *entry = *it;

        // Check if all pending I/O operations have completed
        int pending = entry->pending_count.load (std::memory_order_acquire);
        if (pending == 0) {
            // Safe to delete - no pending operations
            delete entry;
            it = _retired.erase (it);
        } else {
            // Still has pending I/O, keep in retired list
            // CancelIoEx was already called in rm_fd()
            // The cancelled operations will complete with ERROR_OPERATION_ABORTED
            ++it;
        }
    }
}

// ============================================================================
// Simplified IOCP Implementation Notes:
// ============================================================================
// AcceptEx/ConnectEx functions removed for simplicity.
// ServerLink uses BSD socket accept() and connect() calls instead.
// IOCP is only used for high-performance async I/O (WSARecv/WSASend).
//
// Rationale:
// 1. BSD socket API provides better portability across platforms
// 2. AcceptEx/ConnectEx add significant complexity with minimal benefit
// 3. IOCP shines for bulk I/O operations, not connection establishment
// 4. Simpler codebase is easier to maintain and debug
// ============================================================================

}  // namespace slk

#endif  // SL_USE_IOCP

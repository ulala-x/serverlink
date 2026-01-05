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

    // AcceptEx 필드 초기화
    accept_socket = retired_fd;
    memset (accept_buffer, 0, sizeof (accept_buffer));
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
            return iocp_error_action::IGNORE;

        // Retry cases - temporary errors
        case WSAEWOULDBLOCK:
        case WSAEINTR:
        case WSAEINPROGRESS:
            return iocp_error_action::RETRY;

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
            return iocp_error_action::CLOSE;

        // Fatal cases - programming errors or system failures
        case WSAENOTSOCK:
        case WSAEINVAL:
        case WSAEFAULT:
        case WSAEBADF:
        case ERROR_INVALID_HANDLE:
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return iocp_error_action::FATAL;

        // Operation aborted (usually from CancelIoEx)
        case ERROR_OPERATION_ABORTED:
            return iocp_error_action::CLOSE;

        // Default: treat unknown errors as close-worthy
        default:
            return iocp_error_action::CLOSE;
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

    // Create OVERLAPPED structures for read, write, and connect operations
    read_ovl = std::make_unique<overlapped_ex_t> ();
    write_ovl = std::make_unique<overlapped_ex_t> ();
    connect_ovl = std::make_unique<overlapped_ex_t> ();

    read_ovl->type = overlapped_ex_t::OP_READ;
    read_ovl->socket = fd_;
    read_ovl->entry = this;

    write_ovl->type = overlapped_ex_t::OP_WRITE;
    write_ovl->socket = fd_;
    write_ovl->entry = this;

    connect_ovl->type = overlapped_ex_t::OP_CONNECT;
    connect_ovl->socket = fd_;
    connect_ovl->entry = this;

    want_pollin.store (false, std::memory_order_relaxed);
    want_pollout.store (false, std::memory_order_relaxed);
    pending_count.store (0, std::memory_order_relaxed);
    retired.store (false, std::memory_order_relaxed);
    is_listener.store (false, std::memory_order_relaxed);

    // AcceptEx 풀은 enable_accept() 호출 시 초기화
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
      _acceptex_fn (nullptr),
      _connectex_fn (nullptr),
      _mailbox_handler (nullptr)
{
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

    // Close IOCP handle
    if (_iocp) {
        BOOL rc = CloseHandle (_iocp);
        win_assert (rc != 0);
    }
}

iocp_t::handle_t iocp_t::add_fd (fd_t fd_, i_poll_events *events_)
{
    check_thread ();

    // Create new entry
    iocp_entry_t *entry = new (std::nothrow) iocp_entry_t (fd_, events_);
    alloc_assert (entry);

    // Associate socket with IOCP
    // Parameters:
    //   fd_: the socket handle to associate
    //   _iocp: the completion port to associate with
    //   (ULONG_PTR)entry: completion key (pointer to our entry)
    //   0: number of concurrent threads (ignored for existing IOCP)
    HANDLE ret =
      CreateIoCompletionPort (reinterpret_cast<HANDLE> (fd_), _iocp,
                              reinterpret_cast<ULONG_PTR> (entry), 0);
    win_assert (ret == _iocp);

    _entries.push_back (entry);

    // Increase the load metric
    adjust_load (1);

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

    // Post a special completion packet to wake up the worker thread
    BOOL rc = PostQueuedCompletionStatus (_iocp, 0, SHUTDOWN_KEY, NULL);
    win_assert (rc != 0);
}

void iocp_t::send_signal ()
{
    // Post a signaler completion packet to wake up the I/O thread
    // This is thread-safe and can be called from any thread
    BOOL rc = PostQueuedCompletionStatus (_iocp, 0, SIGNALER_KEY, NULL);
    win_assert (rc != 0);
}

void iocp_t::set_mailbox_handler (i_poll_events *handler_)
{
    _mailbox_handler = handler_;
}

int iocp_t::max_fds ()
{
    // IOCP doesn't have a hard limit like select's FD_SETSIZE
    // Return a large value to indicate effectively unlimited
    return 65536;
}

void iocp_t::loop ()
{
    OVERLAPPED_ENTRY entries[MAX_COMPLETIONS];

    while (!_stopping) {
        // Execute any due timers
        const uint64_t timeout = execute_timers ();

        if (get_load () == 0) {
            if (timeout == 0)
                break;
            continue;
        }

        // Wait for I/O completion events
        ULONG count = 0;
        BOOL ok = GetQueuedCompletionStatusEx (
          _iocp, entries, MAX_COMPLETIONS, &count,
          timeout > 0 ? static_cast<DWORD> (timeout) : INFINITE, FALSE);

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

            // Check for shutdown signal
            if (entry.lpCompletionKey == SHUTDOWN_KEY) {
                _stopping = true;
                break;
            }

            // Check for signaler (mailbox wakeup)
            if (entry.lpCompletionKey == SIGNALER_KEY) {
                // Mailbox has commands to process
                // Call the registered mailbox handler's in_event()
                if (_mailbox_handler) {
                    _mailbox_handler->in_event ();
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
            if (ovl->type == overlapped_ex_t::OP_READ) {
                handle_read_completion (iocp_entry, bytes, error);
            } else if (ovl->type == overlapped_ex_t::OP_WRITE) {
                handle_write_completion (iocp_entry, bytes, error);
            } else if (ovl->type == overlapped_ex_t::OP_ACCEPT) {
                handle_accept_completion (iocp_entry, ovl, error);
            } else if (ovl->type == overlapped_ex_t::OP_CONNECT) {
                handle_connect_completion (iocp_entry, error);
            }
        }

        // Clean up retired entries with no pending I/O
        cleanup_retired ();
    }

    // Final cleanup
    cleanup_retired ();
}

void iocp_t::start_async_recv (iocp_entry_t *entry_)
{
    overlapped_ex_t *ovl = entry_->read_ovl.get ();
    slk_assert (ovl);

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

    if (rc == SOCKET_ERROR) {
        DWORD error = WSAGetLastError ();
        if (error != WSA_IO_PENDING) {
            // Operation failed immediately
            ovl->pending.store (false, std::memory_order_release);

            // Classify error and handle
            iocp_error_action action = classify_error (error);
            if (action == iocp_error_action::RETRY) {
                // Retry on next iteration
                return;
            } else if (action == iocp_error_action::CLOSE ||
                       action == iocp_error_action::FATAL) {
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

    if (rc == SOCKET_ERROR) {
        DWORD error = WSAGetLastError ();
        if (error != WSA_IO_PENDING) {
            // Operation failed immediately
            ovl->pending.store (false, std::memory_order_release);

            // Classify error and handle
            iocp_error_action action = classify_error (error);
            if (action == iocp_error_action::RETRY) {
                // Retry on next iteration
                return;
            } else if (action == iocp_error_action::CLOSE ||
                       action == iocp_error_action::FATAL) {
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

    if (action == iocp_error_action::IGNORE) {
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
    } else if (action == iocp_error_action::RETRY) {
        // Temporary error - retry if still interested
        if (entry_->want_pollin.load (std::memory_order_acquire) &&
            !entry_->retired.load (std::memory_order_acquire)) {
            start_async_recv (entry_);
        }
    } else if (action == iocp_error_action::CLOSE ||
               action == iocp_error_action::FATAL) {
        // Connection error or fatal error - notify handler with error code
        // The handler will typically close the socket
        entry_->events->in_completed (nullptr, 0,
                                      static_cast<int> (error_));
    }
}

void iocp_t::handle_write_completion (iocp_entry_t *entry_, DWORD bytes_,
                                      DWORD error_)
{
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

    if (action == iocp_error_action::IGNORE) {
        // Success - notify the event handler with bytes sent
        // This enables Direct Engine pattern: actual bytes sent is provided
        // to the handler for precise flow control
        entry_->events->out_completed (static_cast<size_t> (bytes_), 0);

        // If still want_pollout, start next write
        if (entry_->want_pollout.load (std::memory_order_acquire) &&
            !entry_->retired.load (std::memory_order_acquire)) {
            start_async_send (entry_);
        }
    } else if (action == iocp_error_action::RETRY) {
        // Temporary error - retry if still interested
        if (entry_->want_pollout.load (std::memory_order_acquire) &&
            !entry_->retired.load (std::memory_order_acquire)) {
            start_async_send (entry_);
        }
    } else if (action == iocp_error_action::CLOSE ||
               action == iocp_error_action::FATAL) {
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

void iocp_t::enable_accept (handle_t handle_)
{
    check_thread ();

    iocp_entry_t *entry = handle_;
    slk_assert (entry);

    // AcceptEx 함수 포인터 로드 (최초 1회)
    if (!_acceptex_fn) {
        GUID guid = WSAID_ACCEPTEX;
        DWORD bytes = 0;
        int rc = WSAIoctl (entry->fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid,
                           sizeof (guid), &_acceptex_fn, sizeof (_acceptex_fn),
                           &bytes, NULL, NULL);
        if (rc == SOCKET_ERROR) {
            // AcceptEx를 사용할 수 없으면 select 모드로 fallback
            // 여기서는 단순히 경고만 하고 계속 진행 (set_pollin으로 fallback)
            return;
        }
    }

    // Listener로 표시
    entry->is_listener.store (true, std::memory_order_release);

    // AcceptEx 풀 초기화 및 프리-포스팅
    for (size_t i = 0; i < iocp_entry_t::ACCEPT_POOL_SIZE; ++i) {
        entry->accept_pool[i] = std::make_unique<overlapped_ex_t> ();
        overlapped_ex_t *ovl = entry->accept_pool[i].get ();

        ovl->type = overlapped_ex_t::OP_ACCEPT;
        ovl->socket = entry->fd;
        ovl->entry = entry;

        // AcceptEx 등록
        post_accept (entry, ovl);
    }
}

void iocp_t::post_accept (iocp_entry_t *entry_, overlapped_ex_t *ovl_)
{
    slk_assert (_acceptex_fn);
    slk_assert (ovl_->type == overlapped_ex_t::OP_ACCEPT);

    // 이미 pending이면 스킵
    bool expected = false;
    if (!ovl_->pending.compare_exchange_strong (expected, true,
                                                std::memory_order_acq_rel)) {
        return;
    }

    // Entry가 retired 상태면 중단
    if (entry_->retired.load (std::memory_order_acquire)) {
        ovl_->pending.store (false, std::memory_order_release);
        return;
    }

    // OVERLAPPED 구조체 리셋
    ovl_->Internal = 0;
    ovl_->InternalHigh = 0;
    ovl_->Offset = 0;
    ovl_->OffsetHigh = 0;
    ovl_->hEvent = NULL;
    ovl_->cancelled.store (false, std::memory_order_relaxed);

    // accept 소켓 생성 (IPv6 dual-stack)
    ovl_->accept_socket =
      WSASocket (AF_INET6, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (ovl_->accept_socket == INVALID_SOCKET) {
        ovl_->pending.store (false, std::memory_order_release);
        return;
    }

    // AcceptEx 호출
    // dwReceiveDataLength=0: 연결 즉시 완료 (데이터 수신 대기 안 함)
    DWORD bytes = 0;
    BOOL ok = _acceptex_fn (
      entry_->fd, ovl_->accept_socket, ovl_->accept_buffer, 0,
      sizeof (sockaddr_in6) + 16, sizeof (sockaddr_in6) + 16, &bytes,
      static_cast<LPOVERLAPPED> (ovl_));

    if (!ok) {
        DWORD error = WSAGetLastError ();
        if (error != WSA_IO_PENDING) {
            // 즉시 실패 - accept 소켓 닫기
            closesocket (ovl_->accept_socket);
            ovl_->accept_socket = retired_fd;
            ovl_->pending.store (false, std::memory_order_release);
            return;
        }
    }

    // Pending 카운트 증가
    entry_->pending_count.fetch_add (1, std::memory_order_release);
}

void iocp_t::handle_accept_completion (iocp_entry_t *entry_,
                                        overlapped_ex_t *ovl_, DWORD error_)
{
    slk_assert (ovl_);
    slk_assert (ovl_->type == overlapped_ex_t::OP_ACCEPT);

    // Pending 플래그 해제
    ovl_->pending.store (false, std::memory_order_release);

    // Pending 카운트 감소
    entry_->pending_count.fetch_sub (1, std::memory_order_release);

    // Retired 또는 cancelled 상태면 종료
    if (entry_->retired.load (std::memory_order_acquire) ||
        ovl_->cancelled.load (std::memory_order_acquire)) {
        if (ovl_->accept_socket != retired_fd) {
            closesocket (ovl_->accept_socket);
            ovl_->accept_socket = retired_fd;
        }
        return;
    }

    // 에러 분류
    iocp_error_action action = classify_error (error_);

    if (action == iocp_error_action::IGNORE) {
        // 성공 - SO_UPDATE_ACCEPT_CONTEXT 설정 (필수!)
        int rc = setsockopt (ovl_->accept_socket, SOL_SOCKET,
                             SO_UPDATE_ACCEPT_CONTEXT,
                             reinterpret_cast<char *> (&entry_->fd),
                             sizeof (entry_->fd));
        if (rc == SOCKET_ERROR) {
            // SO_UPDATE_ACCEPT_CONTEXT 실패 - 소켓 닫기
            closesocket (ovl_->accept_socket);
            ovl_->accept_socket = retired_fd;
        } else {
            // 새 연결을 이벤트 핸들러에 전달
            fd_t accepted_socket = ovl_->accept_socket;
            ovl_->accept_socket = retired_fd;  // 소유권 이전

            // accept_completed 호출 (listener가 오버라이드 가능)
            entry_->events->accept_completed (accepted_socket, 0);
        }

        // 다음 accept를 위해 재등록
        if (!entry_->retired.load (std::memory_order_acquire)) {
            post_accept (entry_, ovl_);
        }
    } else {
        // 에러 발생 - accept 소켓 닫기
        if (ovl_->accept_socket != retired_fd) {
            closesocket (ovl_->accept_socket);
            ovl_->accept_socket = retired_fd;
        }

        // RETRY 에러면 재등록 시도
        if (action == iocp_error_action::RETRY &&
            !entry_->retired.load (std::memory_order_acquire)) {
            post_accept (entry_, ovl_);
        }
    }
}

void iocp_t::enable_connect (handle_t handle_, const struct sockaddr *addr_,
                             int addrlen_)
{
    check_thread ();

    iocp_entry_t *entry = handle_;
    slk_assert (entry);

    // ConnectEx 함수 포인터 로드 (최초 1회)
    if (!_connectex_fn) {
        GUID guid = WSAID_CONNECTEX;
        DWORD bytes = 0;
        int rc = WSAIoctl (entry->fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid,
                           sizeof (guid), &_connectex_fn, sizeof (_connectex_fn),
                           &bytes, NULL, NULL);
        if (rc == SOCKET_ERROR) {
            // ConnectEx를 사용할 수 없으면 select 모드로 fallback
            // 여기서는 단순히 경고만 하고 계속 진행 (set_pollout으로 fallback)
            return;
        }
    }

    // 원격 주소 저장
    overlapped_ex_t *ovl = entry->connect_ovl.get ();
    slk_assert (ovl);
    slk_assert (addrlen_ <= static_cast<int> (sizeof (ovl->remote_addr)));

    memcpy (&ovl->remote_addr, addr_, addrlen_);
    ovl->remote_addrlen = addrlen_;

    // ConnectEx 비동기 연결 시작
    start_async_connect (entry);
}

void iocp_t::start_async_connect (iocp_entry_t *entry_)
{
    overlapped_ex_t *ovl = entry_->connect_ovl.get ();
    slk_assert (ovl);
    slk_assert (_connectex_fn);

    // 이미 pending이면 스킵
    bool expected = false;
    if (!ovl->pending.compare_exchange_strong (expected, true,
                                                std::memory_order_acq_rel)) {
        return;
    }

    // Entry가 retired 상태면 중단
    if (entry_->retired.load (std::memory_order_acquire)) {
        ovl->pending.store (false, std::memory_order_release);
        return;
    }

    // OVERLAPPED 구조체 리셋
    ovl->Internal = 0;
    ovl->InternalHigh = 0;
    ovl->Offset = 0;
    ovl->OffsetHigh = 0;
    ovl->hEvent = NULL;
    ovl->cancelled.store (false, std::memory_order_relaxed);

    // ⚠️ ConnectEx는 bind()가 선행되어야 함!
    // tcp_connecter에서 이미 bind 호출을 해야 함 (has_src_addr 또는 INADDR_ANY)
    // 여기서는 bind가 이미 되어 있다고 가정

    // ConnectEx 호출
    DWORD bytes = 0;
    BOOL ok = _connectex_fn (entry_->fd,
                             reinterpret_cast<const sockaddr *> (&ovl->remote_addr),
                             ovl->remote_addrlen, NULL, 0, &bytes,
                             static_cast<LPOVERLAPPED> (ovl));

    if (!ok) {
        DWORD error = WSAGetLastError ();
        if (error != WSA_IO_PENDING) {
            // 즉시 실패
            ovl->pending.store (false, std::memory_order_release);

            // 에러를 이벤트 핸들러에 전달
            entry_->events->connect_completed (static_cast<int> (error));
            return;
        }
    }

    // Pending 카운트 증가
    entry_->pending_count.fetch_add (1, std::memory_order_release);
}

void iocp_t::handle_connect_completion (iocp_entry_t *entry_, DWORD error_)
{
    overlapped_ex_t *ovl = entry_->connect_ovl.get ();
    slk_assert (ovl);

    // Pending 플래그 해제
    ovl->pending.store (false, std::memory_order_release);

    // Pending 카운트 감소
    entry_->pending_count.fetch_sub (1, std::memory_order_release);

    // Retired 또는 cancelled 상태면 종료
    if (entry_->retired.load (std::memory_order_acquire) ||
        ovl->cancelled.load (std::memory_order_acquire)) {
        return;
    }

    // 에러 분류
    iocp_error_action action = classify_error (error_);

    if (action == iocp_error_action::IGNORE) {
        // 성공 - SO_UPDATE_CONNECT_CONTEXT 설정 (필수!)
        int rc = setsockopt (entry_->fd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT,
                             NULL, 0);
        if (rc == SOCKET_ERROR) {
            // SO_UPDATE_CONNECT_CONTEXT 실패
            DWORD update_error = WSAGetLastError ();
            entry_->events->connect_completed (static_cast<int> (update_error));
        } else {
            // 연결 성공 - 이벤트 핸들러에 통지
            entry_->events->connect_completed (0);
        }
    } else {
        // 연결 실패 - 에러 코드와 함께 이벤트 핸들러에 통지
        entry_->events->connect_completed (static_cast<int> (error_));
    }
}

}  // namespace slk

#endif  // SL_USE_IOCP

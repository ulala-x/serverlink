/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_IOCP_HPP_INCLUDED
#define SERVERLINK_IOCP_HPP_INCLUDED

#include "../util/config.hpp"

#if defined SL_USE_IOCP

#include <array>
#include <vector>
#include <memory>
#include <atomic>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mswsock.h>
#endif

#include "fd.hpp"
#include "poller_base.hpp"
#include "../util/macros.hpp"

namespace slk
{
struct i_poll_events;
class ctx_t;

// Socket polling mechanism using Windows IOCP (I/O Completion Ports)
// Provides high-performance asynchronous I/O for Windows platforms

// Forward declaration
struct iocp_entry_t;

// Error classification for IOCP completion status codes
enum class iocp_error_action
{
    IOCP_IGNORE,  // Ignorable error (e.g., WSA_IO_PENDING)
    IOCP_RETRY,   // Retryable error (e.g., WSAEWOULDBLOCK)
    IOCP_CLOSE,   // Connection close required (e.g., WSAECONNRESET)
    IOCP_FATAL    // Fatal error (e.g., WSAENOTSOCK)
};

iocp_error_action classify_error (DWORD error_);

// Extended OVERLAPPED structure with RAII pattern
// Extends Windows OVERLAPPED for I/O operation tracking and management
// NOTE: AcceptEx/ConnectEx support removed - implementation in iocp.cpp needs update
struct overlapped_ex_t : WSAOVERLAPPED
{
    enum op_type
    {
        OP_READ,
        OP_WRITE
    };

    op_type type;
    fd_t socket;
    iocp_entry_t *entry;

    WSABUF wsabuf;
    static constexpr size_t BUF_SIZE = 8192;
    unsigned char buffer[BUF_SIZE];

    std::atomic<bool> pending{false};
    std::atomic<bool> cancelled{false};

    overlapped_ex_t ();
    ~overlapped_ex_t () = default;

    SL_NON_COPYABLE_NOR_MOVABLE (overlapped_ex_t)
};

using overlapped_ptr = std::unique_ptr<overlapped_ex_t>;

// Socket entry - manages state and pending I/O for each socket registered with IOCP
struct iocp_entry_t
{
    fd_t fd;
    i_poll_events *events;

    overlapped_ptr read_ovl;
    overlapped_ptr write_ovl;

    std::atomic<bool> want_pollin{false};
    std::atomic<bool> want_pollout{false};
    std::atomic<int> pending_count{0};
    std::atomic<bool> retired{false};

    SRWLOCK lock;

    iocp_entry_t (fd_t fd_, i_poll_events *events_);
    ~iocp_entry_t ();

    SL_NON_COPYABLE_NOR_MOVABLE (iocp_entry_t)
};

// IOCP Poller class
class iocp_t final : public worker_poller_base_t
{
  public:
    typedef iocp_entry_t *handle_t;

    iocp_t (ctx_t *ctx_);
    ~iocp_t () override;

    // "poller" concept
    handle_t add_fd (fd_t fd_, i_poll_events *events_);
    void rm_fd (handle_t handle_);
    void set_pollin (handle_t handle_);
    void reset_pollin (handle_t handle_);
    void set_pollout (handle_t handle_);
    void reset_pollout (handle_t handle_);
    void stop ();

    // Connector select polling - don't register with IOCP
    handle_t add_fd_select (fd_t fd_, i_poll_events *events_);
    void rm_fd_select (handle_t handle_);
    void set_pollout_select (handle_t handle_);

    // Signaler support - wake up I/O thread from other threads
    void send_signal ();

    // Set mailbox event handler for signaler processing
    void set_mailbox_handler (i_poll_events *handler_);

    // Adjust load count for mailbox (used by io_thread when not using add_fd)
    void adjust_mailbox_load (int amount_);

    static int max_fds ();

  private:
    // Main event loop
    void loop () override;

    // Start async I/O operations
    void start_async_recv (iocp_entry_t *entry_);
    void start_async_send (iocp_entry_t *entry_);

    // I/O completion handlers
    void handle_read_completion (iocp_entry_t *entry_, DWORD bytes_,
                                 DWORD error_);
    void handle_write_completion (iocp_entry_t *entry_, DWORD bytes_,
                                  DWORD error_);

    // Retired entry cleanup
    void cleanup_retired ();

    // IOCP handle
    HANDLE _iocp;

    // Active socket entries
    std::vector<iocp_entry_t *> _entries;

    // Retired socket entries waiting for cleanup
    std::vector<iocp_entry_t *> _retired;

    // Mailbox handler for signaler events (non-owning pointer)
    i_poll_events *_mailbox_handler;

    // Select-based polling for connector sockets (not registered with IOCP)
    struct select_entry_t
    {
        fd_t fd;
        i_poll_events *events;
        bool want_pollout;
    };
    std::vector<select_entry_t *> _select_entries;

    // IOCP configuration constants
    static constexpr int MAX_COMPLETIONS = 256;
    static constexpr ULONG_PTR SHUTDOWN_KEY = 0xDEADBEEF;
    static constexpr ULONG_PTR SIGNALER_KEY = 0x5149AAAA;

    SL_NON_COPYABLE_NOR_MOVABLE (iocp_t)
};

typedef iocp_t poller_t;

}  // namespace slk

#endif  // SL_USE_IOCP

#endif  // SERVERLINK_IOCP_HPP_INCLUDED

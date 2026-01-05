/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_IOCP_HPP_INCLUDED
#define SERVERLINK_IOCP_HPP_INCLUDED

#include "../util/config.hpp"

#if defined SL_USE_IOCP

#include <vector>
#include <memory>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
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

// OVERLAPPED 확장 구조체 (RAII 패턴)
// Windows OVERLAPPED 구조체를 확장하여 I/O 작업 추적 및 관리
struct overlapped_ex_t : WSAOVERLAPPED
{
    enum op_type
    {
        OP_READ,
        OP_WRITE,
        OP_ACCEPT,
        OP_CONNECT
    };

    op_type type;
    fd_t socket;
    iocp_entry_t *entry;

    WSABUF wsabuf;
    static constexpr size_t BUF_SIZE = 8192;
    unsigned char buffer[BUF_SIZE];

    std::atomic<bool> pending{false};
    std::atomic<bool> cancelled{false};

    // AcceptEx 전용 필드
    fd_t accept_socket;  // AcceptEx로 생성된 accept 소켓
    unsigned char accept_buffer[2 * (sizeof (sockaddr_in6) + 16)];

    // ConnectEx 전용 필드
    sockaddr_storage remote_addr;  // ConnectEx 대상 주소
    int remote_addrlen;            // 주소 길이

    overlapped_ex_t ();
    ~overlapped_ex_t () = default;

    SL_NON_COPYABLE_NOR_MOVABLE (overlapped_ex_t)
};

using overlapped_ptr = std::unique_ptr<overlapped_ex_t>;

// 에러 분류 - IOCP 완료 상태 코드에 대한 처리 전략 결정
enum class iocp_error_action
{
    IGNORE,  // 무시 가능한 에러 (예: WSA_IO_PENDING)
    RETRY,   // 재시도 가능한 에러 (예: WSAEWOULDBLOCK)
    CLOSE,   // 연결 종료가 필요한 에러 (예: WSAECONNRESET)
    FATAL    // 치명적 에러 (예: WSAENOTSOCK)
};

iocp_error_action classify_error (DWORD error_);

// 소켓 엔트리 - IOCP 완료 포트에 등록된 각 소켓의 상태 및 대기 중인 I/O 작업 관리
struct iocp_entry_t
{
    fd_t fd;
    i_poll_events *events;

    overlapped_ptr read_ovl;
    overlapped_ptr write_ovl;
    overlapped_ptr connect_ovl;  // ConnectEx 전용 OVERLAPPED

    std::atomic<bool> want_pollin{false};
    std::atomic<bool> want_pollout{false};
    std::atomic<int> pending_count{0};
    std::atomic<bool> retired{false};

    SRWLOCK lock;  // Slim Reader/Writer Lock for thread-safe access

    // AcceptEx 프리-포스팅 풀 (Listener 전용)
    static constexpr int ACCEPT_POOL_SIZE = 8;
    std::array<overlapped_ptr, ACCEPT_POOL_SIZE> accept_pool;
    std::atomic<bool> is_listener{false};

    iocp_entry_t (fd_t fd_, i_poll_events *events_);
    ~iocp_entry_t ();

    SL_NON_COPYABLE_NOR_MOVABLE (iocp_entry_t)
};

// IOCP Poller 클래스
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

    // Signaler support - wake up I/O thread from other threads
    void send_signal ();

    // Set mailbox event handler for signaler processing
    void set_mailbox_handler (i_poll_events *handler_);

    // AcceptEx 지원
    void enable_accept (handle_t handle_);

    // ConnectEx 지원
    void enable_connect (handle_t handle_, const struct sockaddr *addr_,
                         int addrlen_);

    static int max_fds ();

  private:
    // Main event loop
    void loop () override;

    // Async I/O 작업 시작
    void start_async_recv (iocp_entry_t *entry_);
    void start_async_send (iocp_entry_t *entry_);

    // I/O 완료 처리
    void handle_read_completion (iocp_entry_t *entry_, DWORD bytes_,
                                 DWORD error_);
    void handle_write_completion (iocp_entry_t *entry_, DWORD bytes_,
                                  DWORD error_);
    void handle_accept_completion (iocp_entry_t *entry_,
                                   overlapped_ex_t *ovl_, DWORD error_);

    // AcceptEx 작업 시작
    void post_accept (iocp_entry_t *entry_, overlapped_ex_t *ovl_);

    // ConnectEx 작업 시작
    void start_async_connect (iocp_entry_t *entry_);
    void handle_connect_completion (iocp_entry_t *entry_, DWORD error_);

    // Retired 엔트리 정리
    void cleanup_retired ();

    // IOCP handle
    HANDLE _iocp;

    // Active socket entries
    std::vector<iocp_entry_t *> _entries;

    // Retired socket entries waiting for cleanup
    std::vector<iocp_entry_t *> _retired;

    // AcceptEx 함수 포인터 (동적 로드)
    LPFN_ACCEPTEX _acceptex_fn;

    // ConnectEx 함수 포인터 (동적 로드)
    LPFN_CONNECTEX _connectex_fn;

    // Mailbox handler for signaler events (non-owning pointer)
    i_poll_events *_mailbox_handler;

    // IOCP configuration constants
    static constexpr int MAX_COMPLETIONS = 256;
    static constexpr ULONG_PTR SHUTDOWN_KEY = 0xDEADBEEF;
    static constexpr ULONG_PTR SIGNALER_KEY = 0x5149AAAA;

    SL_NON_COPYABLE_NOR_MOVABLE (iocp_t)
};

typedef iocp_t poller_t;
}

#endif  // SL_USE_IOCP

#endif

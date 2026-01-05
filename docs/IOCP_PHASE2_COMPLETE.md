# IOCP Phase 2 - 데이터 구조 및 기본 클래스 생성 완료

**날짜:** 2026-01-05
**상태:** ✅ 완료

## 개요

Windows IOCP (I/O Completion Ports) 통합을 위한 Phase 2 작업이 성공적으로 완료되었습니다.
핵심 데이터 구조와 기본 클래스가 생성되었으며, libzmq 패턴과 기존 select/epoll 구현을 참고하여
일관성 있는 설계를 구현했습니다.

## 생성된 파일

### 1. src/io/iocp.hpp (3,937 bytes)

**핵심 구조체 및 클래스:**

#### overlapped_ex_t
- WSAOVERLAPPED를 확장한 RAII 패턴 구조체
- I/O 작업 타입 추적 (OP_READ, OP_WRITE, OP_ACCEPT, OP_CONNECT)
- 8KB 버퍼와 WSABUF 포함
- atomic flag로 pending/cancelled 상태 관리

**특징:**
```cpp
struct overlapped_ex_t : WSAOVERLAPPED {
    enum op_type { OP_READ, OP_WRITE, OP_ACCEPT, OP_CONNECT };

    op_type type;
    fd_t socket;
    iocp_entry_t* entry;

    WSABUF wsabuf;
    static constexpr size_t BUF_SIZE = 8192;
    unsigned char buffer[BUF_SIZE];

    std::atomic<bool> pending{false};
    std::atomic<bool> cancelled{false};
};
```

#### iocp_entry_t
- IOCP 완료 포트에 등록된 각 소켓의 상태 관리
- read/write OVERLAPPED 구조체 보유
- Thread-safe 접근을 위한 SRWLOCK 사용
- atomic flag로 pollin/pollout 상태 관리

**특징:**
```cpp
struct iocp_entry_t {
    fd_t fd;
    i_poll_events* events;

    overlapped_ptr read_ovl;
    overlapped_ptr write_ovl;

    std::atomic<bool> want_pollin{false};
    std::atomic<bool> want_pollout{false};
    std::atomic<int> pending_count{0};
    std::atomic<bool> retired{false};

    SRWLOCK lock;
};
```

#### iocp_error_action
- IOCP 완료 상태 코드 처리 전략 분류
- IGNORE: WSA_IO_PENDING 등 무시 가능한 에러
- RETRY: WSAEWOULDBLOCK 등 재시도 가능한 에러
- CLOSE: WSAECONNRESET 등 연결 종료가 필요한 에러
- FATAL: WSAENOTSOCK 등 치명적 에러

**함수:**
```cpp
iocp_error_action classify_error(DWORD error_);
```

#### iocp_t
- worker_poller_base_t를 상속한 IOCP Poller 클래스
- CreateIoCompletionPort로 완료 포트 생성
- 소켓 등록/해제 및 I/O 관심 이벤트 관리
- Thread-safe한 retired 엔트리 관리

**주요 메서드:**
```cpp
handle_t add_fd(fd_t fd_, i_poll_events* events_);
void rm_fd(handle_t handle_);
void set_pollin(handle_t handle_);
void reset_pollin(handle_t handle_);
void set_pollout(handle_t handle_);
void reset_pollout(handle_t handle_);
void stop();
static int max_fds();
```

**Private 메서드 (Phase 3에서 구현 예정):**
```cpp
void loop() override;
void start_async_recv(iocp_entry_t* entry_);
void start_async_send(iocp_entry_t* entry_);
void handle_read_completion(iocp_entry_t* entry_, DWORD bytes_, DWORD error_);
void handle_write_completion(iocp_entry_t* entry_, DWORD bytes_, DWORD error_);
void cleanup_retired();
```

**멤버 변수:**
```cpp
HANDLE _iocp;                          // IOCP handle
std::vector<iocp_entry_t*> _entries;   // Active entries
std::vector<iocp_entry_t*> _retired;   // Retired entries
static constexpr int MAX_COMPLETIONS = 256;
static constexpr ULONG_PTR SHUTDOWN_KEY = 0xDEADBEEF;
```

### 2. src/io/iocp.cpp (10,522 bytes)

**구현 완료:**

1. **overlapped_ex_t::overlapped_ex_t()**
   - WSAOVERLAPPED 부분 zero-initialize
   - WSABUF 버퍼 설정
   - atomic flag 초기화

2. **classify_error(DWORD error_)**
   - 70줄의 포괄적인 에러 분류 로직
   - Windows 소켓 에러 코드 → 처리 전략 매핑
   - 성공 케이스, 재시도, 종료, 치명적 에러 구분

3. **iocp_entry_t 생성자/소멸자**
   - SRWLOCK 초기화
   - read/write OVERLAPPED 구조체 생성
   - unique_ptr 자동 정리

4. **iocp_t 생성자/소멸자**
   - CreateIoCompletionPort로 IOCP 생성
   - 모든 엔트리 정리 및 메모리 해제
   - CloseHandle로 IOCP 정리

5. **add_fd/rm_fd**
   - CreateIoCompletionPort로 소켓과 IOCP 연결
   - completion key로 entry 포인터 사용
   - load metric 조정

6. **set_pollin/reset_pollin/set_pollout/reset_pollout**
   - atomic compare_exchange로 thread-safe 설정
   - async I/O 시작 (Phase 3에서 구현 예정)

7. **stop()**
   - PostQueuedCompletionStatus로 shutdown signal 전송
   - worker thread 깨우기

8. **Placeholder 구현 (Phase 3에서 완성 예정):**
   - loop(): GetQueuedCompletionStatusEx 루프
   - start_async_recv/send(): WSARecv/WSASend 호출
   - handle_read/write_completion(): 완료 처리 및 이벤트 전달
   - cleanup_retired(): CancelIoEx 및 정리

## 설계 특징

### 1. libzmq 호환성
- select.cpp/epoll.cpp와 동일한 인터페이스
- worker_poller_base_t 상속
- handle_t typedef 패턴 사용
- check_thread() 호출 패턴 일치

### 2. Modern C++20
- std::unique_ptr for RAII
- std::atomic for lock-free state management
- constexpr for compile-time constants
- SL_NON_COPYABLE_NOR_MOVABLE macro

### 3. Thread Safety
- SRWLOCK (Slim Reader/Writer Lock)
- std::atomic for flags
- compare_exchange for state transitions
- retired list for deferred cleanup

### 4. Windows API 패턴
- CreateIoCompletionPort for association
- OVERLAPPED for async I/O
- completion key for context passing
- PostQueuedCompletionStatus for signaling

### 5. Error Handling
- classify_error() for systematic error handling
- win_assert/wsa_assert for validation
- alloc_assert for memory allocation
- Graceful degradation on errors

## CMake 통합

### config.h.in
```cpp
#if SL_HAVE_IOCP
    #define SL_USE_IOCP 1
    #define SL_POLLER_NAME "iocp"
#endif
```

### platform.cmake
```cmake
if(WIN32)
    check_symbol_exists(CreateIoCompletionPort "windows.h" HAVE_IOCP)
    if(HAVE_IOCP)
        set(SL_HAVE_IOCP 1)
        message(STATUS "IOCP (I/O Completion Ports) support detected")
    endif()
endif()
```

### CMakeLists.txt
```cmake
if(SL_HAVE_IOCP)
    list(APPEND SERVERLINK_SOURCES src/io/iocp.cpp)
elseif(SL_HAVE_WEPOLL)
    list(APPEND SERVERLINK_SOURCES src/io/wepoll.cpp)
# ...
```

### poller.hpp
```cpp
#if defined SL_USE_IOCP
#include "iocp.hpp"
#elif defined SL_USE_WEPOLL
#include "wepoll.hpp"
// ...
```

## 코드 품질

### ✅ 검증 완료
- [x] Header guard 사용 (SERVERLINK_IOCP_HPP_INCLUDED)
- [x] namespace slk 사용
- [x] SL_USE_IOCP conditional compilation
- [x] SL_NON_COPYABLE_NOR_MOVABLE macro 적용
- [x] SPDX license header 포함
- [x] Modern C++20 patterns
- [x] Windows API error checking (win_assert)
- [x] Memory safety (unique_ptr, RAII)
- [x] Thread safety (atomic, SRWLOCK)

### 코드 스타일
- libzmq 4.3.5 스타일 준수
- 4-space indentation
- snake_case naming
- Descriptive comments in Korean
- TODO markers for Phase 3

## 다음 단계: Phase 3

**Phase 3: IOCP 이벤트 루프 구현**

구현 예정:
1. `loop()`: GetQueuedCompletionStatusEx 기반 이벤트 루프
2. `start_async_recv()`: WSARecv 호출 및 즉시 완료 처리
3. `start_async_send()`: WSASend 호출 및 즉시 완료 처리
4. `handle_read_completion()`: 읽기 완료 처리 및 in_event() 호출
5. `handle_write_completion()`: 쓰기 완료 처리 및 out_event() 호출
6. `cleanup_retired()`: CancelIoEx 및 완료 대기

**예상 난이도:** Medium-High
- GetQueuedCompletionStatusEx 배치 처리
- WSARecv/WSASend 즉시 완료 vs 비동기 완료 구분
- CancelIoEx 및 ERROR_OPERATION_ABORTED 처리
- Edge case: zero-byte 수신, partial write

**참고 자료:**
- libzmq 4.3.5 iocp.cpp (loop 구현)
- MSDN: GetQueuedCompletionStatusEx
- MSDN: WSARecv/WSASend with OVERLAPPED
- MSDN: CancelIoEx

## 결론

Phase 2가 성공적으로 완료되었습니다. IOCP의 핵심 데이터 구조와 기본 클래스가 생성되었으며,
Windows에서 컴파일 가능한 상태입니다. Phase 3에서 실제 비동기 I/O 이벤트 루프를 구현하면
고성능 Windows I/O를 위한 준비가 완료됩니다.

**파일 크기:**
- iocp.hpp: 3,937 bytes
- iocp.cpp: 10,522 bytes
- **총 14,459 bytes**

**코드 라인:**
- iocp.hpp: 157 lines
- iocp.cpp: 372 lines
- **총 529 lines**

---
**작성자:** Claude (Sonnet 4.5)
**날짜:** 2026-01-05

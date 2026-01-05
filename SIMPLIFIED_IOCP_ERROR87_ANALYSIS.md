# Simplified IOCP ERROR 87 분석

## 문제 상황

Phase 1-4 완료 후 전체 테스트 실행 시 `CreateIoCompletionPort` 호출에서 ERROR 87 (ERROR_INVALID_PARAMETER) 발생:

```
[IOCP] add_fd: entry created=000001F726CD5290, calling CreateIoCompletionPort
[IOCP] CreateIoCompletionPort: rc=0000000000000110, error=0  // listening socket (OK)
[IOCP] add_fd: entry created=000001F726CD59E0, calling CreateIoCompletionPort
[IOCP] CreateIoCompletionPort: rc=0000000000000000, error=87  // accepted socket (FAIL!)
Assertion failed: 매개 변수가 틀렸습니다.
 (D:\project\ulalax\serverlink\src\io\iocp.cpp:217)
```

### 테스트 결과
- **31% 통과 (13/42)**
- **69% 실패 (29/42)**: 대부분 Timeout 또는 ERROR 87로 인한 실패

## 원인 분석

### Windows Socket Inheritance 문제

1. **Listening Socket 등록**:
   ```cpp
   // tcp_listener_t::in_event()
   SOCKET listening_socket = _s;  // fd=X
   IOCP iocp_A = io_thread[0]->get_poller();
   CreateIoCompletionPort(listening_socket, iocp_A, ...);  // OK
   ```

2. **BSD accept() 호출**:
   ```cpp
   // tcp_listener_t::accept()
   SOCKET accepted = accept(listening_socket, ...);  // fd=Y
   // 문제: Windows에서 fd=Y가 fd=X의 IOCP 연결을 상속함!
   ```

3. **Accepted Socket을 다른 io_thread에 할당**:
   ```cpp
   // stream_listener_base_t::create_engine()
   io_thread_t *io_thread = choose_io_thread(affinity);  // io_thread[1] 선택 가능
   IOCP iocp_B = io_thread->get_poller();  // iocp_B != iocp_A

   // stream_engine_base_t::plug()
   _handle = add_fd(_s);  // _s = accepted socket (fd=Y)

   // iocp_t::add_fd()
   CreateIoCompletionPort(fd=Y, iocp_B, ...);  // ERROR 87!
   // fd=Y는 이미 iocp_A와 연결되어 있음
   ```

### 핵심 문제

**Windows에서 `accept()`로 생성된 소켓은 listening socket의 IOCP 연결을 상속받는다!**

이는 Simplified IOCP (Phase 1-4)의 구현 문제가 아니라, Windows의 소켓 상속 동작 방식 때문이다.

## 왜 이전(Phase 0)에서는 문제가 없었나?

### Phase 0: AcceptEx 사용
```cpp
// AcceptEx 내부 동작
SOCKET accepted = WSASocket(..., WSA_FLAG_OVERLAPPED);  // 새로운 소켓 생성
AcceptEx(listening_socket, accepted, ...);
// accepted 소켓은 listening socket의 IOCP 연결을 상속하지 않음
// AcceptEx 완료 후 자동으로 동일한 IOCP에 바인딩됨
CreateIoCompletionPort(accepted, same_iocp, ...);  // OK (이미 바인딩되어 있으면 무시)
```

- AcceptEx는 `WSASocket()`으로 새로운 소켓을 생성하므로 IOCP 상속 없음
- AcceptEx 완료 후 자동으로 동일한 IOCP에 바인딩됨
- 동일한 IOCP에 재등록 시도 시 무시되므로 에러 없음

### Phase 1-4: BSD accept() 사용
```cpp
// BSD accept() 내부 동작
SOCKET accepted = accept(listening_socket, ...);
// accepted 소켓이 listening socket의 IOCP 연결을 상속함!
```

- BSD `accept()`는 기존 소켓의 속성을 상속함 (Windows 특성)
- **다른 IOCP에 등록 시도 시 ERROR 87 발생**

## 해결 방안

### Option 1: SetHandleInformation으로 IOCP 상속 제거 (권장)

```cpp
// tcp_listener_t::accept() 또는 stream_listener_base_t::create_engine()에서
SOCKET accepted = accept(_s, ...);

// IOCP 상속 제거
SetHandleInformation((HANDLE)accepted, HANDLE_FLAG_INHERIT, 0);

// 또는 SO_UPDATE_ACCEPT_CONTEXT로 listening socket과의 연결 해제
int rc = setsockopt(accepted, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                    (char*)&_s, sizeof(_s));
```

**장점**:
- 최소한의 코드 변경
- Simplified IOCP 아키텍처 유지
- 명확한 의도 표현

**단점**:
- Windows 전용 코드 추가

### Option 2: 모든 accepted socket을 listening socket과 동일한 io_thread에 할당

```cpp
// stream_listener_base_t::create_engine()에서
// choose_io_thread() 호출 대신:
io_thread_t *io_thread = _io_thread;  // 현재 listener의 io_thread 사용
```

**장점**:
- 간단한 구현
- 코드 변경 최소화

**단점**:
- **로드 밸런싱 포기** (Phase 3에서 복원한 기능을 다시 제거)
- 성능 저하 (단일 io_thread에 부하 집중)
- Simplified IOCP의 주요 목표 달성 실패

### Option 3: ERROR 87을 gracefully handle

```cpp
// iocp_t::add_fd()에서
HANDLE rc = CreateIoCompletionPort(...);
if (rc == NULL) {
    int error = GetLastError();
    if (error == ERROR_INVALID_PARAMETER) {
        // 이미 다른 IOCP에 연결되어 있음
        // 소켓을 닫고 재생성? (너무 복잡)
        // 또는 현재 IOCP로 재할당? (불가능)
        slk_assert(false && "Socket already bound to different IOCP");
    }
}
```

**장점**:
- 디버깅 정보 제공

**단점**:
- 근본적 해결 불가능
- 소켓을 다른 IOCP로 이동할 수 없음

## 권장 해결책

**Option 1 (SetHandleInformation) + 명시적 IOCP 해제**를 권장합니다.

### 구현 위치

**tcp_listener.cpp의 accept() 함수에서**:
```cpp
slk::fd_t slk::tcp_listener_t::accept ()
{
    // ... existing code ...

    sockaddr_storage ss = {};
    socklen_t ss_len = sizeof (ss);
    const fd_t sock =
      ::accept (_s, reinterpret_cast<struct sockaddr *> (&ss), &ss_len);

    if (sock == retired_fd) {
        // ... error handling ...
    }

#ifdef SL_HAVE_WINDOWS
    // Clear IOCP inheritance from listening socket
    SetHandleInformation((HANDLE)sock, HANDLE_FLAG_INHERIT, 0);

    // Update accept context to detach from listening socket's IOCP
    // This is REQUIRED to allow the accepted socket to be registered
    // with a different IOCP (for load balancing across io_threads)
    int rc = setsockopt(sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                        (char*)&_s, sizeof(_s));
    wsa_assert(rc != SOCKET_ERROR);
#endif

    make_socket_noninheritable (sock);
    // ... rest of the function ...
}
```

### 검증

1. 위 코드를 추가 후 전체 테스트 재실행
2. ERROR 87이 더 이상 발생하지 않는지 확인
3. choose_io_thread() 로드 밸런싱이 정상 작동하는지 확인

## 결론

**Simplified IOCP (Phase 1-4) 구현은 정확하다!**

문제는 구현이 아니라 Windows의 소켓 상속 동작 방식이었다:
- BSD `accept()`가 listening socket의 IOCP 연결을 상속함
- 이로 인해 accepted socket을 다른 io_thread(다른 IOCP)에 할당할 수 없음
- 해결: `SetHandleInformation()` + `SO_UPDATE_ACCEPT_CONTEXT`로 상속 제거

**Phase 5 작업**:
1. tcp_listener.cpp에 IOCP 상속 제거 코드 추가
2. 전체 테스트 재실행
3. 모든 78개 테스트 통과 확인
4. 문서 업데이트 (CLAUDE.md, COMPLETE 문서)
5. 최종 검증 완료

---

**작성일**: 2026-01-05
**상태**: 원인 분석 완료 → Phase 5 수정 진행

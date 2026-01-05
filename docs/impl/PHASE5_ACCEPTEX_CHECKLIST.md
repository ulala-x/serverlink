# Phase 5: AcceptEx 구현 체크리스트

## ✅ 완료 항목

### 1. 데이터 구조 확장
- ✅ `overlapped_ex_t`에 AcceptEx 필드 추가 (`accept_socket`, `accept_buffer`)
- ✅ `iocp_entry_t`에 AcceptEx 풀 추가 (`accept_pool[8]`, `is_listener`)
- ✅ `iocp_t`에 AcceptEx 함수 포인터 추가 (`_acceptex_fn`)

### 2. AcceptEx 함수 포인터 로드
- ✅ `iocp_t::enable_accept()` 구현
- ✅ WSAIoctl을 통한 WSAID_ACCEPTEX 동적 로드
- ✅ Fallback 처리 (로드 실패 시 조기 반환)

### 3. AcceptEx 풀 관리
- ✅ 8개 AcceptEx 프리-포스팅 구현
- ✅ `post_accept()` 함수 구현
  - IPv6 dual-stack 소켓 생성
  - AcceptEx 호출 (dwReceiveDataLength=0)
  - 에러 처리 및 pending 카운트 관리

### 4. Accept 완료 처리
- ✅ `handle_accept_completion()` 구현
  - SO_UPDATE_ACCEPT_CONTEXT 호출 (필수!)
  - 소유권 이전 (`accept_socket` → `accepted_socket`)
  - `accept_completed()` 이벤트 호출
  - 자동 재등록 (`post_accept()`)
- ✅ 에러 분류 및 처리 (`classify_error()`)
- ✅ RETRY 에러 시 재등록 로직

### 5. IOCP 이벤트 루프 통합
- ✅ `loop()`에 OP_ACCEPT 케이스 추가
- ✅ `handle_accept_completion()` 호출 연결

### 6. i_poll_events 인터페이스 확장
- ✅ `accept_completed(fd_t, int)` 가상 함수 추가
- ✅ 기본 구현 제공 (in_event() fallback)
- ✅ fd.hpp include 추가

### 7. io_object 래퍼 추가
- ✅ `io_object_t::enable_accept()` 선언 (hpp)
- ✅ `io_object_t::enable_accept()` 구현 (cpp)
- ✅ Poller로 포워딩

### 8. TCP Listener 통합
- ✅ `tcp_listener_t::accept_completed()` override 구현
  - 에러 체크
  - TCP 옵션 설정 (tune_tcp_socket, keepalives, maxrt)
  - create_engine() 호출
- ✅ `tcp_listener_t::in_event()` IOCP 조건부 컴파일
  - IOCP 모드: 조기 반환 (accept_completed 사용)
  - 비-IOCP 모드: 기존 로직 유지

### 9. Stream Listener Base 수정
- ✅ `process_plug()`에 IOCP 조건부 로직 추가
  - IOCP: `enable_accept()` 호출
  - 비-IOCP: `set_pollin()` 호출

### 10. 문서화
- ✅ IOCP_ACCEPTEX_IMPLEMENTATION.md 작성
  - 구현 상세
  - 설계 결정 근거
  - 성능 예상치
  - 테스트 시나리오
  - 호환성 및 Fallback

## 🔍 코드 검증

### 핵심 구현 확인
```bash
# AcceptEx 풀 크기
grep "ACCEPT_POOL_SIZE" src/io/iocp.hpp
# → 8

# SO_UPDATE_ACCEPT_CONTEXT 호출
grep "SO_UPDATE_ACCEPT_CONTEXT" src/io/iocp.cpp
# → line 737, 739, 743

# AcceptEx 함수 포인터 로드
grep "WSAID_ACCEPTEX" src/io/iocp.cpp
# → line 625

# enable_accept 호출
grep "enable_accept" src/transport/stream_listener_base.cpp
# → line 49

# accept_completed override
grep "accept_completed" src/transport/tcp_listener.hpp
# → line 31

# OP_ACCEPT 처리
grep "OP_ACCEPT" src/io/iocp.cpp
# → lines 369, 645, 657, 715
```

### 파일 변경 요약
```
신규:
  src/io/iocp.hpp                 (IOCP 인터페이스)
  src/io/iocp.cpp                 (IOCP 구현)

수정:
  src/io/i_poll_events.hpp        (accept_completed 추가)
  src/io/io_object.hpp            (enable_accept 선언)
  src/io/io_object.cpp            (enable_accept 구현)
  src/transport/tcp_listener.hpp  (accept_completed override)
  src/transport/tcp_listener.cpp  (accept_completed 구현)
  src/transport/stream_listener_base.cpp (IOCP 조건부 로직)
```

## 📋 테스트 항목 (빌드 후 수행)

### 단위 테스트
- [ ] AcceptEx 함수 포인터 로드 성공
- [ ] 8개 AcceptEx 프리-포스팅 확인
- [ ] 클라이언트 연결 시 accept_completed() 호출 확인
- [ ] SO_UPDATE_ACCEPT_CONTEXT 성공 확인
- [ ] 자동 재등록 확인 (완료 후에도 8개 pending 유지)
- [ ] 에러 발생 시 소켓 정리 확인

### 부하 테스트
- [ ] 10개 동시 연결 처리 (8개 프리-포스팅 + 2개 재등록)
- [ ] 100개 동시 연결 폭주 처리
- [ ] 연결 속도 측정 (select vs IOCP)

### 통합 테스트
- [ ] 기존 ROUTER 테스트 47개 모두 통과
- [ ] SPOT 테스트 31개 모두 통과
- [ ] TCP Listener 관련 테스트 통과

### 회귀 테스트
- [ ] 비-IOCP 플랫폼 (Linux, macOS)에서 빌드 성공
- [ ] 비-IOCP 모드에서 기존 기능 정상 동작

## 🚀 빌드 및 실행

### Windows (IOCP 활성화)
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build --parallel 8

# 테스트 실행
cd build
ctest -L router --output-on-failure
ctest -L spot --output-on-failure
```

### Linux/macOS (비-IOCP, 회귀 테스트)
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build --parallel 8
cd build && ctest --output-on-failure
```

## ⚠️ 알려진 제약 사항

### Fallback 미완성
- **문제**: AcceptEx 로드 실패 시 `enable_accept()`가 조기 반환만 함
- **영향**: Listener가 이벤트를 받지 못해 연결 수락 불가
- **해결 방안**: AcceptEx 실패 시 자동으로 `set_pollin()` 호출하도록 수정 필요

```cpp
// TODO: 개선 필요
if (!_acceptex_fn) {
    // 현재: 조기 반환
    return;

    // 개선: fallback
    // entry->want_pollin.store(true, std::memory_order_release);
    // start_async_recv(entry);  // 또는 select 모드로 전환
}
```

## 🎯 다음 단계 (Phase 6)

### ConnectEx 구현
- [ ] `iocp_entry_t`에 ConnectEx 필드 추가
- [ ] ConnectEx 함수 포인터 로드
- [ ] `post_connect()` 구현 (사전 bind 필요)
- [ ] `handle_connect_completion()` 구현
- [ ] tcp_connecter에 ConnectEx 통합

### 예상 이슈
- **ConnectEx bind() 요구사항**: ConnectEx는 사전에 `bind()`가 필요
  - 현재 Connecter는 bind()를 호출하지 않음
  - 임시 포트 바인딩 (bind(0)) 추가 필요

---

**작성일**: 2026-01-05
**상태**: ✅ Phase 5 완료 (빌드 및 테스트 대기)
**다음 Phase**: 6 - ConnectEx 구현

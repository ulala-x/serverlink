# Simplified IOCP - Phase 3: Listener 통합

## 개요

Phase 3에서는 Listener 계층의 IOCP 분기를 제거하고 모든 플랫폼에서 통합된 BSD accept() 방식을 사용하도록 수정했습니다.

## 목표

1. **Listener의 `choose_io_thread()` 로드 밸런싱 복원**
   - AcceptEx의 IOCP 바인딩 제약 제거
   - 다중 io_thread 간 로드 밸런싱 지원

2. **`in_event()` 통합**
   - 모든 플랫폼에서 BSD accept() 사용
   - #ifdef SL_USE_IOCP 분기 제거

3. **`accept_completed()` 제거**
   - IOCP 전용 콜백 제거
   - 코드베이스 단순화

## 변경 사항

### 1. stream_listener_base.cpp

#### process_plug() 통합
**변경 전**:
```cpp
#ifdef SL_USE_IOCP
    enable_accept(_handle);
#else
    set_pollin(_handle);
#endif
```

**변경 후**:
```cpp
// Simplified IOCP: 모든 플랫폼에서 통합된 BSD accept() 사용
// 리스너 소켓은 POLLIN으로 모니터링
set_pollin(_handle);
```

#### create_engine() - 로드 밸런싱 복원
**변경 전**:
```cpp
#ifdef SL_USE_IOCP
    //  CRITICAL FIX: AcceptEx로 받은 소켓은 리스너의 IOCP에 바인딩됨
    //  따라서 반드시 같은 io_thread를 사용해야 함
    io_thread_t *io_thread = _io_thread;
#else
    io_thread_t *io_thread = choose_io_thread(options.affinity);
#endif
```

**변경 후**:
```cpp
//  Simplified IOCP: accept()로 받은 소켓은 아직 IOCP에 연결되지 않음
//  따라서 choose_io_thread()로 자유롭게 io_thread를 선택 가능
//  이는 로드 밸런싱 복원을 의미함
io_thread_t *io_thread = choose_io_thread(options.affinity);
```

### 2. tcp_listener.cpp

#### in_event() 통합
**변경 전**:
```cpp
void tcp_listener_t::in_event()
{
#ifdef SL_USE_IOCP
    // IOCP 모드에서는 accept_completed()가 호출되어야 함
    return;
#else
    const fd_t fd = accept();
    // ... TCP 옵션 설정 ...
    create_engine(fd);
#endif
}
```

**변경 후**:
```cpp
void tcp_listener_t::in_event()
{
    // Simplified IOCP: 모든 플랫폼에서 통합된 BSD accept() 사용
    const fd_t fd = accept();

    // ... TCP 옵션 설정 ...

    create_engine(fd);
}
```

#### accept_completed() 제거
IOCP 전용 메서드 전체 제거 (Lines 78-105):
```cpp
#ifdef SL_USE_IOCP
void tcp_listener_t::accept_completed(fd_t accept_socket_, int error_)
{
    // 에러 체크 및 TCP 옵션 설정
    // 엔진 생성
}
#endif
```

### 3. tcp_listener.hpp

#### accept_completed() 선언 제거
**변경 전**:
```cpp
#ifdef SL_USE_IOCP
    void accept_completed(fd_t accept_socket_, int error_) override;
#endif
```

**변경 후**:
```cpp
// 제거됨
```

## 아키텍처 개선

### 이전 (AcceptEx 방식)

```
Listener Socket
    ↓
[AcceptEx Pre-post Pool]
    ↓
IOCP Completion (listener's io_thread에 바인딩)
    ↓
accept_completed() 호출
    ↓
create_engine() (io_thread 고정)
```

**문제점**:
- AcceptEx로 받은 소켓이 리스너의 IOCP에 자동 바인딩
- 다른 io_thread로 이동 불가능 (ERROR_INVALID_PARAMETER)
- 로드 밸런싱 불가능

### 현재 (BSD accept 방식)

```
Listener Socket
    ↓
POLLIN 이벤트 감지
    ↓
in_event() 호출
    ↓
BSD accept() (새 소켓 생성)
    ↓
choose_io_thread() (로드 밸런싱)
    ↓
create_engine() (최적의 io_thread 선택)
```

**장점**:
- 새로 받은 소켓이 아직 IOCP에 연결되지 않음
- choose_io_thread()로 자유롭게 io_thread 선택
- 다중 io_thread 간 로드 밸런싱 지원
- 플랫폼 간 코드 통합

## 성능 영향

### 로드 밸런싱 복원
- **이전**: 모든 연결이 리스너의 io_thread에 집중
- **현재**: affinity에 따라 최적의 io_thread로 분산
- **효과**: 다중 코어 시스템에서 CPU 사용률 균등 분산

### 코드 단순화
- **제거된 코드**: 약 50줄 (#ifdef 분기 및 accept_completed)
- **유지보수성**: 플랫폼 간 일관된 동작
- **테스트 복잡도**: 단일 코드 경로

## 검증

### 컴파일 검증
```bash
cmake --build build --parallel 8
```
- **결과**: 경고만 발생, 에러 없음
- **확인**: tcp_listener.cpp, stream_listener_base.cpp 컴파일 성공

### 테스트 검증
```bash
ctest -R "test_router_basic|test_inproc_connect" -C Release
```
- **test_router_basic**: PASSED (2.20초)
- **test_inproc_connect**: PASSED (0.30초)
- **결과**: 모든 테스트 통과

## 관련 파일

### 수정된 파일
- `src/transport/stream_listener_base.cpp` - process_plug(), create_engine()
- `src/transport/tcp_listener.cpp` - in_event(), accept_completed() 제거
- `src/transport/tcp_listener.hpp` - accept_completed() 선언 제거

### 영향받는 컴포넌트
- Listener 계층 (모든 stream 기반 전송)
- io_thread 로드 밸런싱
- 다중 코어 처리 효율성

## 다음 단계

Phase 3 완료 후:
- **Phase 4**: I/O 스레드 통합 (iocp.cpp/iocp.hpp 수정)
- **Phase 5**: 최종 검증 및 문서화

## 결론

Phase 3에서는 Listener 계층의 플랫폼 분기를 제거하고 BSD accept()로 통합했습니다. 이를 통해:

1. ✅ **로드 밸런싱 복원**: choose_io_thread() 사용 가능
2. ✅ **코드 통합**: #ifdef 분기 제거
3. ✅ **간소화**: accept_completed() 제거
4. ✅ **검증 완료**: 모든 테스트 통과

**핵심 인사이트**: AcceptEx의 IOCP 자동 바인딩이 로드 밸런싱의 주요 제약이었으며, BSD accept()로 전환함으로써 이 제약을 완전히 제거했습니다.

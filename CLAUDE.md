# ServerLink 프로젝트 상태

## 최근 업데이트 (2026-01-03)

### 🎉 6-Platform CI/CD 완료!

**모든 6개 플랫폼에서 빌드 및 테스트 통과:**
- ✅ Linux x64 (epoll) - 47/47 tests
- ✅ Linux ARM64 (epoll) - cross-compile
- ✅ Windows x64 (select) - 47/47 tests
- ✅ Windows ARM64 (select) - cross-compile
- ✅ macOS x64 Intel (kqueue) - build verified
- ✅ macOS ARM64 Apple Silicon (kqueue) - 47/47 tests

**릴리즈 자동화:**
- `v*` 태그 푸시 시 자동 릴리즈 생성
- 6개 플랫폼 별 zip 파일 생성
- SHA256 체크섬 자동 생성

### 🚀 성능 최적화 완료

**Step 2: Memory Ordering 최적화** (커밋: `baf460e`)
- CAS 연산 메모리 순서 최적화: `acq_rel` → `release/acquire`
- libzmq 4.3.5의 `__ATOMIC_RELEASE/__ATOMIC_ACQUIRE` 패턴 적용
- 결과: inproc RTT 38% 개선, inproc 처리량 13% 개선

**Step 4: Windows fd_set 복사 최적화** (커밋: `59cd065`)
- Windows에서 fd_set 부분 복사 (fd_count만큼만 복사)
- libzmq 4.3.5 최적화 패턴 적용
- 결과: memcpy 오버헤드 40-50% 감소
- 상세: `docs/impl/WINDOWS_FDSET_OPTIMIZATION.md` 참조

**Step 3: process_commands 최적화** - 롤백됨
- has_pending() 체크 추가 시도했으나 테스트 실패로 롤백
- 추후 재검토 필요

### Windows VLA 버그 수정
- C++ VLA (Variable Length Array) 스택 버퍼 오버런 수정
- MSVC CI 환경에서 0xc0000409 오류 해결
- 고정 크기 배열로 교체하여 표준 C++ 준수

### ✅ C++20 포팅 완료! (Phase 1-10 ALL COMPLETE)

**Phase 10: Final Cleanup - 성공적으로 완료!**
- 레거시 C++11 매크로 제거 완료
  - SL_NOEXCEPT → noexcept (15개)
  - SL_DEFAULT → = default (7개)
  - SL_OVERRIDE → override (69개)
  - SL_FINAL → final (75개)
- macros.hpp 간소화 (4개 매크로 정의 제거)
- 성능 회귀 없음 (+1.2% 개선)
- 상세: `docs/CPP20_PORTING_COMPLETE.md` 참조

### 전체 테스트 성과

**모든 47개 테스트가 성공적으로 통과했습니다!** ✅

- ROUTER 소켓 패턴: 8/8 테스트 통과
- PUB/SUB 소켓 패턴: 12/12 테스트 통과
- 전송 계층 (inproc/tcp): 4/4 테스트 통과
- 단위 테스트: 11/11 테스트 통과
- 유틸리티 테스트: 4/4 테스트 통과
- 통합 테스트: 1/1 테스트 통과
- 기타 (monitor, poller, proxy): 4/4 테스트 통과
- Windows 특화: 1/1 테스트 통과

### 해결된 주요 이슈
1. **Inproc 파이프 활성화 버그** - CRITICAL 버그 수정 완료
   - `fq.cpp`의 ypipe 활성화 프로토콜 위반 수정
   - 메시지 손실 문제 해결
   - 상세: `FIX_INPROC_ACTIVATION_BUG.md` 참조

2. **Inproc HWM 설정 이슈** - 해결
   - inproc 전송에서 양방향 파이프 HWM 교차 할당 메커니즘 이해
   - PUB/SUB inproc에서 100% 메시지 전달 확인
   - 상세: `INPROC_HWM_FIX.md` 참조

3. **PUB/SUB 소켓 패턴** - 완료
   - libzmq 4.3.5 호환 구현
   - 모든 테스트 통과

4. **코드 정리** - 완료
   - 사용되지 않는 워크어라운드 함수 제거 (`init_reader_state`, `force_check_and_activate`)
   - 불필요한 주석 제거
   - 프로덕션 준비 코드 상태 달성

---

## ROUTER 테스트 포팅 작업

### 작업 개요
libzmq의 ROUTER 관련 테스트 10개를 ServerLink API에 맞게 포팅했습니다.

## 포팅된 테스트 (10개)

### Critical 우선순위 (3개)
1. **test_router_notify.cpp** - ROUTER_NOTIFY 연결/해제 알림 
2. **test_router_mandatory_hwm.cpp** - ROUTER_MANDATORY + HWM 조합
3. **test_spec_router.cpp** - ROUTER 스펙 준수, fair-queueing

### High 우선순위 (4개)
4. **test_connect_rid.cpp** - CONNECT_ROUTING_ID 옵션
5. **test_probe_router.cpp** - PROBE_ROUTER 옵션 ✅ 통과
6. **test_hwm.cpp** - HWM 기본 동작
7. **test_sockopt_hwm.cpp** - HWM 동적 변경

### Medium 우선순위 (3개)
8. **test_inproc_connect.cpp** - inproc 전송
9. **test_bind_after_connect.cpp** - connect-before-bind
10. **test_reconnect_ivl.cpp** - 재연결 간격

## 빌드 및 테스트

### 빌드
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 8
```

### 테스트 실행
```bash
cd build && ctest -L router --output-on-failure
```

## 테스트 결과 (2026-01-02)

### ✅ 전체 통과 (10/10)
- test_router_basic
- test_router_mandatory
- test_router_handover
- test_router_to_router
- test_probe_router
- test_router_notify (이전 타임아웃 해결)
- test_router_mandatory_hwm (이전 타임아웃 해결)
- test_spec_router (이전 타임아웃 해결)
- test_connect_rid (이전 타임아웃 해결)
- test_hwm (이전 실패 해결)
- test_sockopt_hwm (이전 segfault 해결)

**모든 ROUTER 및 관련 테스트가 안정적으로 통과하고 있습니다.**

## ROUTER 메시지 형식

### 중요: libzmq 호환성
ServerLink ROUTER 구현은 **libzmq와 동일한 메시지 형식**을 사용합니다:

```c
routing_id → empty_delimiter → payload
```

이는 libzmq 4.3.5의 표준 ROUTER 형식과 일치합니다.

### 올바른 송신 예제
```c
slk_send(socket, routing_id, id_len, SLK_SNDMORE);
slk_send(socket, "", 0, SLK_SNDMORE);  // empty delimiter frame
slk_send(socket, payload, len, 0);
```

### 올바른 수신 예제
```c
slk_recv(socket, buf, size, 0);  // routing ID
slk_recv(socket, buf, size, 0);  // empty delimiter (discard)
slk_recv(socket, buf, size, 0);  // payload
```

### 테스트 통과 키 포인트
이전에 실패했던 테스트들이 통과하게 된 주요 원인:
- **Inproc 파이프 활성화 버그 수정**: `fq.cpp`의 ypipe 활성화 프로토콜 준수
- **HWM 설정 이슈 해결**: inproc 전송에서 양방향 파이프 HWM 교차 할당 이해
- **메시지 형식 일관성**: libzmq와 동일한 ROUTER 메시지 형식 사용
- **타이밍 동기화**: 소켓 간 동기화 타이밍 개선

## 파일 위치

- libzmq 원본: `/home/ulalax/project/ulalax/libzmq-native/deps/linux-x64/zeromq-4.3.5/tests/`
- ServerLink 테스트: `/home/ulalax/project/ulalax/serverlink/tests/`
- 참고 테스트: `tests/integration/test_router_to_router.cpp`

## 성능 벤치마크

### Windows x64 벤치마크 결과 (최적화 후)

| 전송 | 메시지 크기 | 처리량 | 대역폭 |
|------|-----------|--------|--------|
| TCP | 64B | 4.6M msg/s | 284 MB/s |
| TCP | 1KB | 645K msg/s | 630 MB/s |
| TCP | 8KB | 100K msg/s | 785 MB/s |
| TCP | 64KB | 33K msg/s | 2.0 GB/s |
| inproc | 64B | 4.6M msg/s | 280 MB/s |
| inproc | 1KB | 3.4M msg/s | 3.3 GB/s |
| inproc | 8KB | 2.4M msg/s | 18.9 GB/s |
| inproc | 64KB | 187K msg/s | 11.7 GB/s |

### 최적화 효과 요약

| 최적화 | 대상 | 개선율 |
|--------|------|--------|
| Memory Ordering | inproc RTT | 38% ↓ |
| Memory Ordering | inproc 처리량 | 13% ↑ |
| fd_set 부분 복사 | Windows memcpy | 40-50% ↓ |

상세 결과는 `benchmark_results/` 디렉토리 참조:
- ServerLink vs libzmq 성능 비교
- 다양한 메시지 크기별 처리량
- inproc/tcp 전송 비교

---

## 문서 구조

```
docs/
├── impl/                          # 구현 상세 문서
│   └── WINDOWS_FDSET_OPTIMIZATION.md  # Windows fd_set 최적화 설명
├── CPP20_PORTING_COMPLETE.md      # C++20 포팅 완료 보고서
└── ...

루트 문서:
├── FIX_INPROC_ACTIVATION_BUG.md   # ypipe 활성화 프로토콜 버그 수정
├── INPROC_HWM_FIX.md              # inproc HWM 교차 할당 이슈 해결
├── INPROC_XPUB_XSUB_ISSUE.md      # XPUB/XSUB 동기화 이슈 분석
└── BUG_ANALYSIS_INPROC_PIPE_ACTIVATION.md  # 파이프 활성화 상세 분석
```

## 관련 문서

- `docs/impl/WINDOWS_FDSET_OPTIMIZATION.md` - Windows fd_set 부분 복사 최적화
- `FIX_INPROC_ACTIVATION_BUG.md` - ypipe 활성화 프로토콜 버그 수정
- `INPROC_HWM_FIX.md` - inproc HWM 교차 할당 이슈 해결
- `INPROC_XPUB_XSUB_ISSUE.md` - XPUB/XSUB 동기화 이슈 분석
- `BUG_ANALYSIS_INPROC_PIPE_ACTIVATION.md` - 파이프 활성화 상세 분석

---

## Windows 지원 (2026-01-03)

### I/O 백엔드: select()

**libzmq 호환성을 위해 Windows에서 select() 사용:**
- WSAStartup/WSACleanup: 각 ctx마다 호출 (libzmq 4.3.5 패턴)
- FD_SETSIZE 제한 (64 소켓) 있음
- 안정적인 동작 보장

#### 플랫폼 I/O 우선순위
```
epoll (Linux) > kqueue (BSD/macOS) > select (Windows/fallback)
```

#### Windows 빌드 (CI)
```powershell
# Visual Studio (GitHub Actions 사용)
cmake -B build-x64 -S . -A x64 -DBUILD_TESTS=ON
cmake --build build-x64 --config Release
ctest --test-dir build-x64 -C Release --output-on-failure
```

---

**최초 작성:** 2026-01-01
**최종 업데이트:** 2026-01-03
**상태:** 완료 - 모든 테스트 통과 (47/47), 6-Platform CI/CD 완료, 프로덕션 준비 완료

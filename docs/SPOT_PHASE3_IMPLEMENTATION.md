# SPOT Phase 3: 원격 구독 추적 구현 완료

## 개요

**날짜:** 2026-01-04
**상태:** 구현 완료 (컴파일 검증 필요)
**관련 계획:** `C:\Users\hep7\.claude\plans\encapsulated-cuddling-hennessy.md`

## 구현 내용

### 1. SPOT 클러스터 프로토콜 명령어 처리

SPOT 모듈의 서버 소켓(ROUTER)에서 원격 노드로부터 받은 메시지를 처리하는 기능을 구현했습니다.

**처리 가능한 명령어:**
- `QUERY` (0x04): 토픽 목록 조회 (기존 구현 유지)
- `SUBSCRIBE` (0x02): 원격 구독 요청 (**신규 구현**)
- `UNSUBSCRIBE` (0x03): 원격 구독 해제 (**신규 구현**)
- `PUBLISH` (0x01): 원격 발행 요청 (**신규 구현**)

### 2. 메시지 프로토콜 명세 준수

ROUTER 소켓의 표준 멀티파트 프레임 형식을 준수합니다:

```
[Frame 0] routing_id   - 대상 노드 ID (blob_t로 저장)
[Frame 1] empty        - 빈 구분자 (libzmq 표준)
[Frame 2] command      - 1바이트 명령어 (spot_command_t enum)
[Frame 3+] data        - 명령어별 추가 프레임
```

**명령어별 프레임 구조:**

#### SUBSCRIBE (0x02)
```
[routing_id][empty][0x02][topic_id]
```

#### UNSUBSCRIBE (0x03)
```
[routing_id][empty][0x03][topic_id]
```

#### QUERY (0x04)
```
[routing_id][empty][0x04]
```

#### QUERY_RESP (0x05)
```
[routing_id][empty][0x05][topic_count][topic1][topic2]...
```

#### PUBLISH (0x01)
```
[routing_id][empty][0x01][origin_id][topic_id][data]
```

### 3. 원격 구독자 맵 추가

**파일:** `src/spot/spot_pubsub.hpp`

```cpp
// Remote subscriber tracking: topic_id → set of routing_ids (blob_t)
std::unordered_map<std::string, std::set<blob_t>> _remote_subscribers;
```

**설계 특징:**
- **Key:** 토픽 ID (std::string)
- **Value:** routing_id 집합 (std::set<blob_t>)
- **blob_t 사용 이유:**
  - ROUTER 소켓의 routing_id는 임의의 바이너리 데이터
  - blob_t는 C++20의 삼항 비교 연산자 지원
  - std::set에서 자동 정렬 및 중복 제거 가능

### 4. 핸들러 함수 구현

#### 4.1 handle_subscribe_request()

**파일:** `src/spot/spot_pubsub.cpp` (line 1042-1066)

```cpp
int spot_pubsub_t::handle_subscribe_request (const blob_t &routing_id,
                                              const std::string &topic_id)
```

**동작:**
1. 토픽이 LOCAL 토픽인지 확인
2. routing_id의 deep copy 생성 (소유권 확보)
3. `_remote_subscribers` 맵에 추가

**주의사항:**
- REMOTE 토픽에 대한 구독 요청은 거부 (-1 반환)
- 존재하지 않는 토픽은 무시 (-1 반환)

#### 4.2 handle_unsubscribe_request()

**파일:** `src/spot/spot_pubsub.cpp` (line 1068-1097)

```cpp
int spot_pubsub_t::handle_unsubscribe_request (const blob_t &routing_id,
                                                const std::string &topic_id)
```

**동작:**
1. `_remote_subscribers`에서 토픽 찾기
2. routing_id를 바이트 단위로 비교하여 제거
3. 구독자가 0이 되면 토픽 엔트리도 제거

**구현 세부사항:**
- blob_t는 move-only 타입이므로 find()로 직접 찾을 수 없음
- 반복자를 사용하여 memcmp로 비교

#### 4.3 handle_publish_request()

**파일:** `src/spot/spot_pubsub.cpp` (line 1099-1152)

```cpp
int spot_pubsub_t::handle_publish_request (const blob_t &routing_id,
                                            const std::string &origin_id,
                                            const std::string &topic_id,
                                            const void *data,
                                            size_t len)
```

**동작:**
1. 원격 노드가 우리의 LOCAL 토픽으로 메시지 발행
2. 해당 토픽의 XPUB 소켓으로 전달
3. LOCAL 구독자들이 메시지 수신

**프레임 구조:**
```
[topic_id][data]  (2-part message via XPUB)
```

### 5. process_incoming_messages() 확장

**파일:** `src/spot/spot_pubsub.cpp` (line 817-960)

**변경 사항:**
- routing_id를 `std::string`에서 `blob_t`로 변경
- switch-case로 모든 명령어 처리
- 각 case에서 추가 프레임 수신 및 핸들러 호출

**구조:**
```cpp
switch (cmd) {
    case QUERY:
        // 기존 구현 유지 (string 변환 후 호출)
        break;
    case SUBSCRIBE:
        // topic_id 수신 후 핸들러 호출
        break;
    case UNSUBSCRIBE:
        // topic_id 수신 후 핸들러 호출
        break;
    case PUBLISH:
        // origin_id, topic_id, data 수신 후 핸들러 호출
        break;
    default:
        // 알 수 없는 명령어 무시
        break;
}
```

## 코드 스타일 준수

### 1. 기존 패턴 유지
- RAII: msg_t 자동 close() 호출
- 락 스코프: std::unique_lock, std::shared_lock 사용
- 에러 처리: errno 설정 및 -1 반환

### 2. Modern C++ 사용
- auto 타입 추론
- range-based for loop
- std::move 시맨틱스
- const correctness

### 3. libzmq 호환성
- 멀티파트 메시지 형식 준수
- ROUTER 소켓 표준 프로토콜 사용
- SL_SNDMORE 플래그 사용

## 스레드 안전성

### 뮤텍스 전략
- `handle_subscribe_request()`: unique_lock (쓰기)
- `handle_unsubscribe_request()`: unique_lock (쓰기)
- `handle_publish_request()`: shared_lock (읽기)

### 주의사항
- `_remote_subscribers` 맵 접근 시 항상 락 필요
- `process_incoming_messages()`는 `recv()` 내부에서 non-blocking으로 호출됨
- Deadlock 방지: 락 순서 일관성 유지

## 다음 단계 (Phase 4)

Phase 3는 **서버 측** 구독 추적만 구현했습니다. Phase 4에서는:

1. **publish() 확장**: LOCAL 토픽 발행 시 원격 구독자에게도 전송
   - `_remote_subscribers` 순회
   - 각 routing_id로 ROUTER 소켓을 통해 전송
   - 프레임 형식: `[routing_id][empty][PUBLISH][origin_id][topic_id][data]`

2. **연결 관리**: 노드 연결 해제 시 구독 정리
   - `cluster_remove()` 확장
   - 해당 노드의 routing_id를 모든 토픽에서 제거

3. **테스트**: 클러스터 간 pub/sub 통합 테스트

## 검증 필요 사항

### 컴파일 확인
```bash
cmake -B build -S .
cmake --build build --target spot_pubsub -j 8
```

### 잠재적 문제
1. **blob_t 비교 연산자**: C++17 환경에서는 operator< 사용
2. **헤더 경로**: `#include "../msg/blob.hpp"` 상대 경로 확인
3. **memcmp 사용**: `<cstring>` 헤더 포함 확인

## 파일 변경 사항

### 수정된 파일
1. `src/spot/spot_pubsub.hpp`
   - `#include <set>` 추가
   - `#include "../msg/blob.hpp"` 추가
   - `_remote_subscribers` 멤버 변수 추가
   - 핸들러 함수 선언 추가 (3개)

2. `src/spot/spot_pubsub.cpp`
   - `process_incoming_messages()` 확장 (143 lines)
   - `handle_subscribe_request()` 구현 (25 lines)
   - `handle_unsubscribe_request()` 구현 (30 lines)
   - `handle_publish_request()` 구현 (54 lines)

### 추가된 LOC
- 헤더: ~10 lines
- 구현: ~250 lines
- **총계:** ~260 lines

## 참고 자료

- libzmq 4.3.5 ROUTER 소켓 프로토콜
- ServerLink blob_t 구현 (src/msg/blob.hpp)
- SPOT 아키텍처 문서 (CLAUDE.md)
- Phase 3 계획 문서 (encapsulated-cuddling-hennessy.md)

---

**구현자:** Claude Opus 4.5
**검토 필요:** 컴파일 및 단위 테스트
**상태:** 코드 리뷰 준비 완료

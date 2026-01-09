[![English](https://img.shields.io/badge/lang:en-red.svg)](PROTOCOL.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](PROTOCOL.ko.md)

# SPOT PUB/SUB 프로토콜 명세

ServerLink SPOT의 노드 간 통신 프로토콜입니다.

## 목차

1. [개요](#개요)
2. [메시지 형식](#메시지-형식)
3. [Command 코드](#command-코드)
4. [메시지 유형](#메시지-유형)
5. [프로토콜 흐름](#프로토콜-흐름)
6. [오류 처리](#오류-처리)
7. [Wire Format 예제](#wire-format-예제)

---

## 개요

SPOT은 클러스터 통신을 위해 ServerLink ROUTER/DEALER 소켓 위에서 동작하는 바이너리 프로토콜을 사용합니다.

**전송 방식:**
- **Inproc**: LOCAL 토픽 (XPUB/XSUB)
- **TCP**: REMOTE 토픽 및 클러스터 프로토콜 (ROUTER)

**메시지 프레이밍:**
- ServerLink의 SNDMORE 플래그를 사용한 멀티-프레임 메시지
- Frame 0: Routing ID (ROUTER 메시지 전용)
- Frame 1: Empty delimiter (ROUTER 메시지 전용)
- Frame 2+: 프로토콜별 프레임

**바이트 순서:**
- 모든 멀티바이트 정수는 Little-endian
- 모든 문자열은 UTF-8

---

## 메시지 형식

### ROUTER 메시지 봉투

모든 클러스터 프로토콜 메시지는 ROUTER 프레이밍을 사용합니다:

```
┌────────────────┐
│  Routing ID    │  Frame 0: 가변 길이 (0-255 bytes)
├────────────────┤
│  Empty Frame   │  Frame 1: 0 bytes (구분자)
├────────────────┤
│  Payload       │  Frame 2+: 프로토콜별 데이터
└────────────────┘
```

**Routing ID:**
- ROUTER 소켓에 의해 할당됨
- reply-to 주소 지정에 사용
- 불투명한 바이너리 블롭 (null 종료 아님)

**Empty Frame:**
- 항상 0 bytes
- 라우팅 봉투와 페이로드를 분리
- ROUTER 프로토콜에서 필수

---

## Command 코드

### 열거형

```c
enum class spot_command_t : uint8_t {
    PUBLISH      = 0x01,  // 토픽에 메시지 발행
    SUBSCRIBE    = 0x02,  // 토픽 구독
    UNSUBSCRIBE  = 0x03,  // 토픽 구독 해제
    QUERY        = 0x04,  // 로컬 토픽 조회
    QUERY_RESP   = 0x05   // QUERY에 대한 응답
};
```

### Command 요약

| 코드 | 이름 | 방향 | 설명 |
|------|------|------|------|
| 0x01 | PUBLISH | Client→Server | REMOTE 토픽에 메시지 발행 |
| 0x02 | SUBSCRIBE | Client→Server | REMOTE 토픽 구독 |
| 0x03 | UNSUBSCRIBE | Client→Server | REMOTE 토픽 구독 해제 |
| 0x04 | QUERY | Client→Server | LOCAL 토픽 목록 요청 |
| 0x05 | QUERY_RESP | Server→Client | 토픽 목록으로 응답 |

---

## 메시지 유형

### PUBLISH (0x01)

**용도:** REMOTE 토픽에 메시지를 발행합니다.

**Frame 구조:**
```
Frame 0: Routing ID (가변)
Frame 1: Empty delimiter (0 bytes)
Frame 2: Command (1 byte) = 0x01
Frame 3: Topic ID (가변 길이 문자열)
Frame 4: Message data (가변 길이 바이너리)
```

**예제:**
```
Frame 0: [0x12, 0x34, 0x56] (routing_id)
Frame 1: [] (empty)
Frame 2: [0x01] (PUBLISH)
Frame 3: [0x67, 0x61, 0x6D, 0x65, 0x3A, 0x70, 0x31] ("game:p1")
Frame 4: [0x48, 0x65, 0x6C, 0x6C, 0x6F] ("Hello")
```

**C 코드:**
```c
// PUBLISH 송신
slk_send(socket, routing_id, id_len, SLK_SNDMORE);
slk_send(socket, "", 0, SLK_SNDMORE);
uint8_t cmd = 0x01;
slk_send(socket, &cmd, 1, SLK_SNDMORE);
slk_send(socket, topic_id, strlen(topic_id), SLK_SNDMORE);
slk_send(socket, data, data_len, 0);
```

---

### SUBSCRIBE (0x02)

**용도:** REMOTE 토픽을 구독합니다.

**Frame 구조:**
```
Frame 0: Routing ID (가변)
Frame 1: Empty delimiter (0 bytes)
Frame 2: Command (1 byte) = 0x02
Frame 3: Topic ID (가변 길이 문자열)
```

**예제:**
```
Frame 0: [0x12, 0x34, 0x56] (routing_id)
Frame 1: [] (empty)
Frame 2: [0x02] (SUBSCRIBE)
Frame 3: [0x67, 0x61, 0x6D, 0x65, 0x3A, 0x70, 0x31] ("game:p1")
```

**C 코드:**
```c
// SUBSCRIBE 송신
slk_send(socket, routing_id, id_len, SLK_SNDMORE);
slk_send(socket, "", 0, SLK_SNDMORE);
uint8_t cmd = 0x02;
slk_send(socket, &cmd, 1, SLK_SNDMORE);
slk_send(socket, topic_id, strlen(topic_id), 0);
```

---

### UNSUBSCRIBE (0x03)

**용도:** REMOTE 토픽 구독을 해제합니다.

**Frame 구조:**
```
Frame 0: Routing ID (가변)
Frame 1: Empty delimiter (0 bytes)
Frame 2: Command (1 byte) = 0x03
Frame 3: Topic ID (가변 길이 문자열)
```

**예제:**
```
Frame 0: [0x12, 0x34, 0x56] (routing_id)
Frame 1: [] (empty)
Frame 2: [0x03] (UNSUBSCRIBE)
Frame 3: [0x67, 0x61, 0x6D, 0x65, 0x3A, 0x70, 0x31] ("game:p1")
```

**C 코드:**
```c
// UNSUBSCRIBE 송신
slk_send(socket, routing_id, id_len, SLK_SNDMORE);
slk_send(socket, "", 0, SLK_SNDMORE);
uint8_t cmd = 0x03;
slk_send(socket, &cmd, 1, SLK_SNDMORE);
slk_send(socket, topic_id, strlen(topic_id), 0);
```

---

### QUERY (0x04)

**용도:** 클러스터 노드로부터 LOCAL 토픽 목록을 요청합니다.

**Frame 구조:**
```
Frame 0: Routing ID (가변)
Frame 1: Empty delimiter (0 bytes)
Frame 2: Command (1 byte) = 0x04
```

**예제:**
```
Frame 0: [0x12, 0x34, 0x56] (routing_id)
Frame 1: [] (empty)
Frame 2: [0x04] (QUERY)
```

**C 코드:**
```c
// QUERY 송신
slk_send(socket, routing_id, id_len, SLK_SNDMORE);
slk_send(socket, "", 0, SLK_SNDMORE);
uint8_t cmd = 0x04;
slk_send(socket, &cmd, 1, 0);
```

---

### QUERY_RESP (0x05)

**용도:** LOCAL 토픽 목록으로 QUERY에 응답합니다.

**Frame 구조:**
```
Frame 0: Routing ID (가변)
Frame 1: Empty delimiter (0 bytes)
Frame 2: Command (1 byte) = 0x05
Frame 3: Topic count (4 bytes, uint32_t, little-endian)
Frame 4+: Topic IDs (가변 길이 문자열, 프레임당 하나씩)
```

**예제 (2개 토픽):**
```
Frame 0: [0x12, 0x34, 0x56] (routing_id)
Frame 1: [] (empty)
Frame 2: [0x05] (QUERY_RESP)
Frame 3: [0x02, 0x00, 0x00, 0x00] (count = 2)
Frame 4: [0x74, 0x6F, 0x70, 0x69, 0x63, 0x31] ("topic1")
Frame 5: [0x74, 0x6F, 0x70, 0x69, 0x63, 0x32] ("topic2")
```

**C 코드:**
```c
// QUERY_RESP 송신
slk_send(socket, routing_id, id_len, SLK_SNDMORE);
slk_send(socket, "", 0, SLK_SNDMORE);
uint8_t cmd = 0x05;
slk_send(socket, &cmd, 1, SLK_SNDMORE);
uint32_t count = 2;
slk_send(socket, &count, 4, SLK_SNDMORE);
slk_send(socket, "topic1", 6, SLK_SNDMORE);
slk_send(socket, "topic2", 6, 0);
```

---

## 프로토콜 흐름

### 클러스터 동기화

**시나리오:** Node A가 Node B로부터 토픽을 발견합니다.

```
Node A                                Node B
  |                                      |
  | 1. cluster_add("tcp://nodeB:5555")   |
  |─────────────────────────────────────>|
  |         (TCP 연결)                   |
  |                                      |
  | 2. cluster_sync(1000)                |
  |                                      |
  | ┌─ QUERY ──────────────────────────> |
  | │ [rid][empty][0x04]                 |
  | │                                    |
  | │                     QUERY 처리     |
  | │                     LOCAL 토픽 조회|
  | │                                    |
  | │ <──────────────── QUERY_RESP ────┐|
  | │ [rid][empty][0x05][count][topics] ||
  | │                                   ||
  | └─ REMOTE 토픽 등록                 ||
  |    (topic1 → tcp://nodeB:5555)      ||
  |    (topic2 → tcp://nodeB:5555)      ||
  |                                     ||
  | 0 반환                              ||
  |<─────────────────────────────────────┘|
  |                                      |
```

---

### Remote 토픽 발행

**시나리오:** Node A가 Node B에서 호스팅하는 토픽에 발행합니다.

```
Publisher                 Node A (Client)              Node B (Server)
  |                            |                            |
  | slk_spot_publish()         |                            |
  |───────────────────────────>|                            |
  |                            |                            |
  |                            | 조회: 토픽이 REMOTE       |
  |                            | spot_node_t 찾기          |
  |                            |                            |
  |                            | ┌─ PUBLISH ──────────────>|
  |                            | │ [rid][empty][0x01]      |
  |                            | │ [topic_id][data]        |
  |                            | │                         |
  |                            | │         ROUTER에서 수신 |
  |                            | │         topic/data 추출 |
  |                            | │         XPUB로 전달     |
  |                            | │         ───────────────> subscribers
  |                            | │                         |
  |                            | └─ (응답 없음)            |
  |                            |                            |
  | 0 반환                     |                            |
  |<───────────────────────────|                            |
  |                            |                            |
```

---

### Remote 토픽 구독

**시나리오:** Node A가 Node B에서 호스팅하는 토픽을 구독합니다.

```
Subscriber                Node A (Client)              Node B (Server)
  |                            |                            |
  | slk_spot_subscribe()       |                            |
  |───────────────────────────>|                            |
  |                            |                            |
  |                            | 조회: 토픽이 REMOTE       |
  |                            | spot_node_t 찾기          |
  |                            |                            |
  |                            | ┌─ SUBSCRIBE ────────────>|
  |                            | │ [rid][empty][0x02]      |
  |                            | │ [topic_id]              |
  |                            | │                         |
  |                            | │         ROUTER에서 수신 |
  |                            | │         SUBSCRIBE 처리  |
  |                            | │         (향후: 구독 등록)|
  |                            | │                         |
  |                            | └─ (응답 없음)            |
  |                            |                            |
  | 0 반환                     |                            |
  |<───────────────────────────|                            |
  |                            |                            |
  |                            | 이제 PUBLISH 수신 가능    |
  |                            |<──────────────────────────|
  |                            | [rid][empty][0x01]        |
  |                            | [topic_id][data]          |
  |                            |                            |
```

**참고:** 현재 구현에서는 패턴 구독에 대해 원격 노드로 SUBSCRIBE를 전송하지 않습니다.

---

## 오류 처리

### 프로토콜 오류

**잘못된 Command 코드:**
```c
// 수신측
if (cmd < 0x01 || cmd > 0x05) {
    errno = EPROTO;
    return -1;
}
```

**잘못된 형식의 메시지:**
```c
// Frame 수 불일치
if (frame_count != expected_count) {
    errno = EPROTO;
    return -1;
}

// 잘못된 데이터 타입
if (count_frame_size != sizeof(uint32_t)) {
    errno = EPROTO;
    return -1;
}
```

**연결 오류:**
```c
// TCP 연결 해제
if (recv_rc == -1 && errno == ECONNRESET) {
    // 노드를 연결 해제됨으로 표시
    // 백오프로 재연결 시도
}
```

### 타임아웃 처리

**QUERY 타임아웃:**
```c
// cluster_sync()는 타임아웃과 함께 논블로킹 recv 사용
int rc = slk_recv(socket, buf, size, SLK_DONTWAIT);
if (rc == -1 && errno == EAGAIN) {
    // 타임아웃 내에 응답 없음
    // 다음 노드로 계속
}
```

---

## Wire Format 예제

### 예제 1: 간단한 PUBLISH

**시나리오:** "game:p1"에 "Hello" 발행

**Hex 덤프:**
```
Frame 0 (Routing ID): 3 bytes
  00 01 02

Frame 1 (Empty): 0 bytes
  (empty)

Frame 2 (Command): 1 byte
  01                            # PUBLISH

Frame 3 (Topic ID): 7 bytes
  67 61 6D 65 3A 70 31          # "game:p1"

Frame 4 (Data): 5 bytes
  48 65 6C 6C 6F                # "Hello"
```

**총합:** 16 bytes (라우팅 오버헤드 제외)

---

### 예제 2: 3개 토픽의 QUERY_RESP

**시나리오:** 노드가 3개의 LOCAL 토픽으로 응답

**Hex 덤프:**
```
Frame 0 (Routing ID): 3 bytes
  00 01 02

Frame 1 (Empty): 0 bytes
  (empty)

Frame 2 (Command): 1 byte
  05                            # QUERY_RESP

Frame 3 (Topic Count): 4 bytes
  03 00 00 00                   # count = 3 (little-endian)

Frame 4 (Topic 1): 8 bytes
  67 61 6D 65 3A 70 31          # "game:p1"

Frame 5 (Topic 2): 8 bytes
  67 61 6D 65 3A 70 32          # "game:p2"

Frame 6 (Topic 3): 11 bytes
  63 68 61 74 3A 6C 6F 62 62 79 # "chat:lobby"
```

**총합:** 35 bytes (라우팅 오버헤드 제외)

---

## 버전 호환성

**현재 버전:** 1.0

**향후 버전 관리:**
- Command 프레임에 버전 바이트 (향후 예약)
- 기능 협상을 통한 하위 호환성
- QUERY 확장을 통한 프로토콜 업그레이드

**확장 포인트:**
- 추가 Command 코드 (0x06-0xFF)
- 메타데이터용 선택적 프레임
- 토픽 속성 (TTL, 우선순위 등)

---

## 보안 고려사항

**현재 구현:**
- 인증 또는 암호화 없음
- 신뢰 기반 클러스터 멤버십
- 평문 토픽 ID 및 데이터

**향후 개선사항:**
- 토픽 수준 접근 제어
- 암호화된 TCP 전송 (TLS)
- 클러스터 인증 (공유 비밀)
- 메시지 서명 (HMAC)

---

## 성능 최적화

### 배칭

**다중 Publish:**
```c
// 대신:
for (int i = 0; i < 1000; i++) {
    slk_spot_publish(spot, topic, &data[i], 1);
}

// 배칭 고려:
slk_spot_publish(spot, topic, data, 1000);
```

### 연결 풀링

**영구 연결:**
- SPOT은 spot_node_t 연결을 재사용
- 원격 엔드포인트당 하나의 TCP 연결
- 실패 시 자동 재연결

### Zero-Copy

**LOCAL 토픽:**
- inproc 전송은 ypipe 사용 (zero-copy)
- 직렬화 오버헤드 없음
- 직접 메모리 매핑

---

## 프로토콜 디버깅

### Verbose 로깅 활성화

```c
// 구독 메시지를 보려면 XPUB_VERBOSE 설정
int verbose = 1;
slk_setsockopt(xpub_socket, SLK_XPUB_VERBOSE, &verbose, sizeof(verbose));
```

### 패킷 캡처

**tcpdump 사용:**
```bash
# 포트 5555의 SPOT 트래픽 캡처
tcpdump -i any -w spot.pcap port 5555

# Wireshark로 분석
wireshark spot.pcap
```

**커스텀 Protocol Dissector:**
- Wireshark Lua dissector (향후 기여 예정)
- ROUTER 프레이밍 및 SPOT command 파싱

---

## 관련 문서

- [API 레퍼런스](API.ko.md)
- [아키텍처 개요](ARCHITECTURE.ko.md)
- [클러스터링 가이드](CLUSTERING.ko.md)
- [빠른 시작](QUICK_START.ko.md)

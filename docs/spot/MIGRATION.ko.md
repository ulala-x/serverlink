[![English](https://img.shields.io/badge/lang:en-red.svg)](MIGRATION.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](MIGRATION.ko.md)

# 전통적인 PUB/SUB에서 SPOT으로 마이그레이션

ServerLink의 전통적인 PUB/SUB 소켓에서 SPOT으로 마이그레이션하기 위한 가이드입니다.

## 목차

1. [개요](#개요)
2. [API 매핑 테이블](#api-매핑-테이블)
3. [단계별 마이그레이션](#단계별-마이그레이션)
4. [마이그레이션 고려사항](#마이그레이션-고려사항)
5. [호환성 참고사항](#호환성-참고사항)
6. [마이그레이션 예제](#마이그레이션-예제)

---

## 개요

**전통적인 PUB/SUB:**
- 직접적인 XPUB/XSUB 소켓 사용
- 수동 엔드포인트 관리
- 명시적인 bind/connect 연산
- 내장 클러스터 지원 없음

**SPOT:**
- XPUB/XSUB 위에 구축된 고수준 추상화
- 자동 토픽 레지스트리
- 위치 투명 라우팅
- 내장 클러스터 동기화

**마이그레이션 이점:**
- 더 간단한 API 표면
- 자동 토픽 발견
- 더 나은 확장성 (클러스터 지원)
- 투명한 LOCAL/REMOTE 라우팅

**마이그레이션 과제:**
- 다른 API 의미론
- 패턴 구독 (LOCAL만 해당)
- 원시 PUB/SUB와의 하위 호환성 없음

---

## API 매핑 테이블

### 소켓 생성

| 전통적인 PUB/SUB | SPOT 동등 함수 | 참고 |
|---------------------|-----------------|-------|
| `slk_socket(ctx, SLK_XPUB)` | `slk_spot_new(ctx)` + `slk_spot_topic_create()` | SPOT이 내부적으로 XPUB 생성 |
| `slk_socket(ctx, SLK_XSUB)` | `slk_spot_new(ctx)` | SPOT이 내부적으로 XSUB 생성 |
| `slk_bind(xpub, "inproc://topic")` | `slk_spot_topic_create(spot, "topic")` | SPOT이 inproc 엔드포인트 생성 |
| `slk_connect(xsub, "inproc://topic")` | `slk_spot_subscribe(spot, "topic")` | SPOT이 자동으로 연결 |

### 발행

| 전통적인 PUB/SUB | SPOT 동등 함수 | 참고 |
|---------------------|-----------------|-------|
| `slk_send(xpub, data, len, 0)` | `slk_spot_publish(spot, topic, data, len)` | SPOT이 토픽 프레임 추가 |
| 멀티 프레임: `[topic][data]` | 내부적으로 동일한 형식 사용 | SPOT이 프레이밍 처리 |

### 구독

| 전통적인 PUB/SUB | SPOT 동등 함수 | 참고 |
|---------------------|-----------------|-------|
| `slk_setsockopt(xsub, SLK_SUBSCRIBE, topic, len)` | `slk_spot_subscribe(spot, topic)` | SPOT이 구독 관리 |
| `slk_setsockopt(xsub, SLK_UNSUBSCRIBE, topic, len)` | `slk_spot_unsubscribe(spot, topic)` | SPOT이 구독 제거 |
| `slk_setsockopt(xsub, SLK_PSUBSCRIBE, pattern, len)` | `slk_spot_subscribe_pattern(spot, pattern)` | LOCAL 토픽만 해당 |

### 수신

| 전통적인 PUB/SUB | SPOT 동등 함수 | 참고 |
|---------------------|-----------------|-------|
| `slk_recv(xsub, buf, len, flags)` (프레임 1) | `slk_spot_recv(spot, topic, ...)` | SPOT이 토픽/데이터 분리 |
| `slk_recv(xsub, buf, len, flags)` (프레임 2) | 동일한 호출 | 두 프레임 모두 단일 API |

### 소켓 옵션

| 전통적인 PUB/SUB | SPOT 동등 함수 | 참고 |
|---------------------|-----------------|-------|
| `slk_setsockopt(socket, SLK_SNDHWM, ...)` | `slk_spot_set_hwm(spot, sndhwm, rcvhwm)` | 모든 소켓에 설정 |
| `slk_setsockopt(socket, SLK_RCVHWM, ...)` | 동일 | |
| `slk_getsockopt(socket, SLK_FD, ...)` | `slk_spot_fd(spot, &fd)` | XSUB FD 반환 |

### 정리

| 전통적인 PUB/SUB | SPOT 동등 함수 | 참고 |
|---------------------|-----------------|-------|
| `slk_close(xpub)` | `slk_spot_topic_destroy(spot, topic)` | 토픽의 XPUB 파괴 |
| `slk_close(xsub)` | `slk_spot_destroy(&spot)` | 모든 소켓 파괴 |

---

## 단계별 마이그레이션

### 단계 1: PUB/SUB 사용 식별

**모든 PUB/SUB 소켓 생성 찾기:**
```bash
grep -r "SLK_XPUB\|SLK_XSUB\|SLK_PUB\|SLK_SUB" src/
```

**토픽 목록 작성:**
- 발행/구독되는 모든 토픽 나열
- 토픽 명명 패턴 식별
- 멀티 프레임 메시지 사용 기록

### 단계 2: 토픽 매핑 생성

**이전 (전통적인 방식):**
```c
// Publisher
slk_socket_t *xpub1 = slk_socket(ctx, SLK_XPUB);
slk_bind(xpub1, "inproc://game:player");

slk_socket_t *xpub2 = slk_socket(ctx, SLK_XPUB);
slk_bind(xpub2, "inproc://game:score");

// Subscriber
slk_socket_t *xsub = slk_socket(ctx, SLK_XSUB);
slk_connect(xsub, "inproc://game:player");
slk_connect(xsub, "inproc://game:score");
```

**이후 (SPOT):**
```c
// Single SPOT instance
slk_spot_t *spot = slk_spot_new(ctx);

// Create topics
slk_spot_topic_create(spot, "game:player");
slk_spot_topic_create(spot, "game:score");

// Subscribe
slk_spot_subscribe(spot, "game:player");
slk_spot_subscribe(spot, "game:score");
```

### 단계 3: 발행 코드 업데이트

**이전:**
```c
// Multi-frame publish
msg_t topic_msg, data_msg;
msg_init_buffer(&topic_msg, "game:player", 11);
msg_init_buffer(&data_msg, "Player1", 7);

slk_msg_send(&topic_msg, xpub, SLK_SNDMORE);
slk_msg_send(&data_msg, xpub, 0);

msg_close(&topic_msg);
msg_close(&data_msg);
```

**이후:**
```c
// Single call
slk_spot_publish(spot, "game:player", "Player1", 7);
```

### 단계 4: 구독 코드 업데이트

**이전:**
```c
// Set subscription option
const char *topic = "game:player";
slk_setsockopt(xsub, SLK_SUBSCRIBE, topic, strlen(topic));
```

**이후:**
```c
// Subscribe API
slk_spot_subscribe(spot, "game:player");
```

### 단계 5: 수신 코드 업데이트

**이전:**
```c
// Receive multi-frame message
msg_t topic_msg, data_msg;

msg_init(&topic_msg);
slk_msg_recv(&topic_msg, xsub, 0);

msg_init(&data_msg);
slk_msg_recv(&data_msg, xsub, 0);

// Extract topic and data
const char *topic = (const char *)msg_data(&topic_msg);
size_t topic_len = msg_size(&topic_msg);

const char *data = (const char *)msg_data(&data_msg);
size_t data_len = msg_size(&data_msg);

msg_close(&topic_msg);
msg_close(&data_msg);
```

**이후:**
```c
// Single call with separated buffers
char topic[256], data[4096];
size_t topic_len, data_len;

slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, 0);
```

### 단계 6: 정리 코드 업데이트

**이전:**
```c
slk_close(xpub1);
slk_close(xpub2);
slk_close(xsub);
```

**이후:**
```c
slk_spot_destroy(&spot); // Closes all sockets
```

### 단계 7: 테스트

**단위 테스트:**
```c
void test_spot_migration()
{
    slk_ctx_t *ctx = slk_ctx_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    // Create and subscribe
    slk_spot_topic_create(spot, "test:topic");
    slk_spot_subscribe(spot, "test:topic");

    // Publish
    slk_spot_publish(spot, "test:topic", "hello", 5);
    slk_sleep(10);

    // Receive
    char topic[256], data[256];
    size_t topic_len, data_len;
    int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, SLK_DONTWAIT);

    assert(rc == 0);
    assert(memcmp(data, "hello", 5) == 0);

    slk_spot_destroy(&spot);
    slk_ctx_destroy(ctx);
}
```

---

## 마이그레이션 고려사항

### 패턴 구독

**전통적인 PUB/SUB:**
- 패턴 구독이 inproc와 TCP 모두에서 동작

**SPOT:**
- 패턴 구독은 **LOCAL만 해당**
- REMOTE 토픽은 명시적 구독 필요

**마이그레이션:**
```c
// Before: Pattern subscription over TCP
slk_setsockopt(xsub, SLK_PSUBSCRIBE, "game:*", 6);

// After: SPOT pattern subscription (LOCAL only)
slk_spot_subscribe_pattern(spot, "game:*");

// For REMOTE topics, use explicit subscriptions
slk_spot_subscribe(spot, "game:player");
slk_spot_subscribe(spot, "game:score");
```

### 멀티 프레임 메시지

**전통적인 PUB/SUB:**
- 프레임 경계에 대한 직접적인 제어
- 임의의 멀티 프레임 메시지 전송 가능

**SPOT:**
- 고정된 두 프레임 형식: `[topic][data]`
- 추가 프레임 지원 없음

**마이그레이션:**
추가 프레임이 필요한 경우 데이터에 인코딩:
```c
// Before: [topic][metadata][data]
slk_send(xpub, "topic", 5, SLK_SNDMORE);
slk_send(xpub, "metadata", 8, SLK_SNDMORE);
slk_send(xpub, "data", 4, 0);

// After: Encode metadata in data
typedef struct {
    char metadata[8];
    char data[4];
} message_t;

message_t msg;
memcpy(msg.metadata, "metadata", 8);
memcpy(msg.data, "data", 4);

slk_spot_publish(spot, "topic", &msg, sizeof(msg));
```

### 소켓 옵션

**전통적인 PUB/SUB:**
- 소켓별 세밀한 제어

**SPOT:**
- 모든 토픽에 대한 전역 HWM 설정

**마이그레이션:**
```c
// Before: Per-socket HWM
int hwm = 10000;
slk_setsockopt(xpub1, SLK_SNDHWM, &hwm, sizeof(hwm));
slk_setsockopt(xpub2, SLK_SNDHWM, &hwm, sizeof(hwm));

// After: Global HWM
slk_spot_set_hwm(spot, 10000, 10000);
```

토픽별 HWM이 필요한 경우 별도의 SPOT 인스턴스를 사용하세요.

### TCP 전송

**전통적인 PUB/SUB:**
```c
// Publisher
slk_socket_t *xpub = slk_socket(ctx, SLK_XPUB);
slk_bind(xpub, "tcp://*:5555");

// Subscriber
slk_socket_t *xsub = slk_socket(ctx, SLK_XSUB);
slk_connect(xsub, "tcp://server:5555");
```

**SPOT:**
```c
// Publisher
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "topic");
slk_spot_bind(spot, "tcp://*:5555");

// Subscriber
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_route(spot, "topic", "tcp://server:5555");
slk_spot_subscribe(spot, "topic");
```

**또는 클러스터 동기화 사용:**
```c
// Subscriber (automatic discovery)
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_cluster_add(spot, "tcp://server:5555");
slk_spot_cluster_sync(spot, 1000);
slk_spot_subscribe(spot, "topic");
```

---

## 호환성 참고사항

### SPOT과 전통적인 PUB/SUB 혼합 불가

**SPOT은 클러스터 프로토콜에 ROUTER를 사용**하므로 원시 XPUB/XSUB 연결과 호환되지 않습니다.

**호환되지 않음:**
```c
// Traditional XSUB
slk_socket_t *xsub = slk_socket(ctx, SLK_XSUB);
slk_connect(xsub, "tcp://spot-node:5555"); // Won't work!
```

**해결 방법:**
- 모든 노드를 SPOT으로 마이그레이션
- 또는 SPOT과 전통적인 PUB/SUB에 별도의 엔드포인트 사용

### 구독 가시성

**전통적인 XPUB:**
- `SLK_XPUB_VERBOSE`를 통해 구독 메시지 표시

**SPOT:**
- 구독 추적은 subscription_manager_t 내부
- 외부 가시성 없음 (향후 개선 예정)

### 메시지 순서

**둘 다:**
- 토픽별 FIFO 순서 유지
- 토픽 간 순서 보장 없음

---

## 마이그레이션 예제

### 예제 1: 간단한 Pub/Sub

**이전:**
```c
// Publisher
slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
slk_bind(pub, "inproc://events");
slk_send(pub, "event1", 6, 0);

// Subscriber
slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_connect(sub, "inproc://events");
const char *filter = "";
slk_setsockopt(sub, SLK_SUBSCRIBE, filter, 0);
char buf[256];
slk_recv(sub, buf, sizeof(buf), 0);
```

**이후:**
```c
// Combined pub/sub
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "events");
slk_spot_subscribe(spot, "events");
slk_spot_publish(spot, "events", "event1", 6);

slk_sleep(10);

char topic[256], data[256];
size_t topic_len, data_len;
slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, SLK_DONTWAIT);
```

### 예제 2: 다중 발행자

**이전:**
```c
// Publisher 1
slk_socket_t *pub1 = slk_socket(ctx, SLK_XPUB);
slk_bind(pub1, "inproc://topic1");

// Publisher 2
slk_socket_t *pub2 = slk_socket(ctx, SLK_XPUB);
slk_bind(pub2, "inproc://topic2");

// Subscriber
slk_socket_t *sub = slk_socket(ctx, SLK_XSUB);
slk_connect(sub, "inproc://topic1");
slk_connect(sub, "inproc://topic2");
```

**이후:**
```c
// Single SPOT instance
slk_spot_t *spot = slk_spot_new(ctx);

// Create both topics
slk_spot_topic_create(spot, "topic1");
slk_spot_topic_create(spot, "topic2");

// Subscribe to both
slk_spot_subscribe(spot, "topic1");
slk_spot_subscribe(spot, "topic2");
```

### 예제 3: 패턴 구독

**이전:**
```c
slk_socket_t *xsub = slk_socket(ctx, SLK_XSUB);
slk_connect(xsub, "inproc://game");

const char *pattern = "player:";
slk_setsockopt(xsub, SLK_PSUBSCRIBE, pattern, strlen(pattern));
```

**이후:**
```c
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_subscribe_pattern(spot, "player:*");
```

**참고:** 패턴 매칭 문법이 다릅니다:
- 전통적인 방식: 접두사 매칭 ("player:"는 "player:123"과 매칭)
- SPOT: 와일드카드 매칭 ("player:*"는 "player:123"과 매칭)

### 예제 4: TCP Pub/Sub

**이전:**
```c
// Server
slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
slk_bind(pub, "tcp://*:5555");
slk_send(pub, "data", 4, 0);

// Client
slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_connect(sub, "tcp://server:5555");
slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);
char buf[256];
slk_recv(sub, buf, sizeof(buf), 0);
```

**이후:**
```c
// Server
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "data");
slk_spot_bind(spot, "tcp://*:5555");
slk_spot_publish(spot, "data", "data", 4);

// Client
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_route(spot, "data", "tcp://server:5555");
slk_spot_subscribe(spot, "data");

char topic[256], data[256];
size_t topic_len, data_len;
slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, 0);
```

---

## 마이그레이션 테스트

### 병렬 실행 전략

전통적인 시스템과 SPOT 시스템을 병렬로 실행:

**이중 발행:**
```c
// Publish to both traditional and SPOT
slk_send(xpub, data, len, 0);
slk_spot_publish(spot, topic, data, len);

// Compare results
```

**섀도우 수신:**
```c
// Receive from both and compare
char traditional_data[256];
slk_recv(xsub, traditional_data, sizeof(traditional_data), 0);

char spot_topic[256], spot_data[256];
size_t topic_len, data_len;
slk_spot_recv(spot, spot_topic, sizeof(spot_topic), &topic_len,
              spot_data, sizeof(spot_data), &data_len, 0);

// Assert equality
assert(memcmp(traditional_data, spot_data, data_len) == 0);
```

### 점진적 마이그레이션

한 번에 하나의 토픽씩 마이그레이션:

1. **1단계**: 발행자 마이그레이션 (이중 발행)
2. **2단계**: 구독자 마이그레이션 (이중 구독)
3. **3단계**: 전통적인 코드 제거

---

## 성능 비교

| 지표 | 전통적인 PUB/SUB | SPOT | 참고 |
|--------|---------------------|------|-------|
| 지연시간 (inproc) | 0.01-0.1 µs | 0.01-0.1 µs | 동등 |
| 지연시간 (TCP) | 10-50 µs | 10-50 µs | 동등 |
| 처리량 (inproc) | 10M msg/s | 10M msg/s | 동등 |
| 처리량 (TCP) | 1M msg/s | 1M msg/s | 동등 |
| 메모리 오버헤드 | 낮음 | 중간 | SPOT에는 레지스트리가 있음 |
| API 복잡도 | 높음 | 낮음 | SPOT이 단순화 |

---

## 참고 문서

- [API 레퍼런스](API.ko.md)
- [빠른 시작 가이드](QUICK_START.ko.md)
- [아키텍처 개요](ARCHITECTURE.ko.md)
- [사용 패턴](PATTERNS.ko.md)

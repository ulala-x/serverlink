[![English](https://img.shields.io/badge/lang:en-red.svg)](ARCHITECTURE.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](ARCHITECTURE.ko.md)

# SPOT PUB/SUB 아키텍처

ServerLink SPOT (Scalable Partitioned Ordered Topics)의 내부 아키텍처 및 설계에 대한 문서입니다.

## 목차

1. [개요](#개요)
2. [아키텍처 발전 과정](#아키텍처-발전-과정)
3. [컴포넌트 아키텍처](#컴포넌트-아키텍처)
4. [클래스 다이어그램](#클래스-다이어그램)
5. [데이터 흐름](#데이터-흐름)
6. [스레딩 모델](#스레딩-모델)
7. [메모리 관리](#메모리-관리)
8. [성능 특성](#성능-특성)
9. [설계 결정 사항](#설계-결정-사항)

---

## 개요

SPOT은 단순화된 **직접 XPUB/XSUB** 아키텍처를 사용하여 위치 투명한 pub/sub을 제공합니다:

- SPOT 인스턴스당 **하나의 공유 XPUB** 소켓 (모든 토픽 발행)
- SPOT 인스턴스당 **하나의 공유 XSUB** 소켓 (연결된 모든 발행자로부터 수신)
- 토픽 메타데이터 및 라우팅을 위한 **Topic Registry**
- 구독 추적을 위한 **Subscription Manager**

**핵심 설계 원칙:**
1. **단순성** - 중간 라우팅 없이 직접 XPUB/XSUB 연결
2. inproc 전송을 사용한 **Zero-copy LOCAL 토픽**
3. TCP 연결을 통한 **투명한 원격 토픽**
4. read/write 락킹을 통한 **스레드 안전** 연산

---

## 컴포넌트 아키텍처

```
┌─────────────────────────────────────────────────────────────────────┐
│                          slk_spot_t                                  │
│                      (SPOT PUB/SUB Instance)                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌────────────────────┐    ┌────────────────────┐                  │
│  │  topic_registry_t  │    │ subscription_      │                  │
│  │                    │    │ manager_t          │                  │
│  ├────────────────────┤    ├────────────────────┤                  │
│  │ - LOCAL topics     │    │ - Topic subs       │                  │
│  │ - REMOTE topics    │    │ - Pattern subs     │                  │
│  │ - Endpoint mapping │    │ - Subscriber types │                  │
│  └────────────────────┘    └────────────────────┘                  │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                     Socket Layer                              │  │
│  ├──────────────────────────────────────────────────────────────┤  │
│  │                                                               │  │
│  │   _pub_socket (XPUB)         _recv_socket (XSUB)             │  │
│  │   ┌─────────────────┐        ┌─────────────────┐             │  │
│  │   │ Bound to:       │        │ Connected to:   │             │  │
│  │   │ - inproc://spot-N│       │ - Local XPUB    │             │  │
│  │   │ - tcp://*:port  │        │ - Remote XPUBs  │             │  │
│  │   └─────────────────┘        └─────────────────┘             │  │
│  │          │                          │                         │  │
│  │          ▼                          ▼                         │  │
│  │   publish() sends here       recv() reads here               │  │
│  │                                                               │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                     Endpoint Tracking                         │  │
│  ├──────────────────────────────────────────────────────────────┤  │
│  │ _inproc_endpoint: "inproc://spot-0"  (always set)            │  │
│  │ _bind_endpoint: "tcp://...:5555"      (after bind())         │  │
│  │ _bind_endpoints: set of all bound endpoints                  │  │
│  │ _connected_endpoints: set of cluster connections             │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 클래스 다이어그램

### 핵심 클래스

```
┌───────────────────────────────────────┐
│           topic_registry_t            │
├───────────────────────────────────────┤
│ - _topics: map<string, topic_entry_t> │
│ - _mutex: shared_mutex                │
├───────────────────────────────────────┤
│ + register_local(topic_id)            │
│ + register_local(topic_id, endpoint)  │
│ + register_remote(topic_id, endpoint) │
│ + unregister(topic_id)                │
│ + lookup(topic_id) → optional<entry>  │
│ + has_topic(topic_id) → bool          │
│ + get_local_topics() → vector         │
│ + get_all_topics() → vector           │
└───────────────────────────────────────┘

┌───────────────────────────────────────┐
│           topic_entry_t               │
├───────────────────────────────────────┤
│ + location: LOCAL | REMOTE            │
│ + endpoint: string                    │
│ + created_at: timestamp               │
└───────────────────────────────────────┘

┌───────────────────────────────────────┐
│       subscription_manager_t          │
├───────────────────────────────────────┤
│ - _subscriptions: map<string, set>    │
│ - _patterns: map<string, set>         │
│ - _mutex: shared_mutex                │
├───────────────────────────────────────┤
│ + add_subscription(topic, sub)        │
│ + add_pattern_subscription(pattern)   │
│ + remove_subscription(topic, sub)     │
│ + is_subscribed(topic, sub) → bool    │
│ + match_pattern(topic) → bool         │
└───────────────────────────────────────┘

┌───────────────────────────────────────┐
│           spot_pubsub_t               │
├───────────────────────────────────────┤
│ - _ctx: ctx_t*                        │
│ - _registry: unique_ptr<registry>     │
│ - _sub_manager: unique_ptr<sub_mgr>   │
│ - _pub_socket: socket_base_t* (XPUB)  │
│ - _recv_socket: socket_base_t* (XSUB) │
│ - _inproc_endpoint: string            │
│ - _bind_endpoint: string              │
│ - _bind_endpoints: set<string>        │
│ - _connected_endpoints: set<string>   │
│ - _sndhwm, _rcvhwm: int               │
│ - _rcvtimeo: int                      │
│ - _mutex: shared_mutex                │
├───────────────────────────────────────┤
│ + topic_create(topic_id)              │
│ + topic_destroy(topic_id)             │
│ + topic_route(topic_id, endpoint)     │
│ + subscribe(topic_id)                 │
│ + subscribe_pattern(pattern)          │
│ + unsubscribe(topic_id)               │
│ + publish(topic_id, data, len)        │
│ + recv(topic, data, flags)            │
│ + bind(endpoint)                      │
│ + cluster_add(endpoint)               │
│ + cluster_remove(endpoint)            │
│ + cluster_sync(timeout)               │
│ + list_topics() → vector              │
│ + topic_exists(topic_id) → bool       │
│ + topic_is_local(topic_id) → bool     │
│ + set_hwm(sndhwm, rcvhwm)             │
│ + setsockopt(option, value, len)      │
│ + getsockopt(option, value, len)      │
│ + fd() → int                          │
└───────────────────────────────────────┘
```

---

## 데이터 흐름

### LOCAL 토픽 Publish/Subscribe (동일 SPOT 인스턴스)

```
┌──────────────────────────────────────────────────────────────────┐
│                         SPOT Instance                             │
│                                                                   │
│   Publisher Thread              Subscriber Thread                 │
│         │                             │                           │
│         │ slk_spot_publish()          │                           │
│         │                             │                           │
│         ▼                             │                           │
│   ┌───────────┐                       │                           │
│   │   XPUB    │◄── bound to inproc    │                           │
│   └─────┬─────┘                       │                           │
│         │                             │                           │
│         │ [topic][data]               │                           │
│         │                             │                           │
│         └──────────── inproc ─────────┤                           │
│                                       ▼                           │
│                                 ┌───────────┐                     │
│                                 │   XSUB    │◄── connected to XPUB│
│                                 └─────┬─────┘                     │
│                                       │                           │
│                                       │ slk_spot_recv()           │
│                                       ▼                           │
│                              Return [topic][data]                 │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘

성능:
- inproc (ypipe)을 통한 Zero-copy
- 마이크로초 이하의 지연시간
- 직렬화 오버헤드 없음
```

### REMOTE 토픽 Publish/Subscribe (노드 간)

```
┌────────────────────────┐              ┌────────────────────────┐
│        Node A          │              │        Node B          │
│                        │              │                        │
│  ┌─────────────────┐   │              │   ┌─────────────────┐  │
│  │ slk_spot_publish│   │              │   │                 │  │
│  └────────┬────────┘   │              │   │  slk_spot_recv  │  │
│           │            │              │   │                 │  │
│           ▼            │              │   └────────▲────────┘  │
│      ┌─────────┐       │              │            │           │
│      │  XPUB   │       │              │       ┌────┴────┐      │
│      │ (bound) │       │              │       │  XSUB   │      │
│      └────┬────┘       │              │       │(connect)│      │
│           │            │              │       └────┬────┘      │
│           │ TCP        │              │            │           │
│           └────────────┼──────────────┼────────────┘           │
│                        │  [topic]     │                        │
│                        │  [data]      │                        │
│                        │              │                        │
└────────────────────────┘              └────────────────────────┘

설정 방법:
1. Node A: bind("tcp://*:5555")
2. Node B: cluster_add("tcp://nodeA:5555")
3. Node B: subscribe("topic")  → XSUB가 구독 메시지를 XPUB로 전송
4. Node A: publish("topic", data) → XPUB가 매칭되는 XSUB로 전달

성능:
- TCP 네트워크 오버헤드 (~10-100 µs)
- 자동 재연결이 포함된 영구 연결
- ZeroMQ가 구독 필터링 처리
```

### 다중 발행자 시나리오

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Publisher A   │    │   Publisher B   │    │   Subscriber    │
│                 │    │                 │    │                 │
│ ┌─────────────┐ │    │ ┌─────────────┐ │    │ ┌─────────────┐ │
│ │    XPUB     │ │    │ │    XPUB     │ │    │ │    XSUB     │ │
│ │ tcp://:5555 │ │    │ │ tcp://:5556 │ │    │ │ (connects)  │ │
│ └──────┬──────┘ │    │ └──────┬──────┘ │    │ └──────┬──────┘ │
│        │        │    │        │        │    │        │        │
└────────┼────────┘    └────────┼────────┘    └────────┼────────┘
         │                      │                      │
         │     TCP              │     TCP              │
         └──────────────────────┴──────────────────────┘
                                │
                   ┌────────────┴────────────┐
                   │  Subscriber receives    │
                   │  from BOTH publishers   │
                   └─────────────────────────┘

설정 방법:
1. Publisher A: bind("tcp://*:5555")
2. Publisher B: bind("tcp://*:5556")
3. Subscriber: cluster_add("tcp://pubA:5555")
4. Subscriber: cluster_add("tcp://pubB:5556")
5. Subscriber: subscribe("topic")
```

### 패턴 구독

```
Subscriber                                    Publisher
    │                                            │
    │ subscribe_pattern("events:*")              │
    │                                            │
    │ ┌─────────────────────────────────────┐   │
    │ │ Convert to XPUB prefix:             │   │
    │ │ "events:*" → "events:"              │   │
    │ │                                     │   │
    │ │ Send subscription message:          │   │
    │ │ [0x01]["events:"]                   │   │
    │ └─────────────────────────────────────┘   │
    │                                            │
    │           subscription message             │
    │ ─────────────────────────────────────────► │
    │                                            │
    │                      publish("events:login", data)
    │                      publish("events:logout", data)
    │                      publish("metrics:cpu", data)
    │                                            │
    │ ◄──────── "events:login" matches ──────── │
    │ ◄──────── "events:logout" matches ─────── │
    │           "metrics:cpu" filtered out       │
    │                                            │

참고: XPUB는 glob 패턴이 아닌 prefix 매칭을 사용합니다.
"events:*" 패턴은 "events:" prefix로 변환됩니다.
```

---

## 스레딩 모델

### 동시성 제어

```cpp
// 모든 public 메서드는 read/write 락킹을 사용합니다
class spot_pubsub_t {
    mutable std::shared_mutex _mutex;

    // 읽기 연산 (다중 동시 리더 허용)
    bool topic_exists(const std::string& topic_id) const {
        std::shared_lock<std::shared_mutex> lock(_mutex);
        return _registry->has_topic(topic_id);
    }

    // 쓰기 연산 (배타적 접근)
    int topic_create(const std::string& topic_id) {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        return _registry->register_local(topic_id, endpoint);
    }
};
```

### 스레드 안전성 보장

| 연산 | 스레드 안전성 | 락 유형 |
|-----------|---------------|-----------|
| topic_create | 안전 | Exclusive |
| topic_destroy | 안전 | Exclusive |
| subscribe | 안전 | Exclusive |
| unsubscribe | 안전 | Exclusive |
| publish | 안전 | Shared |
| recv | 안전 | Shared |
| list_topics | 안전 | Shared |
| cluster_add | 안전 | Exclusive |
| cluster_remove | 안전 | Exclusive |

### 모범 사례

```c
// 권장: 내부 락킹이 있는 공유 SPOT 인스턴스
slk_spot_t *spot = slk_spot_new(ctx);

// Thread 1: Publisher
void publisher_thread() {
    while (running) {
        slk_spot_publish(spot, "topic", data, len);  // 스레드 안전
    }
}

// Thread 2: Subscriber
void subscriber_thread() {
    while (running) {
        slk_spot_recv(spot, topic, &topic_len, data, &data_len, 0);  // 스레드 안전
    }
}

// 권장: 스레드별 별도 SPOT 인스턴스 (경합 없음)
void worker_thread() {
    slk_spot_t *local_spot = slk_spot_new(ctx);
    // 이 스레드에서만 독점 사용
}
```

---

## 메모리 관리

### 객체 수명 주기

```cpp
// 생성자: 공유 XPUB/XSUB 소켓 생성
spot_pubsub_t::spot_pubsub_t(ctx_t *ctx_)
{
    // 1. 고유한 inproc endpoint 생성
    _inproc_endpoint = "inproc://" + generate_instance_id();

    // 2. XPUB 생성 (발행용)
    _pub_socket = _ctx->create_socket(SL_XPUB);
    _pub_socket->bind(_inproc_endpoint.c_str());

    // 3. XSUB 생성 (수신용)
    _recv_socket = _ctx->create_socket(SL_XSUB);
    _recv_socket->connect(_inproc_endpoint.c_str());
}

// 소멸자: 올바른 정리 순서
spot_pubsub_t::~spot_pubsub_t()
{
    // 1. 수신 소켓을 먼저 닫음
    if (_recv_socket) {
        _recv_socket->close();
        _recv_socket = nullptr;
    }

    // 2. 발행 소켓 닫음
    if (_pub_socket) {
        _pub_socket->close();
        _pub_socket = nullptr;
    }

    // 3. Registry와 subscription manager는 unique_ptr을 통해 정리됨
}
```

### 메시지 형식

```
XPUB/XSUB 메시지 형식 (multipart):
┌──────────────────┐
│ Frame 1: Topic   │  (가변 길이 문자열)
├──────────────────┤
│ Frame 2: Data    │  (가변 길이 바이너리)
└──────────────────┘

구독 메시지 (XSUB가 XPUB로 전송):
┌──────────────────┐
│ 0x01 + prefix    │  prefix 구독
├──────────────────┤
│ 0x00 + prefix    │  prefix 구독 취소
└──────────────────┘
```

---

## 성능 특성

### 지연시간

| 연산 | LOCAL (inproc) | REMOTE (LAN) | REMOTE (WAN) |
|-----------|----------------|--------------|--------------|
| Publish | 0.1-1 µs | 10-100 µs | 1-100 ms |
| Subscribe | 1-10 µs | 50-200 µs | 50-500 ms |
| Disconnect | 1-10 µs | 10-50 µs | 10-100 ms |

### 처리량 (메시지/초)

| 메시지 크기 | LOCAL (inproc) | REMOTE (1Gbps) |
|--------------|----------------|----------------|
| 64B | 10M msg/s | 1M msg/s |
| 1KB | 5M msg/s | 500K msg/s |
| 8KB | 2M msg/s | 100K msg/s |
| 64KB | 200K msg/s | 15K msg/s |

### 메모리 사용량

| 컴포넌트 | 메모리 |
|-----------|--------|
| SPOT 인스턴스 기본 | ~8 KB |
| 토픽당 (registry) | ~200 bytes |
| 구독당 | ~200 bytes |
| 클러스터 연결당 | ~4 KB |
| 메시지 버퍼 (기본 HWM=1000) | ~1 MB |

---

## 설계 결정 사항

### 인스턴스당 공유 XPUB/XSUB

SPOT은 인스턴스당 하나의 공유 XPUB/XSUB 소켓 쌍을 사용합니다:

**장점:**
- 토픽 수에 관계없이 일정한 소켓 수
- 간단한 리소스 관리
- ZeroMQ의 trie 기반 매칭을 사용한 효율적인 토픽 필터링

**고려사항:**
- 모든 메시지가 동일한 소켓을 통과 (직렬화 지점)
- 높은 처리량 시나리오에서는 여러 인스턴스 고려

### 패턴 구독 (Prefix 매칭)

XPUB/XSUB는 prefix 매칭을 사용합니다:

```
"events:*" pattern → converted to "events:" prefix
```

**매칭 예시:**
- `events:` prefix는 `events:login`, `events:logout`, `events:user:created`와 매칭
- `game:player:` prefix는 `game:player:spawn`, `game:player:death`와 매칭

### 클러스터 연결 관리

`cluster_add()`는 XSUB 소켓에 새 endpoint를 연결하고, `cluster_remove()`는 `term_endpoint()`를 통해 연결을 종료합니다:

```cpp
// cluster_add(): XSUB에 새 endpoint 연결
_recv_socket->connect(endpoint.c_str());
_connected_endpoints.insert(endpoint);

// cluster_remove(): endpoint 연결 해제
_recv_socket->term_endpoint(endpoint.c_str());
_connected_endpoints.erase(endpoint);
```

---

## 관련 문서

- [API 레퍼런스](API.ko.md) - 전체 API 문서
- [빠른 시작](QUICK_START.ko.md) - 시작 가이드
- [클러스터링 가이드](CLUSTERING.ko.md) - 다중 노드 설정
- [사용 패턴](PATTERNS.ko.md) - 일반적인 패턴 및 모범 사례

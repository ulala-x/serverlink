[![English](https://img.shields.io/badge/lang:en-red.svg)](README.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](README.ko.md)

# SPOT PUB/SUB 문서

ServerLink SPOT (Scalable Partitioned Ordered Topics) 전체 문서.

## SPOT이란?

SPOT은 ServerLink 기반의 **위치 투명 pub/sub 시스템**으로 다음을 제공합니다:

- **LOCAL Topic**: Zero-copy inproc 메시징 (나노초 Latency)
- **REMOTE Topic**: 자동 Routing을 통한 TCP 네트워킹
- **Cluster Sync**: Node 간 자동 Topic 발견
- **Pattern Matching**: Wildcard를 사용한 다중 Topic Subscribe

## 문서 목차

### 시작하기

1. **[Quick Start Guide](QUICK_START.ko.md)** - 5분 안에 시작하기
   - SPOT이란?
   - 설치
   - 첫 번째 SPOT Application
   - Local 및 Remote Topic
   - Event Loop 통합

### 핵심 문서

2. **[API Reference](API.ko.md)** - 전체 Function Reference
   - 모든 `slk_spot_*` Function
   - Data Type 및 상수
   - Error Code
   - Thread Safety

3. **[Architecture](ARCHITECTURE.ko.md)** - 내부 설계
   - Component Architecture
   - Class Diagram
   - Data Flow
   - Threading Model
   - 성능 특성

4. **[Protocol Specification](PROTOCOL.ko.md)** - Wire Format
   - Message Format
   - Command Code (PUBLISH, SUBSCRIBE, QUERY 등)
   - Protocol Flow
   - Wire Format 예제

### 고급 주제

5. **[Clustering Guide](CLUSTERING.ko.md)** - Multi-node 배포
   - Single Node 설정
   - Two-node Cluster
   - N-node Mesh Topology
   - Hub-spoke Topology
   - 장애 처리
   - Production 배포

6. **[Usage Patterns](PATTERNS.ko.md)** - Design Pattern
   - 명시적 Routing (Game Server)
   - Central Registry (Microservices)
   - Hybrid 접근법
   - Producer-Consumer
   - Fan-out (1:N)
   - Fan-in (N:1)
   - Event Sourcing
   - Stream Processing

7. **[Migration Guide](MIGRATION.ko.md)** - 기존 PUB/SUB에서 마이그레이션
   - API 매핑 테이블
   - 단계별 마이그레이션
   - 호환성 참고 사항
   - 예제 마이그레이션

## 빠른 참조

### SPOT 생성 및 사용

```c
// SPOT Instance 생성
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// LOCAL Topic 생성
slk_spot_topic_create(spot, "game:player");

// Subscribe
slk_spot_subscribe(spot, "game:player");

// Publish
slk_spot_publish(spot, "game:player", "data", 4);

// Receive
char topic[256], data[4096];
size_t topic_len, data_len;
slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, 0);

// Cleanup
slk_spot_destroy(&spot);
slk_ctx_destroy(ctx);
```

### Cluster 설정

```c
// Node A (Server)
slk_spot_topic_create(spot, "sensor:temp");
slk_spot_bind(spot, "tcp://*:5555");

// Node B (Client)
slk_spot_cluster_add(spot, "tcp://nodeA:5555");
slk_spot_cluster_sync(spot, 1000);
slk_spot_subscribe(spot, "sensor:temp");
```

## 사용 사례

- **Game Server**: Player Event, Chat Room, World State
- **Microservices**: Event 분배, Service Discovery
- **IoT System**: Sensor Data Routing, Device 관리
- **Analytics**: 실시간 Metrics, Log 집계
- **Financial**: Market Data, Trade Event

## 주요 기능

### Location Transparency

위치를 모르고도 Topic에 Subscribe:
```c
// LOCAL과 REMOTE Topic 모두에서 동작
slk_spot_subscribe(spot, "any:topic");
```

### Pattern Subscription

Wildcard를 사용한 다중 Topic Subscribe:
```c
slk_spot_subscribe_pattern(spot, "game:player:*");
// 수신: game:player:spawn, game:player:death 등
```

### Automatic Discovery

Cluster Node 간 Topic 발견:
```c
slk_spot_cluster_sync(spot, 1000);
// 이제 모든 Remote Topic을 알고 있음
```

## 성능

| Transport | Latency | Throughput |
|-----------|---------|------------|
| **inproc** (LOCAL) | 0.01-0.1 µs | 10M msg/s |
| **TCP** (REMOTE) | 10-50 µs | 1M msg/s |

## 예제

### 기본 Pub/Sub
```c
slk_spot_topic_create(spot, "events");
slk_spot_subscribe(spot, "events");
slk_spot_publish(spot, "events", "hello", 5);
```

### Remote Topic
```c
// Server
slk_spot_topic_create(spot, "sensor:temp");
slk_spot_bind(spot, "tcp://*:5555");

// Client
slk_spot_topic_route(spot, "sensor:temp", "tcp://server:5555");
slk_spot_subscribe(spot, "sensor:temp");
```

### Cluster Mesh
```c
// 모든 Node가 서로 연결
slk_spot_bind(nodeA, "tcp://*:5555");
slk_spot_bind(nodeB, "tcp://*:5556");

slk_spot_cluster_add(nodeA, "tcp://nodeB:5556");
slk_spot_cluster_add(nodeB, "tcp://nodeA:5555");

slk_spot_cluster_sync(nodeA, 1000);
slk_spot_cluster_sync(nodeB, 1000);
```

## API 개요

### Lifecycle
- `slk_spot_new()` - Instance 생성
- `slk_spot_destroy()` - Instance 삭제

### Topic Management
- `slk_spot_topic_create()` - LOCAL Topic 생성
- `slk_spot_topic_route()` - REMOTE Topic으로 Route
- `slk_spot_topic_destroy()` - Topic 삭제

### Pub/Sub
- `slk_spot_subscribe()` - Topic Subscribe
- `slk_spot_subscribe_pattern()` - Pattern Subscription (LOCAL 전용)
- `slk_spot_unsubscribe()` - Unsubscribe
- `slk_spot_publish()` - Message Publish
- `slk_spot_recv()` - Message 수신

### Clustering
- `slk_spot_bind()` - Server Mode로 Bind
- `slk_spot_cluster_add()` - Cluster Node 추가
- `slk_spot_cluster_remove()` - Cluster Node 제거
- `slk_spot_cluster_sync()` - Topic 동기화

### Introspection
- `slk_spot_list_topics()` - 모든 Topic 목록
- `slk_spot_topic_exists()` - Topic 존재 확인
- `slk_spot_topic_is_local()` - LOCAL 여부 확인

### Configuration
- `slk_spot_set_hwm()` - High Water Mark 설정
- `slk_spot_fd()` - Pollable FD 가져오기

## Error 처리

모든 Function은 오류 시 `-1`을 반환하고 `errno`를 설정합니다:

```c
int rc = slk_spot_publish(spot, "topic", data, len);
if (rc != 0) {
    int err = slk_errno();
    fprintf(stderr, "Error: %s\n", slk_strerror(err));
}
```

**주요 Error Code:**
- `SLK_EINVAL` - 잘못된 인자
- `SLK_ENOMEM` - 메모리 부족
- `SLK_ENOENT` - Topic 없음
- `SLK_EEXIST` - Topic 이미 존재
- `SLK_EAGAIN` - 리소스 일시적으로 사용 불가
- `SLK_EHOSTUNREACH` - Remote Host 연결 불가

## Thread Safety

SPOT Instance는 **Thread-safe**합니다:
- 여러 Thread가 동시에 `slk_spot_recv()` 호출 가능
- Publish 작업은 Topic 별로 직렬화됨
- 내부 Locking에 `std::shared_mutex` 사용

## 모범 사례

1. **LOCAL Topic 사용** - 동일 Process 내 통신에 사용
2. **Pattern Subscription** - 유연한 Routing (LOCAL 전용)
3. **Cluster Sync** - 자동 Topic 발견에 사용
4. **Error 처리** - 항상 반환값 확인
5. **Resource Cleanup** - `slk_spot_destroy()` 사용

## Platform Support

- **Linux**: epoll (테스트 완료)
- **Windows**: select (테스트 완료)
- **macOS**: kqueue (테스트 완료)
- **BSD**: kqueue (동작 예상)

## 빌드

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## 테스트

```bash
cd build
ctest -R spot --output-on-failure
```

**Test Coverage:**
- `test_spot_basic` - 기본 Pub/Sub 동작
- `test_spot_local` - LOCAL Topic 테스트
- `test_spot_remote` - REMOTE Topic 테스트
- `test_spot_cluster` - Cluster 동기화
- `test_spot_mixed` - LOCAL/REMOTE 혼합

## 기여하기

기여 가이드라인은 `docs/CONTRIBUTING.md`를 참조하세요.

## 라이선스

ServerLink는 Mozilla Public License 2.0 (MPL-2.0) 하에 라이선스됩니다.

## 도움 받기

- **문서**: `docs/spot/`
- **예제**: `examples/spot_cluster_sync_example.cpp`
- **테스트**: `tests/spot/`
- **GitHub Issues**: https://github.com/ulala-x/serverlink/issues

---

**Version**: 1.0
**최종 업데이트**: 2026-01-04
**상태**: Production Ready

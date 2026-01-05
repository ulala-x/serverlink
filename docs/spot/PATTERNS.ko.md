[![English](https://img.shields.io/badge/lang:en-red.svg)](PATTERNS.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](PATTERNS.ko.md)

# SPOT PUB/SUB 사용 패턴

SPOT 애플리케이션을 위한 일반적인 설계 패턴과 모범 사례입니다.

## 목차

1. [개요](#개요)
2. [Explicit Routing (게임 서버)](#explicit-routing-게임-서버)
3. [Central Registry (마이크로서비스)](#central-registry-마이크로서비스)
4. [Hybrid Approach (부분 자동화)](#hybrid-approach-부분-자동화)
5. [Producer-Consumer Pattern](#producer-consumer-pattern)
6. [Fan-Out Pattern (1:N)](#fan-out-pattern-1n)
7. [Fan-In Pattern (N:1)](#fan-in-pattern-n1)
8. [Request-Reply Pattern](#request-reply-pattern)
9. [Event Sourcing](#event-sourcing)
10. [Stream Processing](#stream-processing)

---

## 개요

SPOT은 사용 사례에 따라 여러 아키텍처 패턴을 지원합니다.

**패턴 선택 기준:**
- **Explicit Routing**: 완전한 제어, 수동 토픽-엔드포인트 매핑
- **Central Registry**: 자동 검색, 서비스 레지스트리 필요
- **Hybrid**: Explicit Routing과 자동 라우팅의 조합

---

## Explicit Routing (게임 서버)

**사용 사례:** 게임 서버가 플레이어를 방에 배정하고, 명시적 토픽 라우팅으로 방 서버에 연결합니다.

### 아키텍처

```
┌─────────────────┐
│  Game Server    │  (Coordinator)
│   tcp://*:5555  │
└────────┬────────┘
         │
         │ Explicit routing:
         │ player:123 → tcp://room1:5556
         │ player:456 → tcp://room2:5557
         │
    ┌────┴────┐
    ▼         ▼
┌─────────┐ ┌─────────┐
│ Room 1  │ │ Room 2  │
│tcp://*: │ │tcp://*: │
│  5556   │ │  5557   │
└─────────┘ └─────────┘
```

### 구현

**Game Server (Coordinator):**
```c
typedef struct {
    const char *player_id;
    const char *room_endpoint;
} player_assignment_t;

player_assignment_t assignments[] = {
    {"player:123", "tcp://room1:5556"},
    {"player:456", "tcp://room2:5557"},
    {"player:789", "tcp://room1:5556"}
};

slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Route players to room servers
for (int i = 0; i < 3; i++) {
    slk_spot_topic_route(spot, assignments[i].player_id,
                         assignments[i].room_endpoint);
}

// Subscribe to all player topics
for (int i = 0; i < 3; i++) {
    slk_spot_subscribe(spot, assignments[i].player_id);
}

// Receive player events
while (1) {
    char topic[256], data[4096];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, 100);

    if (rc == 0) {
        topic[topic_len] = '\0';
        printf("Player event: %s\n", topic);
        // Route to game logic
    }
}
```

**Room Server:**
```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Create LOCAL topics for players in this room
slk_spot_topic_create(spot, "player:123");
slk_spot_topic_create(spot, "player:789");

// Bind for external connections
slk_spot_bind(spot, "tcp://*:5556");

// Publish player events
slk_spot_publish(spot, "player:123", "spawn", 5);
slk_spot_publish(spot, "player:789", "move", 4);
```

**장점:**
- 라우팅에 대한 완전한 제어
- 서비스 검색 오버헤드 없음
- 결정론적 메시지 흐름

**단점:**
- 수동 설정 필요
- 정적 토폴로지
- 자동 페일오버 없음

---

## Central Registry (마이크로서비스)

**사용 사례:** 동적 서비스 검색이 필요한 마이크로서비스 아키텍처

### 아키텍처

```
┌──────────────────────┐
│  Service Registry    │  (Consul, etcd, ZooKeeper)
│  - service:auth → tcp://auth:5555
│  - service:user → tcp://user:5556
│  - service:order → tcp://order:5557
└──────────┬───────────┘
           │
           │ Query registry
           │ Auto-route topics
           ▼
┌──────────────────────┐
│   API Gateway        │
│   (SPOT Client)      │
└──────────────────────┘
```

### 구현

**Service Registration:**
```c
// Service Registry API (pseudo-code)
typedef struct {
    const char *service_name;
    const char *endpoint;
} service_entry_t;

service_entry_t services[] = {
    {"service:auth", "tcp://auth:5555"},
    {"service:user", "tcp://user:5556"},
    {"service:order", "tcp://order:5557"}
};

// Publish to registry (Consul HTTP API)
void register_service(const char *name, const char *endpoint)
{
    // PUT /v1/agent/service/register
    // {"Name": "auth", "Address": "auth", "Port": 5555}
}
```

**Auto-Discovery Client:**
```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Query service registry
service_entry_t *services = query_service_registry();
int service_count = get_service_count();

// Auto-route topics based on registry
for (int i = 0; i < service_count; i++) {
    slk_spot_topic_route(spot, services[i].service_name,
                         services[i].endpoint);
}

// Subscribe to service events
slk_spot_subscribe(spot, "service:auth");
slk_spot_subscribe(spot, "service:user");
```

**Service 구현:**
```c
// Auth Service
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "service:auth");
slk_spot_bind(spot, "tcp://*:5555");

// Register with registry
register_service("service:auth", "tcp://auth:5555");

// Publish auth events
slk_spot_publish(spot, "service:auth", "user_login", 10);
```

**장점:**
- 동적 서비스 검색
- 자동 페일오버 (레지스트리가 헬스 체크를 지원하는 경우)
- 확장 가능한 아키텍처

**단점:**
- 외부 의존성 (레지스트리)
- 추가적인 네트워크 홉
- 레지스트리가 단일 장애점이 될 수 있음

---

## Hybrid Approach (부분 자동화)

**사용 사례:** 중요 서비스(Explicit Routing)와 동적 서비스(클러스터 동기화)의 조합

### 아키텍처

```
┌────────────────────┐       ┌────────────────────┐
│  Critical Services │       │  Dynamic Services  │
│  (Explicit)        │       │  (Cluster Sync)    │
├────────────────────┤       ├────────────────────┤
│ - payment:*        │       │ - analytics:*      │
│ - security:*       │       │ - logging:*        │
│ - billing:*        │       │ - metrics:*        │
└────────────────────┘       └────────────────────┘
         │                            │
         └─────────┬──────────────────┘
                   ▼
           ┌──────────────┐
           │  API Gateway │
           └──────────────┘
```

### 구현

```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Explicit routing for critical services
slk_spot_topic_route(spot, "payment:process", "tcp://payment:5555");
slk_spot_topic_route(spot, "security:audit", "tcp://security:5556");

// Cluster sync for dynamic services
slk_spot_cluster_add(spot, "tcp://analytics:5557");
slk_spot_cluster_add(spot, "tcp://logging:5558");
slk_spot_cluster_sync(spot, 1000);

// Subscribe to all topics
slk_spot_subscribe(spot, "payment:process");
slk_spot_subscribe(spot, "security:audit");
slk_spot_subscribe_pattern(spot, "analytics:*");
slk_spot_subscribe_pattern(spot, "logging:*");
```

**장점:**
- 두 방식의 장점을 모두 취함
- 중요 경로는 결정론적
- 동적 서비스는 독립적으로 확장 가능

**단점:**
- 더 복잡한 설정
- 혼합된 장애 모드

---

## Producer-Consumer Pattern

**사용 사례:** 비동기 작업 처리를 위한 작업 큐

### 아키텍처

```
┌──────────┐     ┌──────────┐     ┌──────────┐
│Producer 1│────>│          │<────│Consumer 1│
└──────────┘     │  Queue   │     └──────────┘
┌──────────┐     │ (SPOT)   │     ┌──────────┐
│Producer 2│────>│          │<────│Consumer 2│
└──────────┘     └──────────┘     └──────────┘
```

### 구현

**공유 Queue Topic:**
```c
// Producer 1
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "jobs:queue");

for (int i = 0; i < 100; i++) {
    char job[64];
    snprintf(job, sizeof(job), "job_%d", i);
    slk_spot_publish(spot, "jobs:queue", job, strlen(job));
}
```

**Consumer (Fair Queueing):**
```c
// Consumer 1
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_subscribe(spot, "jobs:queue");

while (1) {
    char topic[256], data[4096];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, -1);

    if (rc == 0) {
        data[data_len] = '\0';
        printf("Consumer 1 processing: %s\n", data);
        process_job(data);
    }
}
```

**참고:** SPOT은 구독에 대해 **fair-queueing**을 사용합니다. 여러 Consumer는 라운드 로빈 방식으로 메시지를 수신합니다.

---

## Fan-Out Pattern (1:N)

**사용 사례:** 여러 구독자에게 메시지 브로드캐스트

### 아키텍처

```
┌────────────┐
│ Publisher  │
└──────┬─────┘
       │
       │ Broadcast
       │
   ┌───┴────┬────┬────┐
   ▼        ▼    ▼    ▼
┌─────┐  ┌─────┐ ┌─────┐ ┌─────┐
│Sub 1│  │Sub 2│ │Sub 3│ │Sub 4│
└─────┘  └─────┘ └─────┘ └─────┘
```

### 구현

**Publisher:**
```c
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "broadcast:alerts");

// Publish to all subscribers
slk_spot_publish(spot, "broadcast:alerts", "System maintenance at 2AM", 26);
```

**Subscribers:**
```c
// Each subscriber creates own SPOT instance
slk_spot_t *sub1 = slk_spot_new(ctx);
slk_spot_subscribe(sub1, "broadcast:alerts");

slk_spot_t *sub2 = slk_spot_new(ctx);
slk_spot_subscribe(sub2, "broadcast:alerts");

// All receive the same message
```

---

## Fan-In Pattern (N:1)

**사용 사례:** 여러 소스에서 데이터 집계

### 아키텍처

```
┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐
│Src 1│  │Src 2│  │Src 3│  │Src 4│
└──┬──┘  └──┬──┘  └──┬──┘  └──┬──┘
   │        │        │        │
   └────────┴────────┴────────┘
                │
                ▼
         ┌─────────────┐
         │ Aggregator  │
         └─────────────┘
```

### 구현

**Sources:**
```c
// Source 1
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "source1:data");
slk_spot_publish(spot, "source1:data", "123", 3);

// Source 2
slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "source2:data");
slk_spot_publish(spot, "source2:data", "456", 3);
```

**Aggregator (Pattern Subscription):**
```c
slk_spot_t *aggregator = slk_spot_new(ctx);
slk_spot_subscribe_pattern(aggregator, "source*:data");

while (1) {
    char topic[256], data[4096];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(aggregator, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, 100);

    if (rc == 0) {
        topic[topic_len] = '\0';
        data[data_len] = '\0';
        printf("Aggregated from %s: %s\n", topic, data);
    }
}
```

---

## Request-Reply Pattern

**사용 사례:** 동기식 RPC 스타일 통신

**참고:** SPOT은 pub/sub용으로 설계되었으며, request/reply용이 아닙니다. RPC의 경우 ServerLink ROUTER/DEALER를 직접 사용하는 것을 고려하세요.

### 우회 구현

```c
// Client: Publish request with reply-to topic
slk_spot_t *client = slk_spot_new(ctx);
slk_spot_topic_create(client, "reply:client123");
slk_spot_subscribe(client, "reply:client123");

// Send request
char request[256];
snprintf(request, sizeof(request), "GET /user/123 reply:client123");
slk_spot_publish(client, "service:api", request, strlen(request));

// Wait for reply
char topic[256], data[4096];
size_t topic_len, data_len;
slk_spot_recv(client, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, 5000); // 5s timeout
```

**Server:**
```c
slk_spot_t *server = slk_spot_new(ctx);
slk_spot_topic_create(server, "service:api");
slk_spot_subscribe(server, "service:api");

// Process request
char topic[256], data[4096];
size_t topic_len, data_len;
slk_spot_recv(server, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, -1);

// Parse request and extract reply-to topic
char reply_topic[256];
sscanf(data, "GET /user/%*s %s", reply_topic);

// Send reply
slk_spot_topic_route(server, reply_topic, "tcp://client:5556");
slk_spot_publish(server, reply_topic, "{\"id\":123,\"name\":\"John\"}", 24);
```

**제한 사항:**
- 수동 correlation ID 관리
- SPOT에서 타임아웃 처리 없음
- ServerLink ROUTER/DEALER가 더 적합함

---

## Event Sourcing

**사용 사례:** 모든 상태 변경을 불변 이벤트로 저장

### 아키텍처

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ Command Side │────>│ Event Store  │────>│  Query Side  │
│  (Write)     │     │   (SPOT)     │     │   (Read)     │
└──────────────┘     └──────────────┘     └──────────────┘
```

### 구현

**Event Store (Publisher):**
```c
typedef struct {
    const char *event_type;
    const char *aggregate_id;
    const char *data;
} event_t;

void publish_event(slk_spot_t *spot, const event_t *event)
{
    char topic[256];
    snprintf(topic, sizeof(topic), "events:%s:%s",
             event->event_type, event->aggregate_id);

    slk_spot_topic_create(spot, topic);
    slk_spot_publish(spot, topic, event->data, strlen(event->data));
}

// Example events
event_t events[] = {
    {"UserCreated", "user123", "{\"name\":\"John\"}"},
    {"UserUpdated", "user123", "{\"email\":\"john@example.com\"}"},
    {"UserDeleted", "user123", "{}"}
};

for (int i = 0; i < 3; i++) {
    publish_event(spot, &events[i]);
}
```

**Event Subscriber (Projections):**
```c
slk_spot_t *subscriber = slk_spot_new(ctx);
slk_spot_subscribe_pattern(subscriber, "events:User*");

// Rebuild state from events
while (1) {
    char topic[256], data[4096];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(subscriber, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, -1);

    if (rc == 0) {
        topic[topic_len] = '\0';
        data[data_len] = '\0';

        // Apply event to projection
        if (strstr(topic, "UserCreated")) {
            // Create user in read model
        } else if (strstr(topic, "UserUpdated")) {
            // Update user in read model
        } else if (strstr(topic, "UserDeleted")) {
            // Delete user from read model
        }
    }
}
```

---

## Stream Processing

**사용 사례:** 실시간 분석 및 메트릭

### 아키텍처

```
┌────────────┐     ┌────────────┐     ┌────────────┐
│  Sensors   │────>│  Streamer  │────>│ Dashboard  │
│ (Publishers)     │  (Filter)  │     │ (Consumer) │
└────────────┘     └────────────┘     └────────────┘
```

### 구현

**Sensors (Publishers):**
```c
slk_spot_t *sensor = slk_spot_new(ctx);
slk_spot_topic_create(sensor, "sensor:temperature");

while (1) {
    float temp = read_temperature();
    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%.2f", temp);
    slk_spot_publish(sensor, "sensor:temperature", temp_str, strlen(temp_str));

    slk_sleep(1000); // 1 Hz
}
```

**Stream Processor (Filter/Transform):**
```c
slk_spot_t *processor = slk_spot_new(ctx);
slk_spot_subscribe_pattern(processor, "sensor:*");
slk_spot_topic_create(processor, "alerts:high_temp");

while (1) {
    char topic[256], data[256];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(processor, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, 100);

    if (rc == 0) {
        data[data_len] = '\0';
        float temp = atof(data);

        // Filter: High temperature alert
        if (temp > 30.0) {
            char alert[256];
            snprintf(alert, sizeof(alert), "High temp: %.2f", temp);
            slk_spot_publish(processor, "alerts:high_temp", alert, strlen(alert));
        }
    }
}
```

**Dashboard (Consumer):**
```c
slk_spot_t *dashboard = slk_spot_new(ctx);
slk_spot_subscribe(dashboard, "alerts:high_temp");

while (1) {
    char topic[256], data[256];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(dashboard, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, -1);

    if (rc == 0) {
        data[data_len] = '\0';
        printf("ALERT: %s\n", data);
    }
}
```

---

## 모범 사례

1. **토픽 명명 규칙**
   - 계층적 이름 사용: `domain:entity:action`
   - 예: `game:player:spawn`, `chat:room:message`

2. **오류 처리**
   - 항상 반환 값 확인
   - 오류 코드는 `slk_errno()` 사용
   - 지수 백오프를 사용한 재시도 로직 구현

3. **리소스 관리**
   - 완료 시 SPOT 인스턴스 파괴
   - `slk_spot_destroy()`로 소켓 정리

4. **성능**
   - 동일 프로세스 통신에는 LOCAL 토픽 사용
   - 가능하면 publish 배치 처리
   - 높은 처리량 시나리오에 맞게 HWM 조정

5. **테스트**
   - LOCAL 토픽으로 단위 테스트
   - REMOTE 토픽으로 통합 테스트
   - 격리를 위해 별도의 SPOT 인스턴스 사용

---

## 관련 문서

- [API Reference](API.ko.md)
- [Quick Start Guide](QUICK_START.ko.md)
- [Architecture Overview](ARCHITECTURE.ko.md)
- [Clustering Guide](CLUSTERING.ko.md)

[![English](https://img.shields.io/badge/lang:en-red.svg)](CLUSTERING.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](CLUSTERING.ko.md)

# SPOT PUB/SUB Clustering 가이드

다중 노드 SPOT Cluster 배포를 위한 완벽 가이드입니다.

## 목차

1. [개요](#개요)
2. [단일 노드 설정](#단일-노드-설정)
3. [2-노드 Cluster](#2-노드-cluster)
4. [N-노드 Mesh Topology](#n-노드-mesh-topology)
5. [Hub-Spoke Topology](#hub-spoke-topology)
6. [장애 처리](#장애-처리)
7. [네트워크 고려사항](#네트워크-고려사항)
8. [모니터링과 Observability](#모니터링과-observability)
9. [프로덕션 배포](#프로덕션-배포)

---

## 개요

SPOT은 Cluster 동기화를 통해 여러 노드에 걸친 분산 pub/sub을 지원합니다.

**핵심 개념:**
- **LOCAL Topics**: 이 노드에서 호스팅됨 (XPUB이 inproc에 bind)
- **REMOTE Topics**: 다른 노드에서 호스팅됨 (TCP를 통해 라우팅)
- **Cluster Sync**: QUERY/QUERY_RESP를 통한 원격 topic 검색
- **Mesh Topology**: 모든 노드가 다른 모든 노드에 연결
- **Hub-Spoke**: 중앙 hub와 spoke 노드들

**필수 조건:**
- TCP 지원으로 빌드된 ServerLink
- 노드 간 네트워크 연결
- 각 노드별 고유한 bind endpoint

---

## 단일 노드 설정

간단한 단일 노드 배포부터 시작합니다.

### 설정

```c
#include <serverlink/serverlink.h>

int main()
{
    slk_ctx_t *ctx = slk_ctx_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    // LOCAL topics 생성
    slk_spot_topic_create(spot, "game:player:spawn");
    slk_spot_topic_create(spot, "game:player:death");
    slk_spot_topic_create(spot, "chat:lobby");

    // local topics 구독
    slk_spot_subscribe(spot, "game:player:spawn");
    slk_spot_subscribe_pattern(spot, "chat:*");

    // 애플리케이션 로직
    while (1) {
        char topic[256], data[4096];
        size_t topic_len, data_len;

        int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                               data, sizeof(data), &data_len, 100);

        if (rc == 0) {
            // 메시지 처리
        }
    }

    slk_spot_destroy(&spot);
    slk_ctx_destroy(ctx);
    return 0;
}
```

**특징:**
- 네트워크 오버헤드 없음 (inproc만 사용)
- 나노초 지연 시간
- 장애 시나리오 없음
- 개발 및 테스트에 이상적

---

## 2-노드 Cluster

하나의 publisher와 하나의 subscriber 노드로 구성된 가장 간단한 분산 설정입니다.

### 아키텍처

```
┌─────────────────────┐         ┌─────────────────────┐
│   Node A (Server)   │         │   Node B (Client)   │
├─────────────────────┤         ├─────────────────────┤
│ Bind: tcp://*:5555  │◄────────┤ Connect to:         │
│                     │         │ tcp://nodeA:5555    │
│ LOCAL topics:       │         │                     │
│ - sensor:temp       │         │ REMOTE topics:      │
│ - sensor:humidity   │         │ - sensor:temp       │
└─────────────────────┘         └─────────────────────┘
```

### Node A (Server)

```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// LOCAL topics 생성
slk_spot_topic_create(spot, "sensor:temperature");
slk_spot_topic_create(spot, "sensor:humidity");

// cluster 연결을 수락하기 위해 bind
slk_spot_bind(spot, "tcp://*:5555");

printf("Node A ready on tcp://*:5555\n");

// 센서 데이터 publish
while (1) {
    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%.1f", read_temperature());
    slk_spot_publish(spot, "sensor:temperature", temp_str, strlen(temp_str));

    slk_sleep(1000); // 1초
}
```

### Node B (Client)

```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// Cluster에 Node A 추가
slk_sleep(100); // Node A의 bind 완료 대기
int rc = slk_spot_cluster_add(spot, "tcp://192.168.1.100:5555");
if (rc != 0) {
    fprintf(stderr, "Failed to add Node A: %s\n", slk_strerror(slk_errno()));
    return -1;
}

// topic 검색을 위한 동기화
rc = slk_spot_cluster_sync(spot, 1000);
if (rc != 0) {
    fprintf(stderr, "Cluster sync failed\n");
}

// REMOTE topic 구독
rc = slk_spot_subscribe(spot, "sensor:temperature");
if (rc != 0) {
    fprintf(stderr, "Subscribe failed: %s\n", slk_strerror(slk_errno()));
}

// 센서 데이터 수신
while (1) {
    char topic[256], data[256];
    size_t topic_len, data_len;

    rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 1000);

    if (rc == 0) {
        topic[topic_len] = '\0';
        data[data_len] = '\0';
        printf("Received: %s = %s\n", topic, data);
    }
}
```

**핵심 포인트:**
- Server가 먼저 bind한 후 client가 connect
- `cluster_sync()`가 remote topics를 검색
- TCP 장애 시 자동 재연결

---

## N-노드 Mesh Topology

모든 노드가 다른 모든 노드에 연결됩니다 (full mesh).

### 아키텍처 (3 노드)

```
        ┌─────────────────┐
        │    Node A       │
        │  tcp://*:5555   │
        └────────┬────────┘
                 │
        ┌────────┼────────┐
        │        │        │
        ▼        ▼        ▼
┌───────────┐ ┌───────────┐
│  Node B   │ │  Node C   │
│tcp://*:5556│ │tcp://*:5557│
└───────┬───┘ └───┬───────┘
        │         │
        └─────────┘
```

**연결:**
- Node A ↔ Node B
- Node A ↔ Node C
- Node B ↔ Node C

### Node A 설정

```c
slk_spot_t *spot = slk_spot_new(ctx);

// LOCAL topics 생성
slk_spot_topic_create(spot, "nodeA:data");

// server 모드를 위해 bind
slk_spot_bind(spot, "tcp://*:5555");

// 다른 노드들에 connect
slk_spot_cluster_add(spot, "tcp://nodeB:5556");
slk_spot_cluster_add(spot, "tcp://nodeC:5557");

// topic 검색을 위한 동기화
slk_spot_cluster_sync(spot, 1000);

// remote topics 구독
slk_spot_subscribe(spot, "nodeB:data");
slk_spot_subscribe(spot, "nodeC:data");
```

### Node B 설정

```c
slk_spot_t *spot = slk_spot_new(ctx);

// LOCAL topics 생성
slk_spot_topic_create(spot, "nodeB:data");

// server 모드를 위해 bind
slk_spot_bind(spot, "tcp://*:5556");

// 다른 노드들에 connect
slk_spot_cluster_add(spot, "tcp://nodeA:5555");
slk_spot_cluster_add(spot, "tcp://nodeC:5557");

// topic 검색을 위한 동기화
slk_spot_cluster_sync(spot, 1000);

// remote topics 구독
slk_spot_subscribe(spot, "nodeA:data");
slk_spot_subscribe(spot, "nodeC:data");
```

### Node C 설정

Node B와 유사하며, endpoint를 적절히 조정합니다.

### 시작 순서

1. **모든 노드 bind** - 각 포트에 bind (병렬 수행 가능)
2. **모든 bind 완료 대기** (예: 100ms 지연)
3. **모든 노드 cluster peer 추가** - `cluster_add()` 사용
4. **모든 노드 동기화** - `cluster_sync(1000)` 사용
5. **노드 구독** - 검색된 topics 구독

**복잡도:**
- N개 노드: N×(N-1)/2 TCP 연결
- 10개 노드: 45개 연결
- 100개 노드: 4,950개 연결

**사용 사례:**
- 고가용성 (단일 장애점 없음)
- 낮은 지연 시간 (직접 peer-to-peer)
- 소규모 cluster (<10 노드)

---

## Hub-Spoke Topology

중앙 hub와 이에 연결되는 spoke 노드들로 구성됩니다.

### 아키텍처

```
                  ┌─────────────┐
                  │  Hub Node   │
                  │tcp://*:5555 │
                  └──────┬──────┘
                         │
       ┌─────────────────┼─────────────────┐
       │                 │                 │
       ▼                 ▼                 ▼
┌──────────┐      ┌──────────┐      ┌──────────┐
│ Spoke 1  │      │ Spoke 2  │      │ Spoke 3  │
│ tcp://*: │      │ tcp://*: │      │ tcp://*: │
│   5556   │      │   5557   │      │   5558   │
└──────────┘      └──────────┘      └──────────┘
```

**연결:**
- Hub ↔ Spoke 1
- Hub ↔ Spoke 2
- Hub ↔ Spoke 3
- **Spoke ↔ Spoke 연결 없음**

### Hub Node 설정

```c
slk_spot_t *hub = slk_spot_new(ctx);

// server 모드를 위해 bind
slk_spot_bind(hub, "tcp://*:5555");

// 모든 spoke에 connect
slk_spot_cluster_add(hub, "tcp://spoke1:5556");
slk_spot_cluster_add(hub, "tcp://spoke2:5557");
slk_spot_cluster_add(hub, "tcp://spoke3:5558");

// spoke topics 검색을 위한 동기화
slk_spot_cluster_sync(hub, 1000);

// 모든 spoke topics 구독 (패턴)
char **topics;
size_t count;
slk_spot_list_topics(hub, &topics, &count);
for (size_t i = 0; i < count; i++) {
    slk_spot_subscribe(hub, topics[i]);
}
slk_spot_list_topics_free(topics, count);

// spoke 간 메시지 전달 (relay 로직)
// ...
```

### Spoke Node 설정

```c
slk_spot_t *spoke = slk_spot_new(ctx);

// LOCAL topics 생성
slk_spot_topic_create(spoke, "spoke1:sensor:temp");

// server 모드를 위해 bind
slk_spot_bind(spoke, "tcp://*:5556");

// hub에만 connect
slk_spot_cluster_add(spoke, "tcp://hub:5555");

// hub topics 검색을 위한 동기화
slk_spot_cluster_sync(spoke, 1000);

// 관심 있는 topics 구독
slk_spot_subscribe(spoke, "hub:config");
```

**메시지 Relay (Hub):**
```c
// Hub가 Spoke 1로부터 메시지 수신
char topic[256], data[4096];
size_t topic_len, data_len;

int rc = slk_spot_recv(hub, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 100);

if (rc == 0) {
    // 다른 모든 spoke로 전달
    // (remote topics로의 명시적 publish 필요)
    // 참고: 현재 SPOT은 직접 forwarding을 지원하지 않음
    // 이것은 향후 개선 사항임
}
```

**복잡도:**
- N개 spoke: N개 TCP 연결 (hub가 N개 연결 보유)
- mesh보다 확장성 좋음
- 단일 장애점 (hub)

**사용 사례:**
- 대규모 cluster (>10 노드)
- 중앙 집중식 모니터링/로깅
- 메시지 브로커링

---

## 장애 처리

### 네트워크 분할

**시나리오:** Node B가 Node A와의 연결을 잃음.

```
┌─────────┐         ┌─────────┐
│ Node A  │    X    │ Node B  │
└─────────┘         └─────────┘
     │                   │
     │                   │
     ▼                   ▼
┌─────────┐         ┌─────────┐
│ Node C  │◄───────►│ Node D  │
└─────────┘         └─────────┘
```

**자동 동작:**
- Node B의 ROUTER 소켓이 TCP disconnect 감지
- Node A topics 구독이 `EHOSTUNREACH`로 실패
- Node B가 자동 재연결 시도 (backoff: 100ms → 5000ms)

**애플리케이션 처리:**
```c
int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, 100);

if (rc != 0) {
    int err = slk_errno();
    if (err == SLK_EAGAIN) {
        // 메시지 없음 (정상)
    } else if (err == SLK_EHOSTUNREACH) {
        // 노드 연결 끊김, 재시도 또는 failover
        slk_sleep(1000);
        slk_spot_cluster_sync(spot, 1000); // 재동기화
    }
}
```

---

### 노드 장애

**시나리오:** Node A가 crash됨.

**감지:**
- TCP keepalive가 죽은 연결 감지 (기본값: 30초)
- SPOT이 노드를 disconnected로 표시
- Node A의 REMOTE topics가 사용 불가 상태가 됨

**복구:**
```c
// 지연 후 수동 재시도
slk_sleep(5000); // 5초 대기

// cluster 노드 재추가
slk_spot_cluster_remove(spot, "tcp://nodeA:5555"); // 정리
slk_spot_cluster_add(spot, "tcp://nodeA:5555");    // 재연결

// 재동기화
if (slk_spot_cluster_sync(spot, 1000) == 0) {
    printf("Node A recovered\n");
}
```

**Failover 예제:**
```c
// Primary와 backup 서버
const char *primary = "tcp://primary:5555";
const char *backup = "tcp://backup:5555";

int rc = slk_spot_topic_route(spot, "critical:topic", primary);
if (rc != 0) {
    // Primary 실패, backup 사용
    rc = slk_spot_topic_route(spot, "critical:topic", backup);
}
```

---

### Split-Brain 시나리오

**시나리오:** Cluster가 두 개의 partition으로 분리됨.

```
Partition 1              Partition 2
┌─────────┐              ┌─────────┐
│ Node A  │              │ Node C  │
│ Node B  │              │ Node D  │
└─────────┘              └─────────┘
```

**현재 동작:**
- 자동 partition 감지 없음
- 각 partition이 독립적으로 동작
- REMOTE topics에 대한 잠재적 메시지 손실

**완화 방안:**
- Quorum 기반 결정 사용 (애플리케이션 레벨)
- 상태 확인 및 모니터링 구현
- Eventual consistency를 위한 설계

---

## 네트워크 고려사항

### 방화벽 설정

**필요 포트:**
```bash
# Node A
iptables -A INPUT -p tcp --dport 5555 -j ACCEPT

# Node B
iptables -A INPUT -p tcp --dport 5556 -j ACCEPT
```

**Docker 네트워킹:**
```yaml
version: '3'
services:
  spot-node-a:
    image: serverlink-spot
    ports:
      - "5555:5555"
    environment:
      - SPOT_BIND=tcp://*:5555
    networks:
      - spot-net

  spot-node-b:
    image: serverlink-spot
    ports:
      - "5556:5556"
    environment:
      - SPOT_BIND=tcp://*:5556
      - SPOT_CLUSTER=tcp://spot-node-a:5555
    networks:
      - spot-net

networks:
  spot-net:
    driver: bridge
```

---

### 지연 시간 최적화

**TCP 튜닝:**
```c
// 낮은 지연 시간을 위해 Nagle 알고리즘 비활성화
int nodelay = 1;
slk_setsockopt(socket, SLK_TCP_NODELAY, &nodelay, sizeof(nodelay));

// TCP keepalive 조정
int keepalive = 1;
int keepalive_idle = 10;  // 10초
int keepalive_intvl = 5;  // 5초
int keepalive_cnt = 3;    // 3번 probe

slk_setsockopt(socket, SLK_TCP_KEEPALIVE, &keepalive, sizeof(keepalive));
slk_setsockopt(socket, SLK_TCP_KEEPALIVE_IDLE, &keepalive_idle, sizeof(keepalive_idle));
slk_setsockopt(socket, SLK_TCP_KEEPALIVE_INTVL, &keepalive_intvl, sizeof(keepalive_intvl));
slk_setsockopt(socket, SLK_TCP_KEEPALIVE_CNT, &keepalive_cnt, sizeof(keepalive_cnt));
```

**참고:** ServerLink는 아직 `SLK_TCP_NODELAY`를 노출하지 않습니다. 이것은 향후 개선 사항입니다.

---

### 대역폭 관리

**HWM 튜닝:**
```c
// 고대역폭 시나리오
slk_spot_set_hwm(spot, 100000, 100000);

// 저대역폭 시나리오 (IoT)
slk_spot_set_hwm(spot, 100, 100);
```

**메시지 배칭:**
```c
// 많은 작은 publish 대신
for (int i = 0; i < 1000; i++) {
    slk_spot_publish(spot, topic, &data[i], sizeof(data[i]));
}

// 더 큰 메시지로 배치
slk_spot_publish(spot, topic, data, sizeof(data));
```

---

## 모니터링과 Observability

### 상태 확인

```c
// cluster 노드 도달 가능 여부 확인
int is_healthy(slk_spot_t *spot, const char *endpoint)
{
    // 짧은 timeout으로 cluster sync 시도
    int rc = slk_spot_cluster_sync(spot, 100);
    return (rc == 0) ? 1 : 0;
}
```

### 메트릭 수집

```c
// publish/subscribe 카운트 추적
typedef struct {
    uint64_t publishes;
    uint64_t subscribes;
    uint64_t recv_msgs;
    uint64_t recv_bytes;
} spot_metrics_t;

spot_metrics_t metrics = {0};

// publish 시 증가
slk_spot_publish(spot, topic, data, len);
metrics.publishes++;

// recv 시 증가
int rc = slk_spot_recv(spot, topic, topic_size, &topic_len,
                       data, data_size, &data_len, flags);
if (rc == 0) {
    metrics.recv_msgs++;
    metrics.recv_bytes += data_len;
}
```

### 로깅

```c
// 상세 로깅 활성화 (애플리케이션 레벨)
#define SPOT_LOG_LEVEL_DEBUG 1

void spot_log(int level, const char *fmt, ...)
{
    if (level <= SPOT_LOG_LEVEL_DEBUG) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

// 사용 예
spot_log(SPOT_LOG_LEVEL_DEBUG, "Cluster sync: discovered %zu topics\n", count);
```

---

## 프로덕션 배포

### 체크리스트

- [ ] **네트워크 보안**: 방화벽 구성, 포트 노출
- [ ] **모니터링**: 상태 확인, 메트릭 수집
- [ ] **로깅**: 중앙 집중식 로깅 (syslog, ELK)
- [ ] **고가용성**: 중복 노드, failover 로직
- [ ] **리소스 제한**: HWM 튜닝, 메모리 제한
- [ ] **테스트**: 부하 테스트, 장애 주입
- [ ] **문서화**: topology 다이어그램, 운영 매뉴얼

### 프로덕션 설정 예제

```c
typedef struct {
    const char *bind_endpoint;
    const char **cluster_peers;
    int peer_count;
    int sndhwm;
    int rcvhwm;
    int sync_timeout_ms;
} spot_config_t;

int spot_init_production(slk_spot_t *spot, const spot_config_t *config)
{
    // endpoint에 bind
    if (slk_spot_bind(spot, config->bind_endpoint) != 0) {
        fprintf(stderr, "Failed to bind to %s\n", config->bind_endpoint);
        return -1;
    }

    // HWM 설정
    if (slk_spot_set_hwm(spot, config->sndhwm, config->rcvhwm) != 0) {
        fprintf(stderr, "Failed to set HWM\n");
        return -1;
    }

    // cluster peer 추가
    for (int i = 0; i < config->peer_count; i++) {
        int rc = slk_spot_cluster_add(spot, config->cluster_peers[i]);
        if (rc != 0) {
            fprintf(stderr, "Warning: Failed to add peer %s\n",
                    config->cluster_peers[i]);
            // 다른 peer들로 계속 진행
        }
    }

    // 초기 sync
    if (slk_spot_cluster_sync(spot, config->sync_timeout_ms) != 0) {
        fprintf(stderr, "Warning: Cluster sync incomplete\n");
        // 치명적이지 않음, 나중에 재시도 가능
    }

    return 0;
}
```

**사용법:**
```c
const char *peers[] = {
    "tcp://node2:5556",
    "tcp://node3:5557"
};

spot_config_t config = {
    .bind_endpoint = "tcp://*:5555",
    .cluster_peers = peers,
    .peer_count = 2,
    .sndhwm = 10000,
    .rcvhwm = 10000,
    .sync_timeout_ms = 1000
};

slk_spot_t *spot = slk_spot_new(ctx);
spot_init_production(spot, &config);
```

---

## 참고 문서

- [API Reference](API.ko.md)
- [Architecture Overview](ARCHITECTURE.ko.md)
- [Protocol Specification](PROTOCOL.ko.md)
- [Usage Patterns](PATTERNS.ko.md)

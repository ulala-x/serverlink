# SPOT Cluster Quick Start Guide

## 5분 안에 클러스터 구축하기

### Step 1: Node A 설정

```cpp
#include "spot/spot_pubsub.hpp"
#include "core/ctx.hpp"

using namespace slk;

// 1. Context와 SPOT 생성
ctx_t ctx;
spot_pubsub_t spot(&ctx);

// 2. 서버 모드 시작 (다른 노드의 연결 수락)
spot.bind("tcp://*:5555");

// 3. 로컬 토픽 생성
spot.topic_create("game:player1");
spot.topic_create("game:score");

// 4. Node B를 클러스터에 추가
spot.cluster_add("tcp://192.168.1.101:5555");

// 5. 토픽 동기화 (Node B의 토픽 가져오기)
spot.cluster_sync(1000);

// 6. 모든 토픽 확인 (LOCAL + REMOTE)
auto topics = spot.list_topics();
for (const auto &topic : topics) {
    printf("%s (%s)\n", topic.c_str(),
           spot.topic_is_local(topic) ? "LOCAL" : "REMOTE");
}

// 7. REMOTE 토픽 구독 가능!
spot.subscribe("chat:room1");  // Node B의 토픽

// 8. 메시지 수신
char topic_buf[256];
char data_buf[1024];
size_t topic_len, data_len;

while (true) {
    int rc = spot.recv(topic_buf, sizeof(topic_buf), &topic_len,
                       data_buf, sizeof(data_buf), &data_len,
                       0);
    if (rc == 0) {
        topic_buf[topic_len] = '\0';
        data_buf[data_len] = '\0';
        printf("Received [%s]: %s\n", topic_buf, data_buf);
    }
}
```

### Step 2: Node B 설정

```cpp
// 1. Context와 SPOT 생성
ctx_t ctx;
spot_pubsub_t spot(&ctx);

// 2. 서버 모드 시작
spot.bind("tcp://*:5555");

// 3. 로컬 토픽 생성
spot.topic_create("chat:room1");
spot.topic_create("chat:lobby");

// 4. Node A를 클러스터에 추가
spot.cluster_add("tcp://192.168.1.100:5555");

// 5. 토픽 동기화
spot.cluster_sync(1000);

// 6. REMOTE 토픽 구독
spot.subscribe("game:player1");  // Node A의 토픽

// 7. 메시지 발행
const char *msg = "Welcome to chat!";
spot.publish("chat:room1", msg, strlen(msg));
```

## 주요 개념

### 1. 양방향 연결

각 노드는 **Server**와 **Client** 역할을 동시에 수행:

```
Node A                Node B
bind(*:5555)         bind(*:5555)
    ↓                     ↓
  ROUTER              ROUTER
    ↑                     ↑
    |   cluster_add()     |
    |─────────→ DEALER ───┘
    |
    └─── DEALER ←─────────|
              cluster_add()
```

### 2. 토픽 동기화

`cluster_sync()` 호출 시:

```
1. QUERY 전송      → 모든 클러스터 노드에
2. QUERY_RESP 수신 → 각 노드의 토픽 목록
3. Registry 업데이트 → REMOTE 토픽으로 등록
```

### 3. 위치 투명성 (Location Transparency)

`subscribe()` 호출 시 LOCAL/REMOTE 구분 불필요:

```cpp
// 내부적으로 registry가 LOCAL/REMOTE 판단
spot.subscribe("any:topic");

// LOCAL: XSUB → XPUB (inproc)
// REMOTE: spot_node_t → DEALER → TCP
```

## 일반적인 패턴

### Pattern 1: Master-Worker

```cpp
// Master
spot.bind("tcp://*:5555");
spot.topic_create("tasks");
spot.publish("tasks", task_data, size);

// Workers
spot.cluster_add("tcp://master:5555");
spot.cluster_sync(1000);
spot.subscribe("tasks");
```

### Pattern 2: Pub-Sub Mesh

```cpp
// 모든 노드가 동등
spot.bind("tcp://*:5555");
spot.cluster_add("tcp://node1:5555");
spot.cluster_add("tcp://node2:5555");
spot.cluster_add("tcp://node3:5555");
spot.cluster_sync(1000);
```

### Pattern 3: Regional Clusters

```cpp
// Region A
spot.bind("tcp://*:5555");
spot.cluster_add("tcp://region-b-gateway:5555");

// Region B
spot.bind("tcp://*:5555");
spot.cluster_add("tcp://region-a-gateway:5555");
```

## 문제 해결

### Q: cluster_sync()가 실패합니다

**A**: 다음을 확인하세요:
1. 상대 노드가 `bind()` 했는지 확인
2. 네트워크 연결 확인 (방화벽, 포트)
3. `cluster_add()`가 성공했는지 확인

```cpp
if (spot.cluster_add("tcp://192.168.1.100:5555") != 0) {
    printf("Failed to add node: %s\n", strerror(errno));
    // errno == EHOSTUNREACH: 연결 실패
}
```

### Q: REMOTE 토픽을 못 찾습니다

**A**: `cluster_sync()` 호출 후 확인:

```cpp
spot.cluster_sync(1000);

if (!spot.topic_exists("remote:topic")) {
    printf("Topic not found after sync\n");
    // 1. 상대 노드가 해당 토픽을 생성했는지 확인
    // 2. cluster_sync() 성공했는지 확인
}
```

### Q: 메시지가 수신되지 않습니다

**A**: 구독 순서 확인:

```cpp
// 올바른 순서
spot.cluster_add("tcp://node:5555");
spot.cluster_sync(1000);          // 먼저 sync
spot.subscribe("remote:topic");   // 그 다음 subscribe

// 잘못된 순서
spot.subscribe("remote:topic");   // ❌ 토픽이 아직 registry에 없음
spot.cluster_sync(1000);
```

## 성능 팁

### 1. 초기에 한 번만 sync

```cpp
// 시작 시
spot.cluster_sync(1000);

// 이후에는 필요 없음 (자동으로 유지됨)
// spot.cluster_sync(1000);  // ❌ 불필요
```

### 2. HWM 설정

```cpp
// 클러스터 노드가 많으면 HWM 증가
spot.set_hwm(10000, 10000);  // send/recv HWM
```

### 3. 배치 구독

```cpp
// 여러 토픽을 한 번에 구독
std::vector<std::string> topics = {
    "game:player1",
    "game:score",
    "chat:room1"
};
spot.subscribe_many(topics);
```

## 다음 단계

1. **예제 실행**: `examples/spot_cluster_sync_example.cpp`
2. **전체 문서**: `docs/SPOT_CLUSTER_SYNC.md`
3. **아키텍처**: `docs/SPOT_ARCHITECTURE.md`

## 요약

```cpp
// 클러스터 구축 3단계
spot.bind("tcp://*:5555");              // 1. 서버 시작
spot.cluster_add("tcp://node:5555");    // 2. 노드 추가
spot.cluster_sync(1000);                 // 3. 동기화

// 이제 모든 토픽 사용 가능!
spot.subscribe("any:topic");
```

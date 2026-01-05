[![English](https://img.shields.io/badge/lang:en-red.svg)](QUICK_START.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](QUICK_START.ko.md)

# SPOT PUB/SUB 빠른 시작 가이드

ServerLink SPOT (Scalable Partitioned Ordered Topics)을 5분 안에 시작하세요.

## 목차

1. [SPOT이란?](#spot이란)
2. [설치](#설치)
3. [첫 번째 SPOT 애플리케이션](#첫-번째-spot-애플리케이션)
4. [LOCAL Topic](#local-topic)
5. [REMOTE Topic](#remote-topic)
6. [다음 단계](#다음-단계)

---

## SPOT이란?

**SPOT** (Scalable Partitioned Ordered Topics)은 ServerLink 기반의 위치 투명 Pub/Sub 시스템입니다.

**주요 기능:**
- **위치 투명성**: Topic 위치를 몰라도 Subscribe 가능
- **LOCAL Topic**: Zero-copy inproc 메시징 (나노초 지연)
- **REMOTE Topic**: 자동 라우팅이 포함된 TCP 네트워킹
- **Cluster Sync**: 여러 노드에서 Topic 자동 탐색
- **Pattern Matching**: 와일드카드로 여러 Topic에 Subscribe

**사용 사례:**
- 게임 서버 (플레이어 이벤트, 채팅방)
- 마이크로서비스 (이벤트 배포)
- IoT 시스템 (센서 데이터 라우팅)
- 실시간 분석 (데이터 집계)

---

## 설치

### ServerLink 빌드

```bash
# 저장소 복제
git clone https://github.com/ulala-x/serverlink.git
cd serverlink

# CMake로 빌드
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build --config Release

# 테스트 실행
cd build && ctest -C Release
```

### 프로젝트에 연결

**CMake:**
```cmake
find_package(ServerLink REQUIRED)
target_link_libraries(your_app ServerLink::serverlink)
```

**수동:**
```bash
gcc your_app.c -lserverlink -o your_app
```

---

## 첫 번째 SPOT 애플리케이션

### 간단한 Pub/Sub (단일 프로세스)

```c
#include <serverlink/serverlink.h>
#include <stdio.h>
#include <string.h>

int main()
{
    // Context와 SPOT 인스턴스 생성
    slk_ctx_t *ctx = slk_ctx_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    // LOCAL Topic 생성
    slk_spot_topic_create(spot, "chat:lobby");

    // Topic에 Subscribe
    slk_spot_subscribe(spot, "chat:lobby");

    // 메시지 Publish
    const char *msg = "Hello, SPOT!";
    slk_spot_publish(spot, "chat:lobby", msg, strlen(msg));

    // 메시지 도착 대기 (inproc은 빠름!)
    slk_sleep(10);

    // 메시지 수신
    char topic[256], data[4096];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, SLK_DONTWAIT);

    if (rc == 0) {
        topic[topic_len] = '\0';
        data[data_len] = '\0';
        printf("Received on '%s': %s\n", topic, data);
    }

    // 정리
    slk_spot_destroy(&spot);
    slk_ctx_destroy(ctx);

    return 0;
}
```

**예상 출력:**
```
Received on 'chat:lobby': Hello, SPOT!
```

---

## LOCAL Topic

LOCAL Topic은 Zero-copy 동일 프로세스 메시징을 위해 inproc 전송을 사용합니다.

### 여러 Topic 생성

```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// 게임 관련 Topic 생성
slk_spot_topic_create(spot, "game:player:spawn");
slk_spot_topic_create(spot, "game:player:death");
slk_spot_topic_create(spot, "game:score:update");

// 특정 Topic에 Subscribe
slk_spot_subscribe(spot, "game:player:spawn");
slk_spot_subscribe(spot, "game:player:death");
```

### Pattern Subscribe

```c
// 모든 플레이어 이벤트에 Subscribe
slk_spot_subscribe_pattern(spot, "game:player:*");

// 이제 다음 Topic에서 메시지를 수신:
// - game:player:spawn
// - game:player:death
// - game:player:move
// - 등등
```

### Publish와 수신

```c
// 여러 Topic에 Publish
slk_spot_publish(spot, "game:player:spawn", "Player1", 7);
slk_spot_publish(spot, "game:player:death", "Player2", 7);
slk_spot_publish(spot, "game:score:update", "1000", 4);

slk_sleep(10); // 메시지 전파 대기

// 메시지 수신
for (int i = 0; i < 2; i++) { // 2개 메시지 예상 (Pattern 매칭)
    char topic[256], data[256];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, 100);

    if (rc == 0) {
        topic[topic_len] = '\0';
        data[data_len] = '\0';
        printf("Topic: %s, Data: %s\n", topic, data);
    }
}
```

---

## REMOTE Topic

REMOTE Topic은 다른 SPOT 노드로 TCP 연결을 통해 메시지를 라우팅합니다.

### 2노드 설정

**Node 1 (서버):**
```c
#include <serverlink/serverlink.h>

void run_server()
{
    slk_ctx_t *ctx = slk_ctx_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    // LOCAL Topic 생성
    slk_spot_topic_create(spot, "sensor:temperature");

    // 연결 수락을 위해 bind()
    slk_spot_bind(spot, "tcp://*:5555");

    // 센서 데이터 Publish
    while (1) {
        slk_spot_publish(spot, "sensor:temperature", "25.5", 4);
        slk_sleep(1000); // 1초
    }
}
```

**Node 2 (클라이언트):**
```c
#include <serverlink/serverlink.h>
#include <stdio.h>

void run_client()
{
    slk_ctx_t *ctx = slk_ctx_new();
    slk_spot_t *spot = slk_spot_new(ctx);

    // Topic을 원격 서버로 라우팅
    slk_spot_topic_route(spot, "sensor:temperature", "tcp://localhost:5555");

    // REMOTE Topic에 Subscribe
    slk_spot_subscribe(spot, "sensor:temperature");

    // 센서 데이터 수신
    while (1) {
        char topic[256], data[256];
        size_t topic_len, data_len;

        int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                               data, sizeof(data), &data_len, 1000);

        if (rc == 0) {
            data[data_len] = '\0';
            printf("Temperature: %s°C\n", data);
        }
    }
}
```

---

## Cluster 동기화

여러 노드에서 Topic을 자동으로 탐색합니다.

### 3노드 메시 Cluster

**Node A:**
```c
slk_spot_t *spot = slk_spot_new(ctx);

// LOCAL Topic 생성
slk_spot_topic_create(spot, "nodeA:data");
slk_spot_bind(spot, "tcp://*:5555");

// 다른 노드에 connect()
slk_spot_cluster_add(spot, "tcp://nodeB:5556");
slk_spot_cluster_add(spot, "tcp://nodeC:5557");

// 동기화 (nodeB와 nodeC에서 Topic 탐색)
slk_spot_cluster_sync(spot, 1000);

// 이제 nodeB와 nodeC의 Topic에 Subscribe 가능
slk_spot_subscribe(spot, "nodeB:data");
slk_spot_subscribe(spot, "nodeC:data");
```

**Node B와 C:** 다른 포트로 유사하게 설정.

### 모든 Topic 나열

```c
char **topics;
size_t count;

slk_spot_cluster_sync(spot, 1000);
slk_spot_list_topics(spot, &topics, &count);

printf("All topics in cluster:\n");
for (size_t i = 0; i < count; i++) {
    int is_local = slk_spot_topic_is_local(spot, topics[i]);
    printf("  - %s (%s)\n", topics[i], is_local ? "LOCAL" : "REMOTE");
}

slk_spot_list_topics_free(topics, count);
```

**출력:**
```
All topics in cluster:
  - nodeA:data (LOCAL)
  - nodeB:data (REMOTE)
  - nodeC:data (REMOTE)
```

---

## Event Loop 통합

비차단 I/O를 위해 `poll()` 또는 `epoll()`과 함께 사용합니다.

```c
#include <poll.h>

slk_spot_t *spot = slk_spot_new(ctx);
slk_spot_topic_create(spot, "events");
slk_spot_subscribe(spot, "events");

// Poll 가능한 파일 디스크립터 가져오기
slk_fd_t spot_fd;
slk_spot_fd(spot, &spot_fd);

struct pollfd pfd = {
    .fd = spot_fd,
    .events = POLLIN
};

while (1) {
    int rc = poll(&pfd, 1, 1000);

    if (rc > 0 && (pfd.revents & POLLIN)) {
        char topic[256], data[4096];
        size_t topic_len, data_len;

        slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                      data, sizeof(data), &data_len, SLK_DONTWAIT);

        // 메시지 처리
    }
}
```

---

## 오류 처리

항상 반환 값을 확인하고 `slk_errno()`를 사용하세요.

```c
int rc = slk_spot_publish(spot, "topic", data, len);
if (rc != 0) {
    int err = slk_errno();

    if (err == SLK_ENOENT) {
        fprintf(stderr, "Topic not found\n");
    } else if (err == SLK_EAGAIN) {
        fprintf(stderr, "Send buffer full, retry later\n");
    } else {
        fprintf(stderr, "Error: %s\n", slk_strerror(err));
    }
}
```

---

## 일반적인 패턴

### Producer-Consumer

```c
// Producer
slk_spot_topic_create(spot, "jobs:queue");
for (int i = 0; i < 100; i++) {
    char job[64];
    snprintf(job, sizeof(job), "job_%d", i);
    slk_spot_publish(spot, "jobs:queue", job, strlen(job));
}

// Consumer
slk_spot_subscribe(spot, "jobs:queue");
while (1) {
    char topic[256], data[256];
    size_t topic_len, data_len;

    int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                           data, sizeof(data), &data_len, -1); // 차단

    if (rc == 0) {
        // 작업 처리
        data[data_len] = '\0';
        printf("Processing: %s\n", data);
    }
}
```

### Fan-Out (1:N)

```c
// Publisher
slk_spot_topic_create(spot, "broadcast");

// 여러 Subscriber
slk_spot_subscribe(spot_sub1, "broadcast");
slk_spot_subscribe(spot_sub2, "broadcast");
slk_spot_subscribe(spot_sub3, "broadcast");

// 모든 Subscriber가 메시지 수신
slk_spot_publish(spot, "broadcast", "announcement", 12);
```

### Fan-In (N:1)

```c
// 여러 Publisher가 다른 Topic으로 Publish
slk_spot_topic_create(spot1, "source1");
slk_spot_topic_create(spot2, "source2");
slk_spot_topic_create(spot3, "source3");

// Pattern으로 단일 Subscriber
slk_spot_subscribe_pattern(aggregator, "source*");

// 모든 소스에서 메시지 수신
```

---

## 성능 팁

1. **LOCAL Topic 사용**: 동일 프로세스 통신용 (Zero-copy)
2. **HWM 증가**: 고처리량 시나리오용:
   ```c
   slk_spot_set_hwm(spot, 100000, 100000);
   ```
3. **Publish 일괄 처리**: 가능할 때
4. **비차단 recv() 사용**: Event Loop 통합과 함께
5. **Pattern Subscribe**: CPU 오버헤드가 있으므로 적절히 사용

---

## 다음 단계

기본을 이해했으니 다음을 살펴보세요:

1. **[아키텍처 가이드](ARCHITECTURE.md)** - 내부 설계와 데이터 흐름
2. **[프로토콜 명세](PROTOCOL.md)** - 메시지 형식과 명령
3. **[클러스터링 가이드](CLUSTERING.md)** - 멀티 노드 배포 패턴
4. **[API 레퍼런스](API.md)** - 전체 함수 문서
5. **[사용 패턴](PATTERNS.md)** - 일반적인 설계 패턴

---

## 문제 해결

### 메시지가 수신되지 않음

```c
// Topic 존재 확인
if (!slk_spot_topic_exists(spot, "topic")) {
    fprintf(stderr, "Topic not found\n");
}

// Subscribe 확인
slk_spot_subscribe(spot, "topic"); // 멱등성

// inproc 전파를 위한 지연 추가
slk_sleep(10);
```

### 연결 거부 (REMOTE Topic)

```c
// 서버가 먼저 bind() 되었는지 확인
slk_spot_bind(server_spot, "tcp://*:5555");

// 그 다음 클라이언트 connect()
int rc = slk_spot_topic_route(client_spot, "topic", "tcp://server:5555");
if (rc != 0 && slk_errno() == SLK_EHOSTUNREACH) {
    fprintf(stderr, "Server not reachable\n");
}
```

### HWM 오류 (EAGAIN)

```c
// HWM 증가
slk_spot_set_hwm(spot, 100000, 100000);

// 또는 백프레셔 처리
if (slk_errno() == SLK_EAGAIN) {
    slk_sleep(10);
    // Publish 재시도
}
```

---

## 전체 예제

Cluster 동기화의 전체 작동 예제는 `examples/spot_cluster_sync_example.cpp`를 참조하세요.

**예제 실행:**
```bash
cd build
./examples/spot_cluster_sync_example
```

---

## 도움 받기

- **GitHub Issues**: https://github.com/ulala-x/serverlink/issues
- **문서**: `docs/spot/`
- **예제**: `examples/`
- **테스트**: `tests/spot/`

SPOT과 함께 즐거운 메시징 되세요!

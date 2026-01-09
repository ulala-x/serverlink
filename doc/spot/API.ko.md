[![English](https://img.shields.io/badge/lang:en-red.svg)](API.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](API.ko.md)

# SPOT PUB/SUB API 레퍼런스

ServerLink SPOT (Scalable Partitioned Ordered Topics) 전체 API 레퍼런스입니다.

## 목차

- [개요](#개요)
- [데이터 타입](#데이터-타입)
- [생명주기 관리](#생명주기-관리)
- [Topic 관리](#topic-관리)
- [Publish 및 Subscribe](#publish-및-subscribe)
- [클러스터 관리](#클러스터-관리)
- [인트로스펙션](#인트로스펙션)
- [설정](#설정)
- [이벤트 루프 통합](#이벤트-루프-통합)
- [에러 처리](#에러-처리)

---

## 개요

SPOT은 Topic ID 기반 라우팅을 사용하는 위치 투명 pub/sub을 제공합니다. Topic은 **LOCAL** (이 노드에서 호스팅) 또는 **REMOTE** (다른 노드로 라우팅)일 수 있습니다.

**주요 기능:**
- Topic 소유권 및 등록
- 정확한 매칭 및 패턴 구독
- 위치 투명 publish/subscribe (inproc/tcp)
- 분산 Topic을 위한 클러스터 동기화
- LOCAL Topic을 위한 제로 카피 메시지 전달
- 자동 장애 조치 및 재연결

---

## 데이터 타입

### slk_spot_t

```c
typedef struct slk_spot_s slk_spot_t;
```

SPOT PUB/SUB 인스턴스에 대한 불투명 핸들입니다. 모든 SPOT 작업에는 이 핸들이 필요합니다.

---

## 생명주기 관리

### slk_spot_new

```c
slk_spot_t* slk_spot_new(slk_ctx_t *ctx);
```

새로운 SPOT PUB/SUB 인스턴스를 생성합니다.

**Parameters:**
- `ctx` - ServerLink context (NULL이 아니어야 함)

**Returns:**
- 성공 시 새로운 SPOT 인스턴스
- 오류 시 `NULL` (errno 설정)

**Error Codes:**
- `ENOMEM` - 메모리 부족
- `EINVAL` - 유효하지 않은 context

**Example:**
```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);
if (!spot) {
    fprintf(stderr, "Failed to create SPOT: %s\n", slk_strerror(slk_errno()));
    return -1;
}
```

**Notes:**
- 메시지 수신을 위한 내부 XSUB Socket 생성
- 기본 HWM: 1000 메시지 (송신 및 수신)
- 스레드 안전

---

### slk_spot_destroy

```c
void slk_spot_destroy(slk_spot_t **spot);
```

SPOT PUB/SUB 인스턴스를 파괴하고 모든 리소스를 해제합니다.

**Parameters:**
- `spot` - SPOT 인스턴스 포인터 (반환 시 NULL로 설정됨)

**Example:**
```c
slk_spot_destroy(&spot);
// spot is now NULL
```

**Notes:**
- 모든 Socket 종료 (XPUB, XSUB, ROUTER)
- 모든 Topic 등록 해제
- 모든 클러스터 노드 연결 해제
- NULL 포인터로 호출해도 안전

---

## Topic 관리

### slk_spot_topic_create

```c
int slk_spot_topic_create(slk_spot_t *spot, const char *topic_id);
```

**LOCAL** Topic을 생성합니다 (이 노드가 publisher).

**Parameters:**
- `spot` - SPOT 인스턴스
- `topic_id` - Topic 식별자 (null 종료 문자열)

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Error Codes:**
- `EEXIST` - Topic이 이미 존재함
- `ENOMEM` - 메모리 부족
- `EINVAL` - 유효하지 않은 매개변수

**Example:**
```c
int rc = slk_spot_topic_create(spot, "game:player123");
if (rc != 0) {
    fprintf(stderr, "Failed to create topic: %s\n", slk_strerror(slk_errno()));
}
```

**Notes:**
- 고유한 inproc endpoint에 바인딩된 XPUB Socket 생성
- Topic이 로컬 소유가 됨
- 즉시 publish 가능
- Topic ID는 클러스터 전체에서 고유해야 함

**Topic 명명 규칙:**
- 콜론으로 구분된 계층 구조 사용: `"game:player:123"`
- 도메인 접두사 사용: `"chat:room:lobby"`
- 특수 문자 피하기: `/ \ * ? < > |`

---

### slk_spot_topic_route

```c
int slk_spot_topic_route(slk_spot_t *spot, const char *topic_id,
                          const char *endpoint);
```

Topic을 **REMOTE** endpoint로 라우팅합니다.

**Parameters:**
- `spot` - SPOT 인스턴스
- `topic_id` - Topic 식별자
- `endpoint` - 원격 endpoint (예: `"tcp://192.168.1.100:5555"`)

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Error Codes:**
- `EEXIST` - Topic이 이미 존재함
- `EHOSTUNREACH` - endpoint 연결 실패
- `EINVAL` - 유효하지 않은 endpoint 형식

**Example:**
```c
int rc = slk_spot_topic_route(spot, "remote:sensor", "tcp://192.168.1.100:5555");
if (rc != 0) {
    fprintf(stderr, "Failed to route topic: %s\n", slk_strerror(slk_errno()));
}
```

**Notes:**
- 필요한 경우 원격 노드에 연결 설정
- 레지스트리에 Topic을 REMOTE로 등록
- 여러 Topic이 동일한 endpoint를 공유 가능
- 자동 재연결이 포함된 영구 연결

---

### slk_spot_topic_destroy

```c
int slk_spot_topic_destroy(slk_spot_t *spot, const char *topic_id);
```

Topic을 파괴하고 등록을 해제합니다.

**Parameters:**
- `spot` - SPOT 인스턴스
- `topic_id` - Topic 식별자

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Error Codes:**
- `ENOENT` - Topic을 찾을 수 없음

**Example:**
```c
int rc = slk_spot_topic_destroy(spot, "game:player123");
```

**Notes:**
- Topic의 XPUB Socket 종료 (LOCAL인 경우)
- 레지스트리에서 제거
- 활성 구독은 영향받지 않음 (다음 사용 시 실패)

---

## Publish 및 Subscribe

### slk_spot_subscribe

```c
int slk_spot_subscribe(slk_spot_t *spot, const char *topic_id);
```

Topic을 구독합니다 (LOCAL 또는 REMOTE).

**Parameters:**
- `spot` - SPOT 인스턴스
- `topic_id` - Topic 식별자

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Error Codes:**
- `ENOENT` - 레지스트리에서 Topic을 찾을 수 없음
- `EEXIST` - 이미 구독됨 (멱등성, 오류 아님)

**Example:**
```c
// Subscribe to LOCAL topic
int rc = slk_spot_subscribe(spot, "game:player123");

// Subscribe to REMOTE topic
rc = slk_spot_subscribe(spot, "remote:sensor");
```

**Notes:**
- **LOCAL topics:** XSUB를 Topic의 inproc endpoint에 연결
- **REMOTE topics:** 원격 노드에 SUBSCRIBE 명령 전송
- 멱등성: 두 번 호출해도 안전
- 명시적 구독 취소 또는 파괴까지 구독 유지

---

### slk_spot_subscribe_pattern

```c
int slk_spot_subscribe_pattern(slk_spot_t *spot, const char *pattern);
```

패턴을 구독합니다 (**LOCAL topics 전용**).

**Parameters:**
- `spot` - SPOT 인스턴스
- `pattern` - 선택적 `*` 와일드카드가 포함된 패턴 문자열

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Example:**
```c
// Subscribe to all player topics
slk_spot_subscribe_pattern(spot, "game:player:*");

// Subscribe to all chat messages
slk_spot_subscribe_pattern(spot, "chat:*");
```

**패턴 매칭 규칙:**
- `*`는 0개 이상의 문자와 매칭
- 접두사 매칭: `"game:*"`는 `"game:player"`, `"game:score"`와 매칭
- LOCAL topics에서만 동작
- 여러 패턴을 동시에 활성화 가능

**Notes:**
- 패턴 구독은 LOCAL 전용
- REMOTE topics에서는 패턴 매칭 없음
- `slk_spot_recv()` 중에 필터링

---

### slk_spot_unsubscribe

```c
int slk_spot_unsubscribe(slk_spot_t *spot, const char *topic_id);
```

Topic 구독을 취소합니다.

**Parameters:**
- `spot` - SPOT 인스턴스
- `topic_id` - Topic 식별자

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Error Codes:**
- `ENOENT` - 구독하지 않았거나 Topic을 찾을 수 없음

**Example:**
```c
int rc = slk_spot_unsubscribe(spot, "game:player123");
```

**Notes:**
- **LOCAL topics:** XPUB에 구독 취소 메시지 전송
- **REMOTE topics:** 원격 노드에 UNSUBSCRIBE 명령 전송
- 멱등성: 여러 번 호출해도 안전

---

### slk_spot_publish

```c
int slk_spot_publish(slk_spot_t *spot, const char *topic_id,
                      const void *data, size_t len);
```

Topic에 메시지를 publish합니다.

**Parameters:**
- `spot` - SPOT 인스턴스
- `topic_id` - Topic 식별자
- `data` - 메시지 데이터 포인터
- `len` - 메시지 데이터 길이 (바이트)

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Error Codes:**
- `ENOENT` - Topic을 찾을 수 없음
- `EAGAIN` - HWM 도달 (논블로킹 모드)
- `EINVAL` - 유효하지 않은 매개변수

**Message Format:**
```
Frame 0: topic_id (가변 길이)
Frame 1: data (가변 길이)
```

**Example:**
```c
const char *msg = "Player joined!";
int rc = slk_spot_publish(spot, "game:player123", msg, strlen(msg));
if (rc != 0) {
    if (slk_errno() == SLK_EAGAIN) {
        fprintf(stderr, "Send buffer full\n");
    }
}
```

**Notes:**
- **LOCAL topics:** XPUB Socket으로 전송
- **REMOTE topics:** 원격 노드에 PUBLISH 명령 전송
- LOCAL topics의 경우 제로 카피 (inproc)
- HWM 제한 적용 (기본값: 1000 메시지)

---

### slk_spot_recv

```c
int slk_spot_recv(slk_spot_t *spot, char *topic, size_t topic_size,
                   size_t *topic_len, void *data, size_t data_size,
                   size_t *data_len, int flags);
```

메시지를 수신합니다 (Topic과 데이터 분리).

**Parameters:**
- `spot` - SPOT 인스턴스
- `topic` - Topic ID 출력 버퍼
- `topic_size` - Topic 버퍼 크기
- `topic_len` - 출력: 실제 Topic 길이
- `data` - 메시지 데이터 출력 버퍼
- `data_size` - 데이터 버퍼 크기
- `data_len` - 출력: 실제 데이터 길이
- `flags` - 수신 플래그 (논블로킹용 `SLK_DONTWAIT`)

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Error Codes:**
- `EAGAIN` - 사용 가능한 메시지 없음 (논블로킹)
- `EMSGSIZE` - 메시지에 비해 버퍼가 너무 작음
- `EINVAL` - 유효하지 않은 매개변수

**Example:**
```c
char topic[256], data[4096];
size_t topic_len, data_len;

int rc = slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
                       data, sizeof(data), &data_len, SLK_DONTWAIT);
if (rc == 0) {
    topic[topic_len] = '\0';
    printf("Received on topic '%s': %zu bytes\n", topic, data_len);
} else if (slk_errno() == SLK_EAGAIN) {
    // No message available
}
```

**Notes:**
- LOCAL topics 먼저 확인 (XSUB Socket)
- 그다음 REMOTE topics 확인 (모든 노드)
- 클러스터 노드의 QUERY 요청 처리
- 블로킹 모드는 LOCAL Socket에서만 대기
- 수신 중 패턴 필터링 적용

---

## 클러스터 관리

### slk_spot_bind

```c
int slk_spot_bind(slk_spot_t *spot, const char *endpoint);
```

서버 모드용 endpoint에 바인딩합니다 (클러스터 연결 수락).

**Parameters:**
- `spot` - SPOT 인스턴스
- `endpoint` - 바인드 endpoint (예: `"tcp://*:5555"`)

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Error Codes:**
- `EEXIST` - 이미 바인딩됨
- `EADDRINUSE` - 주소가 이미 사용 중

**Example:**
```c
int rc = slk_spot_bind(spot, "tcp://*:5555");
if (rc != 0) {
    fprintf(stderr, "Failed to bind: %s\n", slk_strerror(slk_errno()));
}
```

**Notes:**
- 연결 수락을 위한 ROUTER Socket 생성
- 클러스터 동기화에 필요
- `slk_spot_cluster_sync()` 전에 호출해야 함
- endpoint 형식: `tcp://interface:port` 또는 `tcp://*:port`

---

### slk_spot_cluster_add

```c
int slk_spot_cluster_add(slk_spot_t *spot, const char *endpoint);
```

클러스터 노드를 추가합니다 (원격 SPOT 노드에 연결 설정).

**Parameters:**
- `spot` - SPOT 인스턴스
- `endpoint` - 원격 노드 endpoint (예: `"tcp://192.168.1.100:5555"`)

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Error Codes:**
- `EEXIST` - 노드가 이미 추가됨
- `EHOSTUNREACH` - 연결 실패

**Example:**
```c
int rc = slk_spot_cluster_add(spot, "tcp://192.168.1.100:5555");
```

**Notes:**
- 영구 연결 설정
- 실패 시 자동 재연결
- Topic 검색을 위해 `slk_spot_cluster_sync()`와 함께 사용

---

### slk_spot_cluster_remove

```c
int slk_spot_cluster_remove(slk_spot_t *spot, const char *endpoint);
```

클러스터 노드를 제거합니다 (원격 SPOT 노드 연결 해제).

**Parameters:**
- `spot` - SPOT 인스턴스
- `endpoint` - 원격 노드 endpoint

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Error Codes:**
- `ENOENT` - 노드를 찾을 수 없음

**Example:**
```c
int rc = slk_spot_cluster_remove(spot, "tcp://192.168.1.100:5555");
```

**Notes:**
- 원격 노드 연결 종료
- 이 노드와 연관된 모든 REMOTE topics 제거
- 제거된 Topic에 대한 활성 구독은 실패

---

### slk_spot_cluster_sync

```c
int slk_spot_cluster_sync(slk_spot_t *spot, int timeout_ms);
```

클러스터 노드와 Topic을 동기화합니다.

**Parameters:**
- `spot` - SPOT 인스턴스
- `timeout_ms` - 동기화 작업 타임아웃 (밀리초)

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Protocol:**
1. 모든 클러스터 노드에 QUERY 명령 전송
2. Topic 목록이 포함된 QUERY_RESP 수신
3. 검색된 Topic을 REMOTE로 등록

**Example:**
```c
// After adding cluster nodes
int rc = slk_spot_cluster_sync(spot, 1000); // 1 second timeout
if (rc == 0) {
    printf("Cluster synchronized\n");
}
```

**Notes:**
- 먼저 `slk_spot_bind()`를 호출해야 함
- 원격 Topic으로 로컬 레지스트리 업데이트
- 개별 노드에 대해 논블로킹
- 일부 노드가 타임아웃되어도 성공 반환

---

## 인트로스펙션

### slk_spot_list_topics

```c
int slk_spot_list_topics(slk_spot_t *spot, char ***topics, size_t *count);
```

등록된 모든 Topic을 나열합니다 (LOCAL + REMOTE).

**Parameters:**
- `spot` - SPOT 인스턴스
- `topics` - 출력: Topic ID 문자열 배열
- `count` - 출력: Topic 수

**Returns:**
- 성공 시 `0`
- 오류 시 `-1` (errno 설정)

**Example:**
```c
char **topics;
size_t count;

if (slk_spot_list_topics(spot, &topics, &count) == 0) {
    for (size_t i = 0; i < count; i++) {
        printf("Topic %zu: %s\n", i, topics[i]);
    }
    slk_spot_list_topics_free(topics, count);
}
```

**Notes:**
- 동적으로 할당된 배열 반환
- 호출자는 `slk_spot_list_topics_free()`를 사용하여 해제해야 함

---

### slk_spot_list_topics_free

```c
void slk_spot_list_topics_free(char **topics, size_t count);
```

`slk_spot_list_topics`가 반환한 Topic 목록을 해제합니다.

**Parameters:**
- `topics` - Topic ID 문자열 배열
- `count` - Topic 수

---

### slk_spot_topic_exists

```c
int slk_spot_topic_exists(slk_spot_t *spot, const char *topic_id);
```

Topic이 존재하는지 확인합니다.

**Parameters:**
- `spot` - SPOT 인스턴스
- `topic_id` - Topic 식별자

**Returns:**
- Topic이 존재하면 `1`
- 찾을 수 없으면 `0`
- 오류 시 `-1`

**Example:**
```c
if (slk_spot_topic_exists(spot, "game:player123")) {
    printf("Topic exists\n");
}
```

---

### slk_spot_topic_is_local

```c
int slk_spot_topic_is_local(slk_spot_t *spot, const char *topic_id);
```

Topic이 LOCAL인지 확인합니다.

**Parameters:**
- `spot` - SPOT 인스턴스
- `topic_id` - Topic 식별자

**Returns:**
- Topic이 LOCAL이면 `1`
- REMOTE이거나 찾을 수 없으면 `0`
- 오류 시 `-1`

**Example:**
```c
if (slk_spot_topic_is_local(spot, "game:player123")) {
    printf("Topic is LOCAL\n");
} else {
    printf("Topic is REMOTE or not found\n");
}
```

---

## 설정

### slk_spot_set_hwm

```c
int slk_spot_set_hwm(slk_spot_t *spot, int sndhwm, int rcvhwm);
```

High water marks를 설정합니다 (메시지 큐 제한).

**Parameters:**
- `spot` - SPOT 인스턴스
- `sndhwm` - 송신 high water mark (메시지)
- `rcvhwm` - 수신 high water mark (메시지)

**Returns:**
- 성공 시 `0`
- 오류 시 `-1`

**Example:**
```c
// Set limits to 10,000 messages
int rc = slk_spot_set_hwm(spot, 10000, 10000);
```

**Notes:**
- 기본값: 송신 및 수신 모두 1000 메시지
- 기존 및 향후 모든 Socket에 적용
- HWM 도달 시:
  - 블로킹 모드: 공간이 확보될 때까지 블록
  - 논블로킹 모드: `EAGAIN` 반환

---

## 이벤트 루프 통합

### slk_spot_fd

```c
int slk_spot_fd(slk_spot_t *spot, slk_fd_t *fd);
```

수신 Socket의 poll 가능한 file descriptor를 가져옵니다.

**Parameters:**
- `spot` - SPOT 인스턴스
- `fd` - 출력: file descriptor

**Returns:**
- 성공 시 `0`
- 오류 시 `-1`

**Example:**
```c
slk_fd_t fd;
if (slk_spot_fd(spot, &fd) == 0) {
    // Use with poll/epoll/select
}
```

**Notes:**
- XSUB Socket (LOCAL topics)용 FD 반환
- `poll()`, `epoll()`, 또는 `select()`와 함께 사용
- REMOTE topics의 경우 개별 노드 FD를 poll

---

## 에러 처리

### Error Codes

모든 함수는 오류 시 `-1`을 반환하고 `slk_errno()`를 통해 `errno`를 설정합니다.

**Common Error Codes:**
- `EINVAL` - 유효하지 않은 인자
- `ENOMEM` - 메모리 부족
- `ENOENT` - Topic 또는 노드를 찾을 수 없음
- `EEXIST` - Topic 또는 노드가 이미 존재함
- `EAGAIN` - 리소스를 일시적으로 사용할 수 없음 (논블로킹)
- `EHOSTUNREACH` - 원격 호스트에 도달할 수 없음
- `EMSGSIZE` - 버퍼에 비해 메시지가 너무 큼
- `EPROTO` - 프로토콜 오류 (유효하지 않은 메시지 형식)

### Error Handling Example

```c
int rc = slk_spot_publish(spot, "topic", data, len);
if (rc != 0) {
    int err = slk_errno();
    switch (err) {
    case SLK_ENOENT:
        fprintf(stderr, "Topic not found\n");
        break;
    case SLK_EAGAIN:
        fprintf(stderr, "Send buffer full, retry later\n");
        break;
    default:
        fprintf(stderr, "Error: %s\n", slk_strerror(err));
    }
}
```

---

## 스레드 안전성

**SPOT Instance:**
- 모든 작업에 대해 스레드 안전
- 읽기/쓰기 잠금을 위한 내부 `std::shared_mutex` 사용
- 여러 스레드가 `slk_spot_recv()`를 동시에 호출 가능
- Publish 작업은 Topic별로 직렬화

**모범 사례:**
- 최적의 성능을 위해 스레드당 하나의 SPOT 인스턴스 사용
- 필요한 경우 스레드 간 SPOT 인스턴스 공유 (스레드 안전)
- 다른 스레드가 사용 중일 때 SPOT 파괴 피하기

---

## 성능 고려사항

**LOCAL Topics:**
- 제로 카피 inproc 전송
- 나노초 지연 시간
- HWM에 의해서만 제한

**REMOTE Topics:**
- TCP 네트워크 오버헤드
- 작은 메시지에 대한 자동 배칭
- 재연결이 포함된 영구 연결

**최적화 팁:**
- 동일 프로세스 통신에 LOCAL topics 사용
- 가능한 경우 여러 publish 배치
- 높은 처리량 시나리오에서 HWM 증가
- 패턴 구독은 CPU 오버헤드가 있으므로 적게 사용

---

## 참고

- [빠른 시작 가이드](QUICK_START.md)
- [아키텍처 개요](ARCHITECTURE.md)
- [프로토콜 명세](PROTOCOL.md)
- [클러스터링 가이드](CLUSTERING.md)

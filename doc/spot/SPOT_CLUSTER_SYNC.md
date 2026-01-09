# SPOT Cluster Synchronization

## Overview

SPOT PUB/SUB의 클러스터 동기화 기능을 통해 여러 노드가 자동으로 토픽 목록을 공유하고 분산 pub/sub 시스템을 구축할 수 있습니다.

## Architecture

```
[Node A]                          [Node B]
  _server_socket (ROUTER)  ◄────  spot_node_t (DEALER)  <- B가 A에 연결
  spot_node_t (DEALER)     ────►  _server_socket (ROUTER) <- A가 B에 연결

- cluster_add("tcp://B:5555") -> A가 B에 DEALER 연결
- bind("tcp://A:5555") -> A가 서버로서 ROUTER listen
- cluster_sync() -> 양쪽이 서로 QUERY 보내고 토픽 목록 교환
```

### 양방향 통신

각 SPOT 노드는 두 가지 역할을 동시에 수행합니다:

1. **Server Mode (ROUTER socket)**: `bind()` 호출 시 ROUTER 소켓 생성, 다른 노드의 QUERY 요청 수신
2. **Client Mode (DEALER socket)**: `cluster_add()` 호출 시 DEALER 소켓으로 원격 노드에 연결

## API Reference

### bind()

```cpp
int bind(const std::string &endpoint);
```

**Purpose**: 서버 모드 활성화 (다른 노드의 연결 수락)

**Parameters**:
- `endpoint`: Bind endpoint (예: "tcp://*:5555")

**Returns**:
- `0` on success
- `-1` on error
  - `errno = EEXIST` if already bound
  - `errno = ENOMEM` if socket creation failed

**Example**:
```cpp
spot_pubsub_t spot(&ctx);
spot.bind("tcp://*:5555");  // 5555 포트로 서버 모드 시작
```

### cluster_add()

```cpp
int cluster_add(const std::string &endpoint);
```

**Purpose**: 클러스터에 원격 노드 추가

**Parameters**:
- `endpoint`: Remote node endpoint (예: "tcp://192.168.1.100:5555")

**Returns**:
- `0` on success
- `-1` on error
  - `errno = EEXIST` if node already added
  - `errno = EHOSTUNREACH` if connection fails

**Example**:
```cpp
spot.cluster_add("tcp://192.168.1.100:5555");  // Node B 추가
spot.cluster_add("tcp://192.168.1.101:5555");  // Node C 추가
```

### cluster_remove()

```cpp
int cluster_remove(const std::string &endpoint);
```

**Purpose**: 클러스터에서 노드 제거

**Parameters**:
- `endpoint`: Remote node endpoint

**Returns**:
- `0` on success
- `-1` on error
  - `errno = ENOENT` if node not found

**Example**:
```cpp
spot.cluster_remove("tcp://192.168.1.100:5555");
```

### cluster_sync()

```cpp
int cluster_sync(int timeout_ms);
```

**Purpose**: 클러스터 노드들과 토픽 목록 동기화

**How it works**:
1. 등록된 모든 노드에 QUERY 명령 전송
2. QUERY_RESP로 각 노드의 로컬 토픽 목록 수신
3. 수신한 토픽을 REMOTE로 registry에 등록

**Parameters**:
- `timeout_ms`: Timeout in milliseconds (현재 미구현, 향후 추가 예정)

**Returns**:
- `0` on success
- `-1` on error

**Example**:
```cpp
spot.cluster_sync(1000);  // 1초 타임아웃으로 동기화
```

## Message Protocol

### QUERY Request

```
Frame 0: 0x04 (QUERY)
```

노드가 다른 노드에게 토픽 목록을 요청할 때 사용합니다.

### QUERY_RESP Response

```
Frame 0: 0x05 (QUERY_RESP)
Frame 1: topic_count (uint32_t)
Frame 2+: topic_id (string) × topic_count
```

QUERY 요청에 대한 응답으로 로컬 토픽 목록을 전송합니다.

## Usage Example

### 2-Node Cluster Setup

```cpp
// Node A (192.168.1.100:5555)
ctx_t ctx_a;
spot_pubsub_t spot_a(&ctx_a);

// 1. Server mode 활성화
spot_a.bind("tcp://*:5555");

// 2. Local topics 생성
spot_a.topic_create("game:player1");
spot_a.topic_create("game:score");

// 3. Node B를 클러스터에 추가
spot_a.cluster_add("tcp://192.168.1.101:5555");

// 4. 토픽 동기화
spot_a.cluster_sync(1000);

// 5. 이제 Node B의 토픽도 사용 가능
auto topics = spot_a.list_topics();
// → ["game:player1" (LOCAL), "game:score" (LOCAL),
//    "chat:room1" (REMOTE), "chat:lobby" (REMOTE)]

// 6. Remote topic 구독
spot_a.subscribe("chat:room1");
```

```cpp
// Node B (192.168.1.101:5555)
ctx_t ctx_b;
spot_pubsub_t spot_b(&ctx_b);

// 1. Server mode 활성화
spot_b.bind("tcp://*:5555");

// 2. Local topics 생성
spot_b.topic_create("chat:room1");
spot_b.topic_create("chat:lobby");

// 3. Node A를 클러스터에 추가
spot_b.cluster_add("tcp://192.168.1.100:5555");

// 4. 토픽 동기화
spot_b.cluster_sync(1000);

// 5. 이제 Node A의 토픽도 사용 가능
auto topics = spot_b.list_topics();
// → ["chat:room1" (LOCAL), "chat:lobby" (LOCAL),
//    "game:player1" (REMOTE), "game:score" (REMOTE)]

// 6. Remote topic 구독
spot_b.subscribe("game:player1");
```

### Multi-Node Cluster (3+ nodes)

```cpp
// Node A
spot_a.bind("tcp://*:5555");
spot_a.cluster_add("tcp://node-b:5555");
spot_a.cluster_add("tcp://node-c:5555");
spot_a.cluster_sync(1000);

// Node B
spot_b.bind("tcp://*:5555");
spot_b.cluster_add("tcp://node-a:5555");
spot_b.cluster_add("tcp://node-c:5555");
spot_b.cluster_sync(1000);

// Node C
spot_c.bind("tcp://*:5555");
spot_c.cluster_add("tcp://node-a:5555");
spot_c.cluster_add("tcp://node-b:5555");
spot_c.cluster_sync(1000);
```

## Internal Implementation

### Server Mode (ROUTER socket)

`bind()` 호출 시:
1. ROUTER 소켓 생성 (`_server_socket`)
2. 지정된 endpoint에 bind
3. `recv()` 호출 시 자동으로 `process_incoming_messages()` 호출
4. QUERY 요청 수신 시 `handle_query_request()` 호출

### Client Mode (DEALER socket)

`cluster_add()` 호출 시:
1. `spot_node_t` 객체 생성 (DEALER 소켓 사용)
2. 원격 endpoint에 connect
3. `_nodes` 맵에 저장

### Synchronization Flow

```
Node A                                Node B
  |                                      |
  |--- QUERY -------------------------->|  (send_query)
  |                                      |
  |<-- QUERY_RESP (topic list) ---------|  (handle_query_request)
  |                                      |
  |--- register remote topics ----------|  (register_remote)
  |                                      |
```

### Auto-processing QUERY Requests

`recv()` 메서드 내부에서 자동으로 `process_incoming_messages()` 호출:
- `_server_socket`에서 QUERY 요청 확인 (non-blocking)
- QUERY 발견 시 `handle_query_request()` 호출하여 자동 응답
- 사용자는 별도 처리 없이 자동으로 클러스터 동기화 지원

## Thread Safety

모든 cluster 관련 메서드는 thread-safe합니다:
- `bind()`, `cluster_add()`, `cluster_remove()`, `cluster_sync()`: `unique_lock` 사용
- `process_incoming_messages()`, `handle_query_request()`: 내부에서 적절한 locking

## Performance Considerations

### Sync Overhead

- `cluster_sync()`는 모든 노드에 동기적으로 QUERY 전송
- 노드 수가 많을수록 동기화 시간 증가
- 권장: 주기적인 sync보다는 필요 시에만 호출

### Auto-processing Overhead

- `recv()` 호출 시마다 `process_incoming_messages()` 실행
- Non-blocking 방식이므로 메시지가 없으면 즉시 반환
- 오버헤드는 미미함 (단일 non-blocking recv 체크)

## Future Enhancements

1. **Timeout Support**: `cluster_sync(timeout_ms)` 파라미터 실제 구현
2. **Polling Integration**: 여러 소켓을 효율적으로 폴링
3. **Heartbeat**: 노드 간 health check 및 자동 재연결
4. **Topic Change Notification**: 토픽 생성/삭제 시 자동 브로드캐스트
5. **Partition Support**: 토픽 파티셔닝을 통한 확장성 개선

## Related Files

- `src/spot/spot_pubsub.hpp` - Main API
- `src/spot/spot_pubsub.cpp` - Implementation
- `src/spot/spot_node.hpp` - Node connection API
- `src/spot/spot_node.cpp` - Node implementation
- `examples/spot_cluster_sync_example.cpp` - Usage example

## See Also

- `SPOT_ARCHITECTURE.md` - Overall SPOT architecture
- `SPOT_MESSAGE_PROTOCOL.md` - Message format specification

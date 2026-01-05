[![English](https://img.shields.io/badge/lang:en-red.svg)](README.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](README.ko.md)

# ServerLink

[![CI](https://github.com/ulala-x/serverlink/actions/workflows/ci.yml/badge.svg)](https://github.com/ulala-x/serverlink/actions/workflows/ci.yml)
[![Release](https://github.com/ulala-x/serverlink/actions/workflows/release.yml/badge.svg)](https://github.com/ulala-x/serverlink/actions/workflows/release.yml)
[![License: MPL 2.0](https://img.shields.io/badge/License-MPL%202.0-brightgreen.svg)](https://opensource.org/licenses/MPL-2.0)

고성능 메시징 라이브러리 - ZeroMQ 호환 API와 위치 투명 Pub/Sub 시스템 제공

## 주요 특징

- **ZeroMQ 호환 Socket Pattern**: ROUTER, PUB/SUB, XPUB/XSUB, PAIR
- **SPOT PUB/SUB**: 위치 투명 Topic 기반 메시징 시스템
- **고성능**: epoll/kqueue/select 기반 최적화된 I/O, Zero-copy 메시징
- **Cross-platform**: Linux, macOS, Windows, BSD 지원
- **C/C++ API**: 간결한 C API와 C++ 호환성

## 빠른 시작

### Build

```bash
git clone https://github.com/ulalax/serverlink.git
cd serverlink

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 테스트 실행
ctest --test-dir build -C Release --output-on-failure
```

### 기본 사용 예제

```c
#include <serverlink/serverlink.h>

int main() {
    // Context 생성
    slk_ctx_t *ctx = slk_ctx_new();

    // Publisher
    slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
    slk_bind(pub, "tcp://*:5555");
    slk_send(pub, "hello world", 11, 0);

    // Subscriber
    slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
    slk_connect(sub, "tcp://localhost:5555");
    slk_setsockopt(sub, SLK_SUBSCRIBE, "", 0);  // 모든 메시지 구독

    char buf[256];
    slk_recv(sub, buf, sizeof(buf), 0);

    // Cleanup
    slk_close(pub);
    slk_close(sub);
    slk_ctx_destroy(ctx);
    return 0;
}
```

---

## Socket Types

| Type | 설명 |
|------|------|
| `SLK_ROUTER` | Client 식별 기반 Server 측 Routing Socket |
| `SLK_PUB` | Publisher Socket (fan-out) |
| `SLK_SUB` | Topic Filtering Subscriber Socket |
| `SLK_XPUB` | 구독 가시성이 있는 Extended Publisher |
| `SLK_XSUB` | 수동 구독 제어가 있는 Extended Subscriber |
| `SLK_PAIR` | 1:1 양방향 전용 Socket |

## Transport Protocol

| Protocol | 설명 | Latency |
|----------|------|---------|
| `tcp://` | TCP/IP Networking | 10-100 µs |
| `inproc://` | Process 내 (Thread 간) | < 1 µs |

---

## SPOT PUB/SUB

**SPOT** (Scalable Partitioned Ordered Topics) - 위치 투명 Topic 기반 메시징 시스템

### 특징

- **Location Transparency**: Topic 위치와 관계없이 Subscribe/Publish
- **LOCAL Topic**: inproc를 통한 Zero-copy (나노초 Latency)
- **REMOTE Topic**: TCP를 통한 자동 Routing
- **Pattern Subscription**: Prefix Matching (`events:*` → `events:`)
- **Cluster Sync**: Node 간 자동 Topic 발견

### 기본 사용

```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// LOCAL Topic 생성
slk_spot_topic_create(spot, "game:player");

// Subscribe
slk_spot_subscribe(spot, "game:player");

// Publish
slk_spot_publish(spot, "game:player", "hello", 5);

// Receive
char topic[256], data[4096];
size_t topic_len, data_len;
slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, 0);

slk_spot_destroy(&spot);
slk_ctx_destroy(ctx);
```

### Cluster 설정

```c
// Server Node
slk_spot_topic_create(spot, "sensor:temp");
slk_spot_bind(spot, "tcp://*:5555");

// Client Node
slk_spot_cluster_add(spot, "tcp://server:5555");
slk_spot_subscribe(spot, "sensor:temp");
```

### Pattern Subscription

```c
// "events:" Prefix로 시작하는 모든 Topic 구독
slk_spot_subscribe_pattern(spot, "events:*");

// Matching: events:login, events:logout, events:user:created
```

**상세 문서**: [docs/spot/](docs/spot/)

---

## ROUTER Socket

Client 식별 기반 Server 측 Routing:

```c
slk_socket_t *router = slk_socket(ctx, SLK_ROUTER);
slk_bind(router, "tcp://*:5555");

// 연결 알림 활성화
int notify = 1;
slk_setsockopt(router, SLK_ROUTER_NOTIFY, &notify, sizeof(notify));

// Receive: [routing_id][empty][payload]
char id[256], empty[1], msg[1024];
slk_recv(router, id, sizeof(id), 0);
slk_recv(router, empty, sizeof(empty), 0);
slk_recv(router, msg, sizeof(msg), 0);

// 특정 Client에 응답
slk_send(router, id, id_len, SLK_SNDMORE);
slk_send(router, "", 0, SLK_SNDMORE);
slk_send(router, "response", 8, 0);
```

**Options:**
- `SLK_ROUTER_MANDATORY` - 연결되지 않은 Peer에 전송 시 실패
- `SLK_ROUTER_HANDOVER` - 동일 ID의 새 Peer로 전환
- `SLK_ROUTER_NOTIFY` - 연결/해제 Event 활성화
- `SLK_CONNECT_ROUTING_ID` - 연결 시 Routing ID 설정

---

## PUB/SUB

### 기본 Pub/Sub

```c
// Publisher
slk_socket_t *pub = slk_socket(ctx, SLK_PUB);
slk_bind(pub, "tcp://*:5556");
slk_send(pub, "news.sports Hello!", 18, 0);

// Subscriber
slk_socket_t *sub = slk_socket(ctx, SLK_SUB);
slk_connect(sub, "tcp://localhost:5556");
slk_setsockopt(sub, SLK_SUBSCRIBE, "news.", 5);  // Topic Filter

char buf[256];
slk_recv(sub, buf, sizeof(buf), 0);
```

### Pattern Subscription

```c
slk_setsockopt(sub, SLK_PSUBSCRIBE, "news.*", 6);       // news.sports, news.tech
slk_setsockopt(sub, SLK_PSUBSCRIBE, "user.?", 6);       // user.1, user.a
slk_setsockopt(sub, SLK_PSUBSCRIBE, "alert.[0-9]", 11); // alert.0 ~ alert.9
```

| Pattern | 설명 | 예시 |
|---------|------|------|
| `*` | 임의 문자열 | `news.*` → `news.sports` |
| `?` | 단일 문자 | `user.?` → `user.1` |
| `[abc]` | 문자 집합 | `[abc]def` → `adef` |
| `[a-z]` | 문자 범위 | `id.[0-9]` → `id.5` |

### XPUB/XSUB (Extended Pub/Sub)

```c
// XPUB - 구독 메시지 확인
slk_socket_t *xpub = slk_socket(ctx, SLK_XPUB);
int verbose = 1;
slk_setsockopt(xpub, SLK_XPUB_VERBOSE, &verbose, sizeof(verbose));
slk_bind(xpub, "tcp://*:5557");

// 구독 알림 Receive
char sub_msg[256];
slk_recv(xpub, sub_msg, sizeof(sub_msg), 0);
// sub_msg[0] = 1 (subscribe) or 0 (unsubscribe)
// sub_msg[1:] = topic

// XSUB - 수동 구독 관리
slk_socket_t *xsub = slk_socket(ctx, SLK_XSUB);
slk_connect(xsub, "tcp://localhost:5557");
char subscribe[] = "\x01news.";  // 0x01 + topic
slk_send(xsub, subscribe, sizeof(subscribe) - 1, 0);
```

---

## Poller API

Event 기반 I/O:

```c
void *poller = slk_poller_new();

slk_poller_add(poller, socket1, user_data1, SLK_POLLIN);
slk_poller_add(poller, socket2, user_data2, SLK_POLLIN | SLK_POLLOUT);

slk_poller_event_t event;
while (slk_poller_wait(poller, &event, 1000) == 0) {
    if (event.events & SLK_POLLIN) {
        // 읽기 준비 완료
    }
}

slk_poller_destroy(&poller);
```

---

## Socket Options Reference

### Buffer Options
| Option | Type | 설명 |
|--------|------|------|
| `SLK_SNDHWM` | int | Send High Water Mark (메시지 수) |
| `SLK_RCVHWM` | int | Receive High Water Mark (메시지 수) |
| `SLK_SNDBUF` | int | Send Buffer Size (bytes) |
| `SLK_RCVBUF` | int | Receive Buffer Size (bytes) |

### Connection Options
| Option | Type | 설명 |
|--------|------|------|
| `SLK_LINGER` | int | 종료 시 대기 시간 (ms) |
| `SLK_RECONNECT_IVL` | int | 재연결 간격 (ms) |
| `SLK_RECONNECT_IVL_MAX` | int | 최대 재연결 간격 (ms) |
| `SLK_BACKLOG` | int | listen Backlog 크기 |

### TCP Options
| Option | Type | 설명 |
|--------|------|------|
| `SLK_TCP_KEEPALIVE` | int | TCP Keepalive 활성화 |
| `SLK_TCP_KEEPALIVE_IDLE` | int | Keepalive 유휴 시간 (초) |
| `SLK_TCP_KEEPALIVE_INTVL` | int | Keepalive 간격 (초) |
| `SLK_TCP_KEEPALIVE_CNT` | int | Keepalive Probe 횟수 |

---

## Platform Support

| Platform | Architecture | I/O Backend | Status |
|----------|--------------|-------------|--------|
| Linux | x64, ARM64 | epoll | ✅ |
| macOS | x64, ARM64 | kqueue | ✅ |
| Windows | x64, ARM64 | select | ✅ |
| BSD | x64 | kqueue | ✅ |

---

## Test Results

```
78개 테스트 전체 통과

Core (47개):
├── ROUTER: 8
├── PUB/SUB: 12
├── Transport: 4
├── Unit: 11
├── Utilities: 4
├── Integration: 1
├── Poller/Proxy/Monitor: 4
└── Windows: 1

SPOT (31개):
├── Basic: 11
├── Local: 6
├── Remote: 5
├── Cluster: 4
└── Mixed: 5
```

---

## Build Options

| Option | Default | 설명 |
|--------|---------|------|
| `BUILD_SHARED_LIBS` | ON | Shared Library Build |
| `BUILD_TESTS` | ON | Test Suite Build |
| `BUILD_EXAMPLES` | ON | Example Program Build |
| `CMAKE_BUILD_TYPE` | Debug | Build Type (Debug/Release) |

## Requirements

- CMake 3.14+
- C++20 Compiler (GCC 10+, Clang 10+, MSVC 2019+)
- POSIX Threads (Linux/macOS) 또는 Win32 Threads (Windows)

---

## Documentation

- **SPOT PUB/SUB**: [docs/spot/](docs/spot/)
  - [API Reference](docs/spot/API.ko.md)
  - [Architecture](docs/spot/ARCHITECTURE.ko.md)
  - [Quick Start](docs/spot/QUICK_START.ko.md)
  - [Clustering Guide](docs/spot/CLUSTERING.ko.md)

---

## License

Mozilla Public License 2.0 (MPL-2.0). [LICENSE](LICENSE) 참조.

## Acknowledgments

- [ZeroMQ](https://zeromq.org/) - Socket Pattern 영감
- [Redis](https://redis.io/) - Pub/Sub 기능 영감

# ServerLink

[![CI](https://github.com/ulalax/serverlink/actions/workflows/ci.yml/badge.svg)](https://github.com/ulalax/serverlink/actions/workflows/ci.yml)
[![Release](https://github.com/ulalax/serverlink/actions/workflows/release.yml/badge.svg)](https://github.com/ulalax/serverlink/actions/workflows/release.yml)
[![License: MPL 2.0](https://img.shields.io/badge/License-MPL%202.0-brightgreen.svg)](https://opensource.org/licenses/MPL-2.0)

고성능 메시징 라이브러리 - ZeroMQ 호환 API와 위치 투명 Pub/Sub 시스템 제공

## 주요 특징

- **ZeroMQ 호환 소켓 패턴**: ROUTER, PUB/SUB, XPUB/XSUB, PAIR
- **SPOT PUB/SUB**: 위치 투명 토픽 기반 메시징 시스템
- **고성능**: epoll/kqueue/select 기반 최적화된 I/O, 제로카피 메시징
- **크로스 플랫폼**: Linux, macOS, Windows, BSD 지원
- **C/C++ API**: 간결한 C API와 C++ 호환성

## 빠른 시작

### 빌드

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

    // 정리
    slk_close(pub);
    slk_close(sub);
    slk_ctx_destroy(ctx);
    return 0;
}
```

---

## 소켓 타입

| 타입 | 설명 |
|------|------|
| `SLK_ROUTER` | 클라이언트 식별 기반 서버 측 라우팅 소켓 |
| `SLK_PUB` | 발행자 소켓 (fan-out) |
| `SLK_SUB` | 토픽 필터링 구독자 소켓 |
| `SLK_XPUB` | 구독 가시성이 있는 확장 발행자 |
| `SLK_XSUB` | 수동 구독 제어가 있는 확장 구독자 |
| `SLK_PAIR` | 1:1 양방향 전용 소켓 |

## 전송 프로토콜

| 프로토콜 | 설명 | 지연시간 |
|----------|------|----------|
| `tcp://` | TCP/IP 네트워킹 | 10-100 µs |
| `inproc://` | 프로세스 내 (스레드 간) | < 1 µs |

---

## SPOT PUB/SUB

**SPOT** (Scalable Partitioned Ordered Topics) - 위치 투명 토픽 기반 메시징 시스템

### 특징

- **위치 투명성**: 토픽 위치와 관계없이 구독/발행
- **LOCAL 토픽**: inproc를 통한 제로카피 (나노초 지연)
- **REMOTE 토픽**: TCP를 통한 자동 라우팅
- **패턴 구독**: prefix 매칭 (`events:*` → `events:`)
- **클러스터 동기화**: 노드 간 자동 토픽 발견

### 기본 사용

```c
slk_ctx_t *ctx = slk_ctx_new();
slk_spot_t *spot = slk_spot_new(ctx);

// LOCAL 토픽 생성
slk_spot_topic_create(spot, "game:player");

// 구독
slk_spot_subscribe(spot, "game:player");

// 발행
slk_spot_publish(spot, "game:player", "hello", 5);

// 수신
char topic[256], data[4096];
size_t topic_len, data_len;
slk_spot_recv(spot, topic, sizeof(topic), &topic_len,
              data, sizeof(data), &data_len, 0);

slk_spot_destroy(&spot);
slk_ctx_destroy(ctx);
```

### 클러스터 설정

```c
// Server Node
slk_spot_topic_create(spot, "sensor:temp");
slk_spot_bind(spot, "tcp://*:5555");

// Client Node
slk_spot_cluster_add(spot, "tcp://server:5555");
slk_spot_subscribe(spot, "sensor:temp");
```

### 패턴 구독

```c
// "events:" prefix로 시작하는 모든 토픽 구독
slk_spot_subscribe_pattern(spot, "events:*");

// 매칭: events:login, events:logout, events:user:created
```

**상세 문서**: [docs/spot/](docs/spot/)

---

## ROUTER 소켓

클라이언트 식별 기반 서버 측 라우팅:

```c
slk_socket_t *router = slk_socket(ctx, SLK_ROUTER);
slk_bind(router, "tcp://*:5555");

// 연결 알림 활성화
int notify = 1;
slk_setsockopt(router, SLK_ROUTER_NOTIFY, &notify, sizeof(notify));

// 수신: [routing_id][empty][payload]
char id[256], empty[1], msg[1024];
slk_recv(router, id, sizeof(id), 0);
slk_recv(router, empty, sizeof(empty), 0);
slk_recv(router, msg, sizeof(msg), 0);

// 특정 클라이언트에 응답
slk_send(router, id, id_len, SLK_SNDMORE);
slk_send(router, "", 0, SLK_SNDMORE);
slk_send(router, "response", 8, 0);
```

**옵션:**
- `SLK_ROUTER_MANDATORY` - 연결되지 않은 피어에 전송 시 실패
- `SLK_ROUTER_HANDOVER` - 동일 ID의 새 피어로 전환
- `SLK_ROUTER_NOTIFY` - 연결/해제 이벤트 활성화
- `SLK_CONNECT_ROUTING_ID` - 연결 시 라우팅 ID 설정

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
slk_setsockopt(sub, SLK_SUBSCRIBE, "news.", 5);  // 토픽 필터

char buf[256];
slk_recv(sub, buf, sizeof(buf), 0);
```

### 패턴 구독

```c
slk_setsockopt(sub, SLK_PSUBSCRIBE, "news.*", 6);       // news.sports, news.tech
slk_setsockopt(sub, SLK_PSUBSCRIBE, "user.?", 6);       // user.1, user.a
slk_setsockopt(sub, SLK_PSUBSCRIBE, "alert.[0-9]", 11); // alert.0 ~ alert.9
```

| 패턴 | 설명 | 예시 |
|------|------|------|
| `*` | 임의 문자열 | `news.*` → `news.sports` |
| `?` | 단일 문자 | `user.?` → `user.1` |
| `[abc]` | 문자 집합 | `[abc]def` → `adef` |
| `[a-z]` | 문자 범위 | `id.[0-9]` → `id.5` |

### XPUB/XSUB (확장 Pub/Sub)

```c
// XPUB - 구독 메시지 확인
slk_socket_t *xpub = slk_socket(ctx, SLK_XPUB);
int verbose = 1;
slk_setsockopt(xpub, SLK_XPUB_VERBOSE, &verbose, sizeof(verbose));
slk_bind(xpub, "tcp://*:5557");

// 구독 알림 수신
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

이벤트 기반 I/O:

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

## 소켓 옵션 레퍼런스

### 버퍼 옵션
| 옵션 | 타입 | 설명 |
|------|------|------|
| `SLK_SNDHWM` | int | 송신 하이 워터 마크 (메시지 수) |
| `SLK_RCVHWM` | int | 수신 하이 워터 마크 (메시지 수) |
| `SLK_SNDBUF` | int | 송신 버퍼 크기 (바이트) |
| `SLK_RCVBUF` | int | 수신 버퍼 크기 (바이트) |

### 연결 옵션
| 옵션 | 타입 | 설명 |
|------|------|------|
| `SLK_LINGER` | int | 종료 시 대기 시간 (ms) |
| `SLK_RECONNECT_IVL` | int | 재연결 간격 (ms) |
| `SLK_RECONNECT_IVL_MAX` | int | 최대 재연결 간격 (ms) |
| `SLK_BACKLOG` | int | listen 백로그 크기 |

### TCP 옵션
| 옵션 | 타입 | 설명 |
|------|------|------|
| `SLK_TCP_KEEPALIVE` | int | TCP keepalive 활성화 |
| `SLK_TCP_KEEPALIVE_IDLE` | int | Keepalive 유휴 시간 (초) |
| `SLK_TCP_KEEPALIVE_INTVL` | int | Keepalive 간격 (초) |
| `SLK_TCP_KEEPALIVE_CNT` | int | Keepalive 프로브 횟수 |

---

## 플랫폼 지원

| 플랫폼 | 아키텍처 | I/O 백엔드 | 상태 |
|--------|----------|------------|------|
| Linux | x64, ARM64 | epoll | ✅ |
| macOS | x64, ARM64 | kqueue | ✅ |
| Windows | x64, ARM64 | select | ✅ |
| BSD | x64 | kqueue | ✅ |

---

## 테스트 결과

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

## 빌드 옵션

| 옵션 | 기본값 | 설명 |
|------|--------|------|
| `BUILD_SHARED_LIBS` | ON | 공유 라이브러리 빌드 |
| `BUILD_TESTS` | ON | 테스트 스위트 빌드 |
| `BUILD_EXAMPLES` | ON | 예제 프로그램 빌드 |
| `CMAKE_BUILD_TYPE` | Debug | 빌드 타입 (Debug/Release) |

## 요구사항

- CMake 3.14+
- C++20 컴파일러 (GCC 10+, Clang 10+, MSVC 2019+)
- POSIX threads (Linux/macOS) 또는 Win32 threads (Windows)

---

## 문서

- **SPOT PUB/SUB**: [docs/spot/](docs/spot/)
  - [API Reference](docs/spot/API.md)
  - [Architecture](docs/spot/ARCHITECTURE.md)
  - [Quick Start](docs/spot/QUICK_START.md)
  - [Clustering Guide](docs/spot/CLUSTERING.md)

---

## 라이선스

Mozilla Public License 2.0 (MPL-2.0). [LICENSE](LICENSE) 참조.

## 감사의 말

- [ZeroMQ](https://zeromq.org/) - 소켓 패턴 영감
- [Redis](https://redis.io/) - Pub/Sub 기능 영감

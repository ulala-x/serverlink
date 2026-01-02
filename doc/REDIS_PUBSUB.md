# Redis Pub/Sub 완벽 가이드

> 최종 업데이트: 2026-01-02
> Redis 버전: 7.x 기준

---

## 목차

1. [개요](#1-개요)
2. [기본 Pub/Sub](#2-기본-pubsub)
3. [패턴 매칭 구독](#3-패턴-매칭-구독)
4. [Sharded Pub/Sub (Redis 7.0+)](#4-sharded-pubsub-redis-70)
5. [인트로스펙션 명령어](#5-인트로스펙션-명령어)
6. [Wire Protocol](#6-wire-protocol)
7. [메시지 전달 보장](#7-메시지-전달-보장)
8. [클러스터 환경](#8-클러스터-환경)
9. [모범 사례](#9-모범-사례)
10. [제한사항](#10-제한사항)
11. [Redis Streams와 비교](#11-redis-streams와-비교)

---

## 1. 개요

### 1.1 Pub/Sub란?

Redis Pub/Sub(Publish/Subscribe)는 **발행-구독 메시징 패러다임**을 구현한 기능입니다.

- **발행자(Publisher)**: 특정 수신자를 지정하지 않고 채널에 메시지를 발행
- **구독자(Subscriber)**: 관심 있는 채널을 구독하여 해당 메시지만 수신
- **채널(Channel)**: 메시지가 전달되는 논리적 통로

### 1.2 핵심 특징

| 특징 | 설명 |
|------|------|
| 느슨한 결합 | 발행자와 구독자가 서로를 알 필요 없음 |
| 확장성 | 발행자/구독자 수에 관계없이 동작 |
| 실시간 전달 | 메시지가 즉시 구독자에게 전달 |
| 다중 채널 | 하나의 클라이언트가 여러 채널 구독 가능 |

### 1.3 사용 사례

- 실시간 알림 시스템
- 채팅 애플리케이션
- 이벤트 브로드캐스팅
- 마이크로서비스 간 통신
- 실시간 데이터 스트리밍

---

## 2. 기본 Pub/Sub

### 2.1 명령어 요약

| 명령어 | 문법 | 설명 |
|--------|------|------|
| `SUBSCRIBE` | `SUBSCRIBE channel [channel ...]` | 하나 이상의 채널 구독 |
| `UNSUBSCRIBE` | `UNSUBSCRIBE [channel ...]` | 채널 구독 해제 |
| `PUBLISH` | `PUBLISH channel message` | 채널에 메시지 발행 |

### 2.2 기본 사용 예시

#### 구독자 (터미널 1)
```redis
127.0.0.1:6379> SUBSCRIBE mychannel
Reading messages... (press Ctrl-C to quit)
1) "subscribe"
2) "mychannel"
3) (integer) 1

# 메시지 수신 시
1) "message"
2) "mychannel"
3) "Hello, World!"
```

#### 발행자 (터미널 2)
```redis
127.0.0.1:6379> PUBLISH mychannel "Hello, World!"
(integer) 1
```

> **반환값**: `PUBLISH`는 메시지를 수신한 구독자 수를 반환합니다.

### 2.3 다중 채널 구독

```redis
127.0.0.1:6379> SUBSCRIBE news sports weather
Reading messages... (press Ctrl-C to quit)
1) "subscribe"
2) "news"
3) (integer) 1
1) "subscribe"
2) "sports"
3) (integer) 2
1) "subscribe"
2) "weather"
3) (integer) 3
```

### 2.4 구독 해제

```redis
# 특정 채널 구독 해제
UNSUBSCRIBE news

# 모든 채널 구독 해제
UNSUBSCRIBE
```

---

## 3. 패턴 매칭 구독

### 3.1 명령어 요약

| 명령어 | 문법 | 설명 |
|--------|------|------|
| `PSUBSCRIBE` | `PSUBSCRIBE pattern [pattern ...]` | glob 스타일 패턴으로 채널 구독 |
| `PUNSUBSCRIBE` | `PUNSUBSCRIBE [pattern ...]` | 패턴 구독 해제 |

### 3.2 패턴 문법

| 패턴 | 설명 | 예시 |
|------|------|------|
| `*` | 임의의 문자열 매칭 | `news.*` → `news.sports`, `news.weather` |
| `?` | 단일 문자 매칭 | `news.?` → `news.a`, `news.1` |
| `[abc]` | 문자 클래스 | `news.[sw]*` → `news.sports`, `news.weather` |

### 3.3 사용 예시

```redis
# news.로 시작하는 모든 채널 구독
127.0.0.1:6379> PSUBSCRIBE news.*
Reading messages... (press Ctrl-C to quit)
1) "psubscribe"
2) "news.*"
3) (integer) 1

# 매칭되는 메시지 수신 시
1) "pmessage"
2) "news.*"           # 매칭된 패턴
3) "news.sports"      # 실제 채널명
4) "Goal scored!"     # 메시지
```

### 3.4 패턴 매칭 예시

| 패턴 | 채널 | 매칭 여부 |
|------|------|-----------|
| `news.*` | `news.sports` | ✓ |
| `news.*` | `news.weather.today` | ✓ |
| `news.*` | `breaking.news` | ✗ |
| `*news*` | `breaking.news.alert` | ✓ |
| `news.?` | `news.a` | ✓ |
| `news.?` | `news.ab` | ✗ |

### 3.5 중복 수신 주의

동일 채널을 일반 구독과 패턴 구독으로 동시에 구독하면 **메시지가 두 번 수신**됩니다.

```redis
SUBSCRIBE foo
PSUBSCRIBE f*

# "foo" 채널에 메시지 발행 시:
# 1. message 타입으로 한 번
# 2. pmessage 타입으로 한 번
```

---

## 4. Sharded Pub/Sub (Redis 7.0+)

### 4.1 개요

Redis 7.0부터 도입된 **Sharded Pub/Sub**는 클러스터 환경에서 Pub/Sub의 확장성을 크게 향상시킵니다.

### 4.2 기존 Pub/Sub vs Sharded Pub/Sub

| 항목 | 기존 Pub/Sub | Sharded Pub/Sub |
|------|--------------|-----------------|
| 메시지 전파 범위 | 클러스터 전체 | 샤드 내부만 |
| 클러스터 버스 트래픽 | 높음 | 낮음 |
| 확장성 | 제한적 | 수평 확장 가능 |
| 도입 버전 | 초기 버전 | Redis 7.0+ |

### 4.3 명령어

| 명령어 | 문법 | 설명 |
|--------|------|------|
| `SSUBSCRIBE` | `SSUBSCRIBE shardchannel [...]` | 샤드 채널 구독 |
| `SUNSUBSCRIBE` | `SUNSUBSCRIBE [shardchannel ...]` | 샤드 채널 구독 해제 |
| `SPUBLISH` | `SPUBLISH shardchannel message` | 샤드 채널에 메시지 발행 |

### 4.4 작동 원리

```
┌─────────────────────────────────────────────────────────┐
│                    Redis Cluster                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │
│  │   Shard 1   │  │   Shard 2   │  │   Shard 3   │      │
│  │ Slots 0-5460│  │Slots 5461-  │  │Slots 10923- │      │
│  │             │  │   10922     │  │   16383     │      │
│  │  Channel A  │  │  Channel B  │  │  Channel C  │      │
│  │  (슬롯 100) │  │ (슬롯 8000) │  │(슬롯 15000) │      │
│  └─────────────┘  └─────────────┘  └─────────────┘      │
│                                                          │
│  채널 → 슬롯 해시 → 해당 샤드에서만 메시지 전파           │
└─────────────────────────────────────────────────────────┘
```

### 4.5 사용 예시

#### 구독자
```redis
127.0.0.1:6379> SSUBSCRIBE orders
Reading messages... (press Ctrl-C to quit)
1) "ssubscribe"
2) "orders"
3) (integer) 1

# 메시지 수신 시
1) "smessage"
2) "orders"
3) "New order: #12345"
```

#### 발행자
```redis
127.0.0.1:6379> SPUBLISH orders "New order: #12345"
(integer) 1
```

### 4.6 장점

| 장점 | 설명 |
|------|------|
| **수평 확장성** | 샤드 추가로 Pub/Sub 처리량 증가 |
| **트래픽 감소** | 클러스터 버스 데이터 전송량 대폭 감소 |
| **부하 분산** | 연결이 특정 노드에 집중되지 않음 |
| **지역성** | 관련 데이터와 채널이 같은 샤드에 위치 가능 |

### 4.7 요구사항

- Redis 7.0 이상
- RESP3 프로토콜 권장
- 클러스터 모드 활성화

---

## 5. 인트로스펙션 명령어

### 5.1 PUBSUB 명령어 그룹

Pub/Sub 서브시스템의 상태를 조회하는 명령어들입니다.

| 명령어 | 설명 | 도입 버전 |
|--------|------|-----------|
| `PUBSUB CHANNELS [pattern]` | 활성 채널 목록 조회 | 2.8.0 |
| `PUBSUB NUMSUB [channel ...]` | 채널별 구독자 수 조회 | 2.8.0 |
| `PUBSUB NUMPAT` | 활성 패턴 구독 수 조회 | 2.8.0 |
| `PUBSUB SHARDCHANNELS [pattern]` | 활성 샤드 채널 목록 | 7.0.0 |
| `PUBSUB SHARDNUMSUB [channel ...]` | 샤드 채널 구독자 수 | 7.0.0 |

### 5.2 사용 예시

#### 활성 채널 조회
```redis
127.0.0.1:6379> PUBSUB CHANNELS
1) "news"
2) "sports"
3) "weather"

# 패턴으로 필터링
127.0.0.1:6379> PUBSUB CHANNELS news*
1) "news"
2) "newsletter"
```

#### 채널별 구독자 수
```redis
127.0.0.1:6379> PUBSUB NUMSUB news sports
1) "news"
2) (integer) 3
3) "sports"
4) (integer) 5
```

#### 패턴 구독 수
```redis
127.0.0.1:6379> PUBSUB NUMPAT
(integer) 2
```

#### 샤드 채널 정보 (7.0+)
```redis
127.0.0.1:6379> PUBSUB SHARDCHANNELS
1) "orders"
2) "payments"

127.0.0.1:6379> PUBSUB SHARDNUMSUB orders payments
1) "orders"
2) (integer) 2
3) "payments"
4) (integer) 1
```

---

## 6. Wire Protocol

### 6.1 메시지 형식

모든 Pub/Sub 메시지는 **배열 형식**으로 전달됩니다.

### 6.2 메시지 타입

| 타입 | 구성 요소 | 설명 |
|------|-----------|------|
| `subscribe` | [타입, 채널명, 구독수] | 구독 확인 |
| `unsubscribe` | [타입, 채널명, 남은구독수] | 구독 해제 확인 |
| `message` | [타입, 채널명, 메시지] | 일반 메시지 |
| `psubscribe` | [타입, 패턴, 구독수] | 패턴 구독 확인 |
| `punsubscribe` | [타입, 패턴, 남은구독수] | 패턴 구독 해제 확인 |
| `pmessage` | [타입, 패턴, 채널명, 메시지] | 패턴 매칭 메시지 |
| `ssubscribe` | [타입, 샤드채널, 구독수] | 샤드 구독 확인 |
| `sunsubscribe` | [타입, 샤드채널, 남은구독수] | 샤드 구독 해제 확인 |
| `smessage` | [타입, 샤드채널, 메시지] | 샤드 메시지 |

### 6.3 RESP 프로토콜 예시

#### 구독 요청 및 응답
```
클라이언트 → 서버:
SUBSCRIBE first second

서버 → 클라이언트:
*3
$9
subscribe
$5
first
:1
*3
$9
subscribe
$6
second
:2
```

#### 메시지 수신
```
서버 → 클라이언트:
*3
$7
message
$6
second
$5
Hello
```

### 6.4 구독 상태에서 허용되는 명령어

| 프로토콜 | 허용 명령어 |
|----------|-------------|
| RESP2 | `PING`, `QUIT`, `RESET`, `SUBSCRIBE`, `UNSUBSCRIBE`, `PSUBSCRIBE`, `PUNSUBSCRIBE`, `SSUBSCRIBE`, `SUNSUBSCRIBE` |
| RESP3 | **모든 명령어** (개선된 기능) |

---

## 7. 메시지 전달 보장

### 7.1 At-most-once 전달

Redis Pub/Sub는 **at-most-once(최대 한 번)** 전달 의미론을 따릅니다.

```
발행자 ──────► Redis ──────► 구독자
              │
              │ 메시지 전송 후 삭제
              │ (저장하지 않음)
              ▼
```

### 7.2 메시지 손실 가능 상황

| 상황 | 결과 |
|------|------|
| 네트워크 연결 끊김 | 메시지 손실 |
| 구독자 처리 오류 | 메시지 손실 |
| 구독자 미연결 | 메시지 손실 |
| 클라이언트 버퍼 오버플로우 | 연결 종료 |

### 7.3 신뢰성 요구 시 대안

더 강력한 전달 보장이 필요한 경우 **Redis Streams** 사용을 권장합니다.

```redis
# Streams 예시 (at-least-once 전달 지원)
XADD mystream * field value
XREAD BLOCK 0 STREAMS mystream $
```

---

## 8. 클러스터 환경

### 8.1 기본 동작

- 클라이언트는 **모든 노드**에서 구독/발행 가능
- 클러스터가 메시지 포워딩을 자동 처리
- `PUBSUB` 명령 결과는 **해당 노드의 컨텍스트만** 반영

### 8.2 기존 Pub/Sub 클러스터 동작

```
┌─────────────────────────────────────────────────────┐
│                   Redis Cluster                      │
│                                                      │
│  Node1 ◄──────────────────────────────► Node2       │
│    │     클러스터 버스로 메시지 전파        │        │
│    │                                      │         │
│    ▼                                      ▼         │
│  Node3 ◄──────────────────────────────► Node4       │
│                                                      │
│  ※ 모든 노드에 메시지가 브로드캐스트됨               │
└─────────────────────────────────────────────────────┘
```

### 8.3 Sharded Pub/Sub 클러스터 동작 (7.0+)

```
┌─────────────────────────────────────────────────────┐
│                   Redis Cluster                      │
│                                                      │
│  ┌──────────────┐          ┌──────────────┐         │
│  │   Shard 1    │          │   Shard 2    │         │
│  │              │          │              │         │
│  │ Master ◄──► │          │ Master ◄──► │          │
│  │ Replica     │          │ Replica     │          │
│  │              │          │              │         │
│  │ 채널 A 메시지 │          │ 채널 B 메시지 │         │
│  │ (샤드 내 전파)│          │ (샤드 내 전파)│         │
│  └──────────────┘          └──────────────┘         │
│                                                      │
│  ※ 메시지가 해당 샤드 내에서만 전파됨                 │
└─────────────────────────────────────────────────────┘
```

---

## 9. 모범 사례

### 9.1 채널 네이밍 컨벤션

```
# 환경 분리
production:notifications
staging:notifications
development:notifications

# 도메인 기반
user:events:created
order:events:completed
payment:events:failed

# 계층적 구조
app:module:action
myapp:user:login
myapp:order:create
```

### 9.2 성능 최적화

```python
# 권장: 단일 연결로 다중 채널 구독
pubsub = redis.pubsub()
pubsub.subscribe('channel1', 'channel2', 'channel3')

# 비권장: 채널마다 별도 연결
# conn1.subscribe('channel1')
# conn2.subscribe('channel2')
```

### 9.3 오류 처리

```python
import redis

def reliable_subscribe():
    while True:
        try:
            r = redis.Redis()
            pubsub = r.pubsub()
            pubsub.subscribe('mychannel')

            for message in pubsub.listen():
                if message['type'] == 'message':
                    process_message(message['data'])

        except redis.ConnectionError:
            print("연결 끊김, 재연결 시도...")
            time.sleep(1)
            continue
```

### 9.4 데이터베이스 스코핑

```
⚠️ 주의: Pub/Sub는 데이터베이스 번호와 무관합니다!

DB 10에서 PUBLISH → DB 1 구독자도 수신

해결책: 채널명에 환경 접두사 사용
```

---

## 10. 제한사항

### 10.1 주요 제한사항

| 항목 | 설명 |
|------|------|
| 메시지 영속성 | 없음 - 메시지가 저장되지 않음 |
| 전달 보장 | At-most-once만 지원 |
| 메시지 확인 | ACK 메커니즘 없음 |
| 메시지 재전송 | 불가능 |
| 오프라인 구독자 | 메시지 손실 |

### 10.2 클라이언트 버퍼 제한

```redis
# redis.conf 설정
client-output-buffer-limit pubsub 32mb 8mb 60

# 의미:
# - 하드 제한: 32MB 초과 시 즉시 연결 종료
# - 소프트 제한: 8MB 초과 상태가 60초 지속 시 연결 종료
```

### 10.3 RESP2 구독 상태 제한

```redis
# 구독 상태에서는 제한된 명령만 실행 가능
# RESP3 프로토콜에서는 이 제한이 해제됨

# redis-cli에서 UNSUBSCRIBE 불가 (Ctrl-C 필요)
```

---

## 11. Redis Streams와 비교

### 11.1 비교표

| 기능 | Pub/Sub | Streams |
|------|---------|---------|
| 메시지 영속성 | ✗ | ✓ |
| 전달 보장 | At-most-once | At-least-once |
| 메시지 확인(ACK) | ✗ | ✓ |
| 소비자 그룹 | ✗ | ✓ |
| 메시지 재처리 | ✗ | ✓ |
| 오프라인 구독자 지원 | ✗ | ✓ |
| 지연 시간 | 매우 낮음 | 낮음 |
| 메모리 사용 | 낮음 | 높음 |

### 11.2 선택 가이드

**Pub/Sub 적합:**
- 실시간 알림 (손실 허용)
- 라이브 채팅
- 이벤트 브로드캐스팅
- 캐시 무효화

**Streams 적합:**
- 이벤트 소싱
- 작업 큐
- 로그 수집
- 신뢰성 필요한 메시징

---

## 참고 자료

- [Redis 공식 Pub/Sub 문서](https://redis.io/docs/latest/develop/pubsub/)
- [Redis Commands - PUBLISH](https://redis.io/commands/publish/)
- [Redis Commands - SUBSCRIBE](https://redis.io/commands/subscribe/)
- [Redis Commands - SPUBLISH](https://redis.io/commands/spublish/)
- [Redis Cluster Specification](https://redis.io/docs/reference/cluster-spec/)

---

*이 문서는 Context7 및 Redis 공식 문서를 기반으로 작성되었습니다.*

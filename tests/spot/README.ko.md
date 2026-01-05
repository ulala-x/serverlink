[![English](https://img.shields.io/badge/lang:en-red.svg)](README.md) [![한국어](https://img.shields.io/badge/lang:한국어-blue.svg)](README.ko.md)

# SPOT PUB/SUB Tests

ServerLink SPOT (Scalable PUB/SUB over Topics) 테스트 스위트.

## 테스트 현황

**전체 테스트: 31개 (모두 통과)** ✅

| 테스트 파일 | 테스트 수 | 상태 |
|-------------|-----------|------|
| test_spot_basic | 11 | ✅ PASS |
| test_spot_local | 6 | ✅ PASS |
| test_spot_remote | 5 | ✅ PASS |
| test_spot_cluster | 4 | ✅ PASS |
| test_spot_mixed | 5 | ✅ PASS |

## 테스트 구성

### test_spot_basic.cpp (11개 테스트)

기본 SPOT 기능 테스트:
- `test_spot_create_destroy` - Instance Lifecycle
- `test_spot_topic_create` - 단일 Topic 생성
- `test_spot_topic_create_multiple` - 다중 Topic 생성
- `test_spot_subscribe` - 기본 Subscribe
- `test_spot_subscribe_multiple` - 다중 Subscribe
- `test_spot_unsubscribe` - Unsubscribe
- `test_spot_subscribe_pattern` - Pattern 기반 Subscribe
- `test_spot_basic_pubsub` - 기본 Publish/Subscribe
- `test_spot_publish_nonexistent` - 존재하지 않는 Topic Error 처리
- `test_spot_multiple_messages` - 메시지 순서 및 전달
- `test_spot_topic_destroy` - Topic 정리

### test_spot_local.cpp (6개 테스트)

LOCAL Publish/Subscribe 시나리오:
- `test_spot_multi_topic` - 단일 Instance에서 다중 Topic
- `test_spot_multi_subscriber` - 같은 Topic에 다중 Subscriber
- `test_spot_pattern_matching` - Pattern 기반 Topic Filtering
- `test_spot_selective_unsubscribe` - 선택적 Unsubscribe
- `test_spot_large_message` - 1MB 메시지 처리
- `test_spot_rapid_pubsub` - 고빈도 메시징 (100개 메시지)

### test_spot_remote.cpp (5개 테스트)

TCP/inproc를 통한 REMOTE 통신:
- `test_spot_remote_tcp` - TCP를 통한 Remote Pub/Sub
- `test_spot_remote_inproc` - inproc를 통한 Remote Pub/Sub
- `test_spot_bidirectional_remote` - 양방향 Node 통신
- `test_spot_reconnect` - 연결 해제 및 재연결 처리
- `test_spot_multiple_remote_subscribers` - 다중 Remote Node로 Broadcast

### test_spot_cluster.cpp (4개 테스트)

다중 Node Cluster 시나리오:
- `test_spot_three_node_cluster` - 3-Node Full Mesh Cluster
- `test_spot_topic_sync` - Cluster 간 Topic 동기화
- `test_spot_node_failure_recovery` - Node 장애 및 복구
- `test_spot_dynamic_membership` - 동적 Cluster Membership 변경

### test_spot_mixed.cpp (5개 테스트)

LOCAL/REMOTE 혼합 시나리오:
- `test_spot_mixed_local_remote` - LOCAL과 REMOTE Subscriber 혼합
- `test_spot_multi_transport` - 다중 Transport (TCP + inproc)
- `test_spot_topic_routing_mixed` - 혼합 소스의 Topic Routing
- `test_spot_pattern_mixed` - 혼합 소스의 Pattern Subscribe
- `test_spot_high_load_mixed` - 고부하 혼합 시나리오 (50개 메시지)

## 테스트 실행

### 전체 SPOT 테스트 실행
```bash
ctest -R spot --output-on-failure
```

### 개별 테스트 파일 실행
```bash
./build/tests/Release/test_spot_basic
./build/tests/Release/test_spot_local
./build/tests/Release/test_spot_remote
./build/tests/Release/test_spot_cluster
./build/tests/Release/test_spot_mixed
```

### Windows에서 실행
```powershell
.\build\tests\Release\test_spot_basic.exe
.\build\tests\Release\test_spot_local.exe
.\build\tests\Release\test_spot_remote.exe
.\build\tests\Release\test_spot_cluster.exe
.\build\tests\Release\test_spot_mixed.exe
```

## 테스트 패턴

모든 테스트는 ServerLink 테스트 규약을 따릅니다:
- `testutil.hpp` Helper Macro 및 함수 사용
- `TEST_ASSERT*` Assertion Pattern
- Context 관리를 통한 Setup/Teardown
- 동기화를 위한 `test_sleep_ms()` 사용
- TCP 연결 수립을 위한 `SETTLE_TIME` (300ms 기본값)
- `slk_spot_destroy()` 및 `slk_ctx_destroy()`를 통한 적절한 Cleanup

## Coverage Matrix

| 기능 | Basic | Local | Remote | Cluster | Mixed |
|------|-------|-------|--------|---------|-------|
| Topic CRUD | ✓ | ✓ | ✓ | ✓ | ✓ |
| Subscribe/Unsubscribe | ✓ | ✓ | ✓ | ✓ | ✓ |
| Pattern Matching | ✓ | ✓ | - | - | ✓ |
| Publish/Receive | ✓ | ✓ | ✓ | ✓ | ✓ |
| Topic Routing | - | ✓ | - | - | ✓ |
| Cluster Management | - | - | ✓ | ✓ | ✓ |
| Multi-Transport | - | - | ✓ | - | ✓ |
| Node Failure | - | - | ✓ | ✓ | - |
| Large Message | - | ✓ | - | - | - |
| High Frequency | - | ✓ | - | - | ✓ |

## 핵심 테스트 시나리오

### Pattern Subscribe (Prefix Matching)

XPUB/XSUB는 Prefix Matching을 사용합니다:
```c
// "events:*" Pattern은 "events:" Prefix로 변환됨
slk_spot_subscribe_pattern(sub, "events:*");

// 다음 Topic들 모두 Matching:
// - events:login
// - events:logout
// - events:user:created
```

### Multi-Publisher 시나리오

```c
// Publisher A와 B가 각각 bind
slk_spot_bind(pub_a, "tcp://*:5555");
slk_spot_bind(pub_b, "tcp://*:5556");

// Subscriber가 둘 다에 연결
slk_spot_cluster_add(sub, "tcp://...:5555");
slk_spot_cluster_add(sub, "tcp://...:5556");
slk_spot_subscribe_pattern(sub, "events:*");

// 두 Publisher의 메시지 모두 수신
```

### 동적 Cluster Membership

```c
// Node 추가
slk_spot_cluster_add(spot, "tcp://new-node:5555");
slk_spot_subscribe(spot, "topic");

// Node 제거 (실제 연결 해제)
slk_spot_cluster_remove(spot, "tcp://old-node:5555");
// 해당 Node로부터 더 이상 메시지 수신 안됨
```

## 참고 사항

- 모든 테스트는 충돌 방지를 위해 임시 TCP Port 사용
- Resource 누수 방지를 위한 적절한 Cleanup 포함
- Timing에 민감한 테스트는 설정 가능한 `SETTLE_TIME` 사용 (기본 300ms)
- 대용량 메시지 테스트는 1MB Payload 처리 검증
- Pattern Matching은 XPUB Prefix 스타일 사용 (`events:*` → `events:`)

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
- `test_spot_create_destroy` - 인스턴스 생명주기
- `test_spot_topic_create` - 단일 토픽 생성
- `test_spot_topic_create_multiple` - 다중 토픽 생성
- `test_spot_subscribe` - 기본 구독
- `test_spot_subscribe_multiple` - 다중 구독
- `test_spot_unsubscribe` - 구독 해제
- `test_spot_subscribe_pattern` - 패턴 기반 구독
- `test_spot_basic_pubsub` - 기본 발행/구독
- `test_spot_publish_nonexistent` - 존재하지 않는 토픽 에러 처리
- `test_spot_multiple_messages` - 메시지 순서 및 전달
- `test_spot_topic_destroy` - 토픽 정리

### test_spot_local.cpp (6개 테스트)

로컬 발행/구독 시나리오:
- `test_spot_multi_topic` - 단일 인스턴스에서 다중 토픽
- `test_spot_multi_subscriber` - 같은 토픽에 다중 구독자
- `test_spot_pattern_matching` - 패턴 기반 토픽 필터링
- `test_spot_selective_unsubscribe` - 선택적 구독 해제
- `test_spot_large_message` - 1MB 메시지 처리
- `test_spot_rapid_pubsub` - 고빈도 메시징 (100개 메시지)

### test_spot_remote.cpp (5개 테스트)

TCP/inproc를 통한 원격 통신:
- `test_spot_remote_tcp` - TCP를 통한 원격 pub/sub
- `test_spot_remote_inproc` - inproc를 통한 원격 pub/sub
- `test_spot_bidirectional_remote` - 양방향 노드 통신
- `test_spot_reconnect` - 연결 해제 및 재연결 처리
- `test_spot_multiple_remote_subscribers` - 다중 원격 노드로 브로드캐스트

### test_spot_cluster.cpp (4개 테스트)

다중 노드 클러스터 시나리오:
- `test_spot_three_node_cluster` - 3노드 풀 메시 클러스터
- `test_spot_topic_sync` - 클러스터 간 토픽 동기화
- `test_spot_node_failure_recovery` - 노드 장애 및 복구
- `test_spot_dynamic_membership` - 동적 클러스터 멤버십 변경

### test_spot_mixed.cpp (5개 테스트)

로컬/원격 혼합 시나리오:
- `test_spot_mixed_local_remote` - 로컬과 원격 구독자 혼합
- `test_spot_multi_transport` - 다중 전송 (TCP + inproc)
- `test_spot_topic_routing_mixed` - 혼합 소스의 토픽 라우팅
- `test_spot_pattern_mixed` - 혼합 소스의 패턴 구독
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
- `testutil.hpp` 헬퍼 매크로 및 함수 사용
- `TEST_ASSERT*` 어설션 패턴
- 컨텍스트 관리를 통한 setup/teardown
- 동기화를 위한 `test_sleep_ms()` 사용
- TCP 연결 수립을 위한 `SETTLE_TIME` (300ms 기본값)
- `slk_spot_destroy()` 및 `slk_ctx_destroy()`를 통한 적절한 정리

## 커버리지 매트릭스

| 기능 | Basic | Local | Remote | Cluster | Mixed |
|------|-------|-------|--------|---------|-------|
| 토픽 CRUD | ✓ | ✓ | ✓ | ✓ | ✓ |
| 구독/해제 | ✓ | ✓ | ✓ | ✓ | ✓ |
| 패턴 매칭 | ✓ | ✓ | - | - | ✓ |
| 발행/수신 | ✓ | ✓ | ✓ | ✓ | ✓ |
| 토픽 라우팅 | - | ✓ | - | - | ✓ |
| 클러스터 관리 | - | - | ✓ | ✓ | ✓ |
| 다중 전송 | - | - | ✓ | - | ✓ |
| 노드 장애 | - | - | ✓ | ✓ | - |
| 대용량 메시지 | - | ✓ | - | - | - |
| 고빈도 | - | ✓ | - | - | ✓ |

## 핵심 테스트 시나리오

### 패턴 구독 (Prefix Matching)

XPUB/XSUB는 prefix 매칭을 사용합니다:
```c
// "events:*" 패턴은 "events:" prefix로 변환됨
slk_spot_subscribe_pattern(sub, "events:*");

// 다음 토픽들 모두 매칭:
// - events:login
// - events:logout
// - events:user:created
```

### 다중 Publisher 시나리오

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

### 동적 클러스터 멤버십

```c
// 노드 추가
slk_spot_cluster_add(spot, "tcp://new-node:5555");
slk_spot_subscribe(spot, "topic");

// 노드 제거 (실제 연결 해제)
slk_spot_cluster_remove(spot, "tcp://old-node:5555");
// 해당 노드로부터 더 이상 메시지 수신 안됨
```

## 참고 사항

- 모든 테스트는 충돌 방지를 위해 임시 TCP 포트 사용
- 리소스 누수 방지를 위한 적절한 정리 포함
- 타이밍에 민감한 테스트는 설정 가능한 `SETTLE_TIME` 사용 (기본 300ms)
- 대용량 메시지 테스트는 1MB 페이로드 처리 검증
- 패턴 매칭은 XPUB prefix 스타일 사용 (`events:*` → `events:`)

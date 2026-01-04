# SPOT PUB/SUB 샘플 코드 요약

## 생성된 파일 목록

### 1. 기본 샘플

| 파일 | 크기 | 설명 |
|------|------|------|
| `spot_basic.c` | 7.0 KB | SPOT 기본 사용법 (생성, 발행, 구독, 수신) |
| `spot_multi_topic.c` | 8.1 KB | 다중 토픽 관리, 패턴 구독, HWM 설정 |

### 2. 클러스터 샘플

| 파일 | 크기 | 설명 |
|------|------|------|
| `spot_cluster_publisher.c` | 5.4 KB | 클러스터 발행자 (서버 모드, TCP 바인딩) |
| `spot_cluster_subscriber.c` | 6.4 KB | 클러스터 구독자 (원격 토픽 동기화) |

### 3. 게임 시나리오 샘플

| 파일 | 크기 | 설명 |
|------|------|------|
| `spot_mmorpg_cell.c` | 12.8 KB | MMORPG 셀 기반 pub/sub (위치 투명성 데모) |

### 4. 문서

| 파일 | 크기 | 설명 |
|------|------|------|
| `SPOT_EXAMPLES_README.md` | 7.4 KB | 전체 예제 가이드 및 API 레퍼런스 |
| `CMakeLists.txt` | 2.6 KB | 빌드 설정 (업데이트됨) |

## 각 샘플 코드 상세

### spot_basic.c

**학습 내용:**
- slk_spot_new() - SPOT 인스턴스 생성
- slk_spot_topic_create() - 로컬 토픽 생성
- slk_spot_subscribe() - 토픽 구독
- slk_spot_publish() - 메시지 발행
- slk_spot_recv() - 메시지 수신
- slk_spot_unsubscribe() - 구독 해제
- slk_spot_list_topics() - 토픽 목록 조회

**실행 흐름:**
1. 3개 토픽 생성 (news:weather, news:sports, alerts:traffic)
2. 2개 토픽 구독 (news:weather, alerts:traffic)
3. 3개 메시지 발행 (모든 토픽)
4. 2개 메시지 수신 (구독된 토픽만)
5. 구독 해제 및 필터링 확인

**예상 출력:**
```
Creating local topics...
  ✓ Created topic: news:weather
  ✓ Created topic: news:sports
  ✓ Created topic: alerts:traffic

Subscribing to topics...
  ✓ Subscribed to: news:weather
  ✓ Subscribed to: alerts:traffic

Publishing messages...
  ✓ Published to news:weather: Sunny, 25°C
  ✓ Published to news:sports: Team A wins 3-2 (not subscribed)
  ✓ Published to alerts:traffic: Highway A1 congestion

Receiving messages (expecting 2)...
  [1] Topic: news:weather
      Data:  Sunny, 25°C
  [2] Topic: alerts:traffic
      Data:  Highway A1 congestion

Received 2 messages (news:sports was filtered out)
```

### spot_multi_topic.c

**학습 내용:**
- slk_spot_set_hwm() - HWM (High Water Mark) 설정
- slk_spot_subscribe_pattern() - 패턴 기반 구독
- slk_spot_topic_exists() - 토픽 존재 확인
- slk_spot_topic_is_local() - 로컬/원격 구분
- slk_spot_topic_destroy() - 토픽 동적 삭제

**실행 흐름:**
1. HWM 1000으로 설정
2. 8개 알림 토픽 생성 (email, sms, push, system)
3. 패턴 구독: "notify:email:*" (이메일만)
4. 정확 구독: "notify:system:critical"
5. 6개 메시지 발행
6. 3개 메시지 수신 (필터링됨)
7. 이메일 토픽 삭제

**패턴 구독 데모:**
```c
slk_spot_subscribe_pattern(spot, "notify:email:*");
// → notify:email:user1 ✓
// → notify:email:user2 ✓
// → notify:sms:user1  ✗ (filtered)
// → notify:push:user1 ✗ (filtered)
```

### spot_cluster_publisher.c

**학습 내용:**
- slk_spot_bind() - 서버 모드 시작 (ROUTER 소켓)
- 원격 클라이언트 연결 수락
- 주기적 메시지 발행 (실시간 시뮬레이션)

**실행 흐름:**
1. tcp://*:5555 바인딩 (서버 모드)
2. 6개 토픽 생성 (stock, forex, crypto)
3. 1초 간격으로 10라운드 발행
4. 각 라운드마다 가격 변동 시뮬레이션
5. JSON 형식 메시지 발행

**메시지 형식:**
```json
{
  "price": 150.25,
  "volume": 5432,
  "timestamp": 1704355200
}
```

### spot_cluster_subscriber.c

**학습 내용:**
- slk_spot_cluster_add() - 클러스터 노드 추가
- slk_spot_cluster_sync() - 토픽 동기화
- 원격 토픽 발견 및 구독
- TCP를 통한 메시지 수신

**실행 흐름:**
1. tcp://localhost:5555 연결 (Publisher에 연결)
2. 클러스터 동기화 (원격 토픽 발견)
3. 발견된 토픽 목록 출력
4. 3개 토픽 구독 (AAPL, GOOGL, BTC)
5. 최대 30개 메시지 수신
6. 구독 해제 및 연결 종료

**위치 투명성 데모:**
```c
// Publisher 측 (tcp://*:5555)
slk_spot_topic_create(spot, "stock:prices:AAPL");  // LOCAL

// Subscriber 측 (tcp://localhost:5555)
slk_spot_subscribe(spot, "stock:prices:AAPL");     // REMOTE!
// → 동일한 API, 내부적으로 TCP 라우팅
```

### spot_mmorpg_cell.c

**학습 내용:**
- slk_spot_topic_route() - 원격 토픽 라우팅 설정
- 공간 기반 관심 영역 관리 (Spatial Interest Management)
- 로컬/원격 토픽 혼합 사용
- 게임 서버 아키텍처 패턴

**시나리오:**
```
Server A (이 예제)          Server B (원격)
┌──────────────┐          ┌──────────────┐
│ cell(5,7) ✓  │          │ cell(6,7) ✓  │
│ cell(5,8) ✓  │          └──────────────┘
└──────────────┘                 ▲
      │                          │
      └──────── TCP ─────────────┘
      (인접 셀 구독 - AoI)
```

**실행 흐름:**
1. tcp://*:5555 바인딩 (서버 모드)
2. 로컬 셀 생성: cell(5,7), cell(5,8)
3. 원격 셀 라우팅: cell(6,7) → tcp://localhost:6666
4. 인접 셀 구독 (Area of Interest)
5. 플레이어 이벤트 발행 (4개)
6. 인접 셀 이벤트 수신 (2개)

**플레이어 이벤트 구조:**
```json
{
  "player": "hero1",
  "cell": "(5,7)",
  "action": "move",
  "health": 100
}
```

**위치 투명성의 핵심:**
```c
// 로컬 셀에 발행 - inproc (초고속)
slk_spot_publish(spot, "zone1:cell:5,7", data, len);

// 원격 셀에 발행 - TCP (자동 라우팅)
slk_spot_publish(spot, "zone1:cell:6,7", data, len);

// 동일한 API! 개발자는 위치를 신경쓰지 않음!
```

## 빌드 방법

```bash
cd D:\project\ulalax\serverlink
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8

# 개별 샘플 빌드
cmake --build build --target spot_basic
cmake --build build --target spot_multi_topic
cmake --build build --target spot_cluster_publisher
cmake --build build --target spot_cluster_subscriber
cmake --build build --target spot_mmorpg_cell
```

## 실행 순서

### 1. 기본 샘플 (단독 실행)
```bash
cd build/examples
./spot_basic
./spot_multi_topic
./spot_mmorpg_cell
```

### 2. 클러스터 샘플 (2개 터미널 필요)

**터미널 1 - Publisher:**
```bash
cd build/examples
./spot_cluster_publisher
# Listening on tcp://*:5555...
```

**터미널 2 - Subscriber:**
```bash
cd build/examples
./spot_cluster_subscriber
# Connected to tcp://localhost:5555...
# Receiving messages...
```

## 주요 개념

### 1. 위치 투명성 (Location Transparency)
- 로컬 토픽: inproc 전송 (고성능)
- 원격 토픽: TCP 전송 (자동 라우팅)
- **동일한 API로 접근** → 개발 간소화

### 2. 토픽 기반 라우팅
- 토픽 ID로 메시지 필터링
- 정확 매칭 또는 패턴 매칭
- 구독자만 메시지 수신

### 3. 클러스터 동기화
- 토픽 발견 자동화
- 클러스터 노드 간 협업
- 동적 토픽 등록/해제

### 4. HWM (High Water Mark)
- 메시지 큐 크기 제한
- HWM 도달 시:
  - XPUB_NODROP=0: 메시지 드롭
  - XPUB_NODROP=1: 블로킹

## 성능 특성

### Local Topics (inproc)
- 처리량: 수백만 msg/s
- 지연시간: 마이크로초 단위
- Zero-copy 메시지 전달
- Lock-free 큐 구현

### Remote Topics (tcp)
- 처리량: 네트워크 제한
- 지연시간: 밀리초 단위
- 자동 재연결
- 설정 가능한 HWM

## 사용 사례

### 게임 서버
- 공간 분할 (셀, 존)
- 관심 영역 관리 (AoI)
- 서버 간 플레이어 이동
- 실시간 이벤트 브로드캐스트

### 마이크로서비스
- 서비스 간 이벤트 통신
- 토픽 기반 라우팅
- 서비스 디스커버리
- 이벤트 소싱

### IoT / 실시간 데이터
- 센서 데이터 수집
- 실시간 모니터링
- 토픽별 데이터 스트림
- 분산 처리

## 에러 처리 예제

```c
// 발행 실패 처리
if (slk_spot_publish(spot, topic, data, len) < 0) {
    int err = slk_errno();
    if (err == SLK_EAGAIN) {
        printf("HWM reached, message dropped\n");
    } else if (err == SLK_ENOENT) {
        printf("Topic not found\n");
    } else {
        fprintf(stderr, "Publish error: %s\n", slk_strerror(err));
    }
}

// 수신 타임아웃 처리
int rc = slk_spot_recv(spot, topic, topic_size, &topic_len,
                       data, data_size, &data_len,
                       SLK_DONTWAIT);
if (rc < 0) {
    if (slk_errno() == SLK_EAGAIN) {
        // 메시지 없음, 정상
    } else {
        fprintf(stderr, "Recv error: %s\n", slk_strerror(slk_errno()));
    }
}
```

## 다음 단계

1. **기본 샘플 실행**: `spot_basic.c`로 시작
2. **다중 토픽**: `spot_multi_topic.c`로 패턴 학습
3. **클러스터 테스트**: publisher/subscriber 동시 실행
4. **게임 시나리오**: `spot_mmorpg_cell.c`로 실전 패턴 학습
5. **프로덕션 적용**: 실제 사용 사례에 맞게 수정

## 참고 문서

- `serverlink.h` - 전체 C API 레퍼런스
- `SPOT_EXAMPLES_README.md` - 상세 가이드
- `CLAUDE.md` - 프로젝트 문서
- `tests/pubsub/` - 추가 pub/sub 예제

## 라이선스

Mozilla Public License 2.0 (MPL-2.0)

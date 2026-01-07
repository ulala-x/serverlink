# ServerLink 롱텀 안정성 테스트 계획서 (V2 - 강화판)

이 문서는 ServerLink의 최종 최적화 버전이 실제 상용 환경(24/7 가동)에서 견딜 수 있는 신뢰성을 갖췄는지 검증하기 위한 상세 가이드입니다.

---

## 1. 개요
기본적인 기능 테스트를 넘어, 장시간 고부하 상태에서 발생하는 **메모리 파편화(Fragmentation)**, **백프레셔(Backpressure)** 하에서의 자원 관리, 그리고 **ASIO 스케줄링 안정성**을 확인합니다.

---

## 2. 소켓 타입별 정밀 테스트

### 2.1 ROUTER 소켓 (연결성 및 식별 관리)
*   **목표**: 수만 명의 동시 접속자가 수시로 드나드는 게임 서버/채팅 서버 시나리오 검증.
*   **시나리오**: 
    1.  **Peer Churn**: 5,000개 피어가 10분간 지속적으로 접속/단절 반복.
    2.  **Unknown Routing**: 존재하지 않는 Routing ID로 메시지 전송 시 에러 처리 및 자원 보존.
    3.  **Large Identity Map**: 10만 개의 Peer 정보를 메모리에 유지할 때의 룩업 지연 시간 측정.

### 2.2 PUB/SUB 소켓 (구독 전파 및 팬아웃)
*   **목표**: 실시간 시세 전파나 대규모 알림 시스템 시나리오 검증.
*   **시나리오**:
    1.  **Trie Stress**: 복잡한 와일드카드(`topic.*.sub`)를 포함한 5,000개 토픽 필터링 부하.
    2.  **Backpressure Test**: 수신 속도가 느린 소비자(Slow Consumer) 발생 시 송신측 큐 제어(HWM) 및 메모리 격리.
    3.  **Message Replication**: 동일한 대용량 메시지를 수천 명에게 보낼 때 참조 카운팅(`msg_t` refcnt)의 정확성.

---

## 3. 정밀 분석 지표 (KPI)

| 지표 | 측정 방법 | 성공 기준 |
| :--- | :--- | :--- |
| **RSS Stability** | `pidstat -r` | 시작 30분 후부터 종료 시까지 메모리 변동성 1% 이내 |
| **Latency Jitter** | `bench_latency` (long) | 24시간 후의 P99 지연 시간이 초기 대비 1.2배 이내 |
| **FD Integrity** | `lsof -p [pid]` | 테스트 종료 후 소켓 FD 개수가 0으로 회수됨 |
| **Zero Crash** | 커널 로그 (dmesg) | SegFault, Invalid Pointer 에러 발생 0건 |

---

## 4. 실행 단계

### Phase 1: 자원 로깅 자동화
*   테스트 중인 프로세스의 자원 사용량을 CSV로 저장하는 쉘 스크립트 실행.

### Phase 2: Sanitizer 검사 (1단계)
*   ASan 및 TSan 빌드로 1시간 동안 스트레스 테스트를 수행하여 잠재적 버그 사전 차단.

### Phase 3: 장시간 부하 테스트 (2단계)
*   ROUTER 및 PUB/SUB 시나리오를 각각 4시간 이상 실행하여 추이 분석.

---
**작성**: ServerLink 성능/안정성 TF (Gemini)

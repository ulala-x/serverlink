# ServerLink 롱텀 안정성 테스트 계획서 (V4 - 통합본)

본 문서는 ServerLink의 ROUTER 및 PUB/SUB 소켓이 실제 서비스 환경에서 장시간 안정적으로 동작하는지 검증하기 위한 최종 계획서입니다.

---

## 1. ROUTER 테스트: 서버 클러스터 가용성
*   **대상**: 1 ROUTER (Bind) ↔ 500 ROUTER (Connect)
*   **목표**: 서버 노드 추가/제거 및 비정상 종료 시의 완벽한 자원 회수 검증.
*   **시나리오**:
    1.  **Massive Peer Churn**: 500개 피어 중 10%를 10분 간격으로 랜덤하게 종료 후 재실행.
    2.  **Zombie Detection**: `ROUTER_NOTIFY` 이벤트를 통해 단절된 세션의 Mailbox와 Pipe가 즉시 해제되는지 확인.
*   **성공 기준**: 24시간 가동 후 FD 누수 0개, 프로세스 RSS(메모리) 수렴.

---

## 2. PUB/SUB 테스트: 데이터 전파 및 필터링 안정성
*   **대상**: 1 PUB (Broadcast) ↔ 1,000 SUB (Subscribe)
*   **목표**: 1:N 팬아웃 부하 상황에서의 메모리 관리 및 토픽 트리(Trie) 안정성 검증.
*   **시나리오**:
    1.  **High Fan-out Stress**: 1,000개 SUB에게 초당 10만 개의 메시지 전파. (msg_t 참조 카운팅 집중 검사)
    2.  **Topic Storm**: 수천 개의 고유 토픽에 대한 구독/해제를 반복하여 내부 `trie_t` 구조의 누수 확인.
    3.  **Slow Consumer (Backpressure)**: 일부 SUB가 메시지를 읽지 않을 때, PUB 소켓의 HWM이 작동하여 전체 시스템 메모리를 보호하는지 확인.
*   **성공 기준**: 24시간 가동 후 메모리 사용량 일정 유지, 지연 시간(p99) 변동폭 10% 이내.

---

## 3. 분석 및 도구 (Tools)
1.  **AddressSanitizer (ASan)**: 메모리 누수 및 오염 실시간 감지 (초기 4시간).
2.  **Resource Monitor (`monitor_resource.sh`)**:
    *   10초 간격으로 `RSS`, `CPU %`, `FD Count` 기록 및 CSV 저장.
3.  **Latency Tracker**:
    *   장시간 가동 중 주기적으로 RTT를 측정하여 성능 저하 추이 기록.

---

## 4. 적용 시나리오
*   **금융**: 24/7 가동되는 시세 전파(PUB/SUB) 및 주문 라우팅(ROUTER) 엔진의 무중단 신뢰성 확보.
*   **게임**: 대규모 풀 메시 서버 환경에서 노드 장애 시에도 통신 레이어가 죽지 않고 복구되는 자생력 증명.

---
**작성**: ServerLink QA 에이전트 (Gemini)

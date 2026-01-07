# ServerLink 도커 기반 롱텀 클러스터 안정성 테스트 계획서

본 계획서는 도커(Docker)를 활용하여 실제 서버 클러스터 환경을 격리 구축하고, 24시간 이상의 연속 가동을 통해 ServerLink의 신뢰성을 최종 검증하기 위해 작성되었습니다.

---

## 1. 테스트 아키텍처 (Star Topology)

*   **Master Container (1개)**: `ROUTER` 소켓 바인드 (중앙 메시지 허브).
*   **Worker Containers (10~20개)**: 각 컨테이너당 50~100개의 피어 로직을 구동하여 총 1,000개 이상의 논리적 연결 유지.
*   **Monitor Container (1개)**: `cAdvisor` 또는 커스트 텔레메트리 컨테이너를 통해 실시간 자원 사용량 데이터 수집.
*   **Network**: Docker Bridge Network를 사용하여 컨테이너 간 독립된 IP 통신 보장.

---

## 2. 핵심 테스트 시나리오

### 시나리오 A: Chaos Resilience (24시간)
*   **내용**: 매 10분마다 1~2개의 워커 컨테이너를 강제 종료(`docker kill`) 후 재시작.
*   **검증 포인트**:
    *   Master(ROUTER)가 단절된 피어의 자원을 즉시 회수하는가?
    *   재시작된 피어가 Master와 다시 연결되었을 때 `Routing ID` 충돌 없이 정상 통신이 재개되는가?
    *   반복적인 재연결에도 Master 프로세스의 메모리가 우상향하지 않는가?

### 시나리오 B: Sustained High-Throughput (24시간)
*   **내용**: PUB/SUB 패턴을 이용하여 초당 5만 개의 메시지를 1,000개 피어에게 지속 전파.
*   **검증 포인트**:
    *   ASIO 비동기 큐에 메시지가 쌓이지 않고 원활하게 처리되는가?
    *   누적 처리량 수천억 건 돌파 시에도 `msg_t` 참조 카운팅 오류가 발생하지 않는가?

### 시나리오 C: Backpressure & Slow Consumer
*   **내용**: 특정 워커 컨테이너의 네트워크 대역폭을 제한하여 수신 속도를 늦춤.
*   **검증 포인트**:
    *   HWM(High Water Mark)이 작동하여 Master의 송신 큐가 메모리를 무한히 점유하는 것을 방지하는가?

---

## 3. 측정 지표 (KPI)

1.  **Memory Jitter**: 24시간 가동 후 초기 안정기 대비 메모리 증가폭 1% 이내 유지.
2.  **FD Stability**: 가동 종료 시점의 File Descriptor 개수가 (연결 수 + 기본 FD)와 일치.
3.  **Success Rate**: 총 전송 메시지 대비 유실/에러 발생률 0.0001% 미만 (정상 종료 케이스 제외).

---

## 4. 실행 도구 및 방법

1.  **Docker Compose**: 클러스터 환경 일괄 배포.
2.  **Chaos Script (`chaos_monkey.sh`)**: 무작위 컨테이너 재시작 수행.
3.  **Resource Logger**: `docker stats` 데이터를 CSV로 수집.

---
**작성**: ServerLink 안정성 검증 TF (Gemini)

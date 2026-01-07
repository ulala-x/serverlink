# ServerLink 롱텀 안정성 및 신뢰성 테스트 계획서

본 문서는 ServerLink의 ASIO Native 최적화 버전이 실제 운영 환경에서 장시간(Long-term) 안정적으로 동작하는지 검증하기 위한 상세 계획을 담고 있습니다.

---

## 1. 테스트 목적
*   **메모리 누수(Memory Leak) 탐지**: 수억 개의 메시지 처리 후에도 메모리 점유율이 유지되는지 확인.
*   **자원 고갈(Resource Exhaustion) 검증**: 소켓 연결/해제 반복 시 File Descriptor(FD) 누수 여부 확인.
*   **지연 시간 안정성(Latency Jitter)**: 장시간 부하 시 가비지 컬렉션(C++의 경우 파편화 등)이나 큐 적체로 인한 지연 시간 튀는 현상 측정.
*   **고가용성(High Concurrency)**: 수천 개의 동시 연결 상태에서 엔진의 안정성 검증.

---

## 2. 테스트 시나리오 및 방법

### 시나리오 A: 초고부하 지속 테스트 (Soak Test)
*   **방법**: `bench_throughput`을 사용하여 초당 100만 건 이상의 메시지를 전송하며 최소 4시간~24시간 유지.
*   **대상**: TCP 및 IPC 전송 방식.
*   **측정**: 1분 간격으로 프로세스의 RSS(Resident Set Size) 및 FD 개수 기록.

### 시나리오 B: 연결/해제 반복 스트레스 테스트 (Churn Test)
*   **방법**: 1,000개의 클라이언트가 동시에 연결 -> 10개 메시지 송수신 -> 즉시 연결 해제 루프를 반복 실행.
*   **목표**: ASIO의 Acceptor 및 엔진 종료 로직에서 자원 회수가 완벽한지 검증.

### 시나리오 C: 대량 동시 연결 테스트 (High-Concurrency Test)
*   **방법**: 단일 ROUTER 소켓에 5,000개 이상의 클라이언트를 연결하고 모든 클라이언트가 소량의 하트비트를 주고받는 상태 유지.
*   **측정**: 시스템 콜(epoll_wait) 부하 및 CPU 점유율 분산 확인.

---

## 3. 정밀 분석 도구 (Tooling)

1.  **AddressSanitizer (ASan)**:
    *   빌드 시 `-fsanitize=address` 적용.
    *   작은 단위의 메모리 누수(Leak) 및 Invalid Access 즉시 감지.
2.  **Valgrind (Memcheck)**:
    *   초기 10분간 실행하여 초기화되지 않은 메모리 읽기 등 정밀 분석.
3.  **Prometheus + Node Exporter (또는 커스텀 스크립트)**:
    *   시간 흐름에 따른 메모리 사용량 그래프 시각화.
4.  **`pidstat` / `lsof`**:
    *   프로세스 레벨의 자원 사용량 모니터링.

---

## 4. 성공 판정 기준 (Exit Criteria)

1.  **Memory Stability**: 테스트 시작 10분 후부터 종료 시점까지 메모리 증가폭이 1% 미만이어야 함.
2.  **FD Integrity**: 테스트 종료 후 `lsof` 결과가 시작 전과 동일해야 함 (누수 0개).
3.  **Zero Crash**: 테스트 기간 중 단 한 번의 SegFault나 Assertion Failure도 발생하지 않아야 함.
4.  **Latency P99**: 장시간 부하 후에도 64B 메시지의 P99 지연 시간이 시작 시점 대비 20% 이상 악화되지 않아야 함.

---

## 5. 실행 로드맵

1.  **Phase 1**: ASan 빌드를 통한 1차 정밀 검사 (1시간).
2.  **Phase 2**: 로컬 환경 4시간 Soak Test 및 모니터링 데이터 수집.
3.  **Phase 3**: 결과 분석 및 필요 시 보합 수정.
4.  **Phase 4**: 최종 24시간 안정성 검증 완료 보고.

---
**작성**: ServerLink QA 에이전트 (Gemini)
**일자**: 2026-01-07

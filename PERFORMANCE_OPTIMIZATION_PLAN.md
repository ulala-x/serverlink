# ServerLink 성능 최적화 로드맵 (ASIO Native)

이 문서는 ASIO 기반의 안정화된 ServerLink 베이스라인 위에서 시스템의 지연시간을 줄이고 처리량을 극대화하기 위한 단계별 최적화 계획을 담고 있습니다. 모든 단계는 **테스트 통과**와 **성능 측정**을 전제로 진행됩니다.

---

## 🚀 단계별 계획

### Phase 1: 메모리 관리 및 할당 최적화 (Memory & Locality)
*   **목표**: `malloc`/`free` 호출 횟수를 최소화하고 데이터 캐시 적중률 향상.
*   **세부 작업**:
    1.  `msg_t` 객체 풀링(Object Pooling) 도입 검토.
    2.  `ypipe_t` 및 `yqueue_t` 청크 할당 최적화 (Small Buffer Optimization).
    3.  `slnp` 프로토콜 인코딩/디코딩 시 중간 복사 제거 (Zero-copy 강화).
*   **검증**: `bench_latency` (소형 메시지 RTT 개선 확인).

### Phase 2: ASIO 엔진 및 버퍼 튜닝 (ASIO Engine Tuning)
*   **목표**: ASIO 비동기 핸들러 오버헤드 감소 및 시스템 콜 최적화.
*   **세부 작업**:
    1.  `async_read`/`async_write` 시 사용되는 엔진 내부 버퍼 크기 튜닝.
    2.  `io_context::run` 옵션 및 실행 정책 최적화 (Concurrency Hint 등).
    3.  리눅스 `epoll` 관련 ASIO 네이티브 최적화 플래그 적용.
*   **검증**: `bench_throughput` (초당 메시지 처리량 확인).

### Phase 3: CPU 효율 및 핫패스(Hot-path) 최적화
*   **목표**: 메시지 송수신 경로상의 불필요한 연산 및 컨텍스트 스위칭 제거.
*   **세부 작업**:
    1.  `socket_base_t::send`/`recv` 경로의 가상 함수 호출 최소화 (Inline화).
    2.  Lock-free 큐(`ypipe`)의 원자적 연산 경합(Contention) 분석 및 최적화.
    3.  데이터 구조체 패딩(Padding)을 통한 False Sharing 방지.
*   **검증**: `bench_profile` (CPU 사이클 소모 분석).

### Phase 4: 프로토콜 및 일괄 처리(Batching) 최적화
*   **목표**: 네이티브 프로토콜(`slnp`)의 효율성 극대화.
*   **세부 작업**:
    1.  부하 상태에 따른 적응형 일괄 처리(Adaptive Batching) 알고리즘 개선.
    2.  헤더 필드 크기 축소 및 정렬 최적화.
    3.  라우터 소켓의 라우팅 테이블(Map) 룩업 최적화 (Trie 또는 Hash 최적화).
*   **검증**: `bench_spot_throughput` (실제 PUB/SUB 시나리오 성능).

---

## 📊 성능 측정 지표 (KPI)
모든 최적화는 다음 지표를 기준으로 평가됩니다.

1.  **RTT (Round-Trip Time)**: 64B 메시지 기준 < 60us 목표 (현재 ~79us).
2.  **Throughput**: 64B TCP 기준 > 6,000,000 msg/s 목표 (현재 ~518만).
3.  **CPU Efficiency**: 메시지당 소모되는 CPU 사이클 수 감소.

---

**작성**: Gemini (성능 최적화 에이전트)
**일자**: 2026-01-07

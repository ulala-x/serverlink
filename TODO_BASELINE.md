# ServerLink vs libzmq Golden Baseline 정렬 TODO (2026-01-10)

## 1. 기반 객체 (Infrastructure)
- [ ] `src/core/object.hpp` / `src/core/object.cpp` (명령 전달 시스템)
- [ ] `src/core/own.hpp` / `src/core/own.cpp` (소유권 및 생명주기 관리)
- [ ] `src/core/socket_base.hpp` / `src/core/socket_base.cpp` (소켓 공통 로직)
- [ ] `src/core/session_base.hpp` / `src/core/session_base.cpp` (세션 관리)

## 2. 파이프 및 큐 (Pipe & Queue)
- [ ] `src/util/yqueue.hpp` (고성능 락-프리 큐)
- [ ] `src/util/ypipe.hpp` (고성능 락-프리 파이프)
- [ ] `src/pipe/pipe.hpp` / `src/pipe/pipe.cpp` (메시징 파이프)
- [ ] `src/pipe/lb.hpp` / `src/pipe/lb.cpp` (Load Balancer - O(1))
- [ ] `src/pipe/fq.hpp` / `src/pipe/fq.cpp` (Fair Queuer - O(1))
- [ ] `src/pipe/dist.hpp` / `src/pipe/dist.cpp` (Distributor)
- [ ] `src/pipe/mtrie.hpp` (Multi-trie 구독 관리)

## 3. 메시지 및 데이터 (Message & Data)
- [ ] `src/msg/msg.hpp` / `src/msg/msg.cpp` (메시지 객체 및 참조 카운팅)
- [ ] `src/msg/blob.hpp` (Routing ID 저장용 객체)
- [ ] `src/msg/metadata.hpp` / `src/msg/metadata.cpp` (메시지 속성 관리)

## 4. 엔진 및 프로토콜 (Engine & Protocol)
- [ ] `src/protocol/stream_engine_base.hpp` / `src/protocol/stream_engine_base.cpp` (TCP/IPC 전송 핵심)
- [ ] `src/protocol/zmtp_engine.cpp` (ZMTP 프로토콜 핸드셰이크)
- [ ] `src/protocol/wire.hpp` (ZMTP 와이어 프로토콜 정의)

## 5. 설정 및 유틸리티 (Config & Utils)
- [ ] `src/core/options.hpp` / `src/core/options.cpp` (HWM, Linger 등 소켓 옵션)
- [ ] `src/util/atomic_counter.hpp` (참조 카운팅용 아토믹 카운터)

## 6. 소켓 구현 (Socket Implementation)
- [ ] `src/core/pair.cpp` (PAIR 패턴)
- [ ] `src/core/dealer.cpp` (DEALER 패턴)
- [ ] `src/core/router.cpp` (ROUTER 패턴)

---
**총 28개 파일 정밀 검수 대기 중**
# ServerLink libzmq 테스트 포팅 작업

## 작업 개요
libzmq의 ROUTER 관련 테스트 10개를 ServerLink API에 맞게 포팅했습니다.

## 포팅된 테스트 (10개)

### Critical 우선순위 (3개)
1. **test_router_notify.cpp** - ROUTER_NOTIFY 연결/해제 알림 
2. **test_router_mandatory_hwm.cpp** - ROUTER_MANDATORY + HWM 조합
3. **test_spec_router.cpp** - ROUTER 스펙 준수, fair-queueing

### High 우선순위 (4개)
4. **test_connect_rid.cpp** - CONNECT_ROUTING_ID 옵션
5. **test_probe_router.cpp** - PROBE_ROUTER 옵션 ✅ 통과
6. **test_hwm.cpp** - HWM 기본 동작
7. **test_sockopt_hwm.cpp** - HWM 동적 변경

### Medium 우선순위 (3개)
8. **test_inproc_connect.cpp** - inproc 전송
9. **test_bind_after_connect.cpp** - connect-before-bind
10. **test_reconnect_ivl.cpp** - 재연결 간격

## 빌드 및 테스트

### 빌드
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 8
```

### 테스트 실행
```bash
cd build && ctest -L router --output-on-failure
```

## 테스트 결과 (2026-01-01)

### ✅ 통과 (5/17)
- test_router_basic
- test_router_mandatory
- test_router_handover
- test_router_to_router
- test_probe_router

### ⏱️ 타임아웃 (4개) - 메시지 형식 수정 필요
- test_router_notify
- test_router_mandatory_hwm
- test_spec_router
- test_connect_rid

### ❌ 실패 (2개)
- test_hwm (assertion: ROUTER 통신 문제)
- test_sockopt_hwm (segfault)

## 주요 차이점: 메시지 형식

ServerLink ROUTER는 **empty delimiter frame** 필요:

### libzmq
```c
routing_id → payload
```

### ServerLink  
```c
routing_id → empty_delimiter → payload
```

### 올바른 송신
```c
slk_send(socket, routing_id, id_len, SLK_SNDMORE);
slk_send(socket, "", 0, SLK_SNDMORE);  // delimiter
slk_send(socket, payload, len, 0);
```

### 올바른 수신
```c
slk_recv(socket, buf, size, 0);  // routing ID
slk_recv(socket, buf, size, 0);  // empty delimiter
slk_recv(socket, buf, size, 0);  // payload
```

## 다음 단계

### 1. 메시지 형식 수정 (우선)
모든 ROUTER 송수신에 delimiter 처리 추가

### 2. 테스트 검증
수정 후 전체 테스트 재실행

### 3. 문서화
차이점 및 포팅 가이드 작성

## 파일 위치

- libzmq 원본: `/home/ulalax/project/ulalax/libzmq-native/deps/linux-x64/zeromq-4.3.5/tests/`
- ServerLink 테스트: `/home/ulalax/project/ulalax/serverlink/tests/`
- 참고 테스트: `tests/integration/test_router_to_router.cpp`

---
작성일: 2026-01-01

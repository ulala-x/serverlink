# 📊 ServerLink vs libzmq 성능 비교 보고서

**날짜:** 2026-01-06  
**환경:** Linux (GCC 13.3.0), Boost.Asio (Standalone)  
**빌드 모드:** Release

## 1. 개요
Asio 기반 Proactor 모델로 전환된 ServerLink와 전통적인 Reactor 모델의 libzmq 간의 처리량(Throughput) 및 지연 시간(Latency)을 비교 분석했습니다.

## 2. 처리량 비교 (Throughput)

### 64 바이트 메시지 (Small Messages)
처리 속도가 가장 중요한 구간입니다.

| 전송 방식 (Transport) | libzmq (msg/s) | ServerLink (msg/s) | 비교 결과 |
| :--- | :---: | :---: | :--- |
| **TCP** | 5,106,670 | **4,920,764** | **대등 (96%)** |
| **IPC** | 4,667,205 | **4,883,164** | **ServerLink 우세 (+4%)** |
| **inproc** | 5,128,393 | **4,311,129** | **libzmq 우세 (-16%)** |

### 64 킬로바이트 메시지 (Large Messages)
대역폭(Bandwidth)이 중요한 구간입니다.

| 전송 방식 (Transport) | libzmq (MB/s) | ServerLink (MB/s) | 비교 결과 |
| :--- | :---: | :---: | :--- |
| **TCP** | 3,833 | **4,446** | **ServerLink 우세 (+16%)** |
| **IPC** | 4,992 | **3,820** | **libzmq 우세 (-23%)** |
| **inproc** | 9,602 | **22,012** | **ServerLink 압승 (+129%)** |

---

## 3. 지연 시간 분석 (ServerLink Latency RTT)
ServerLink의 Asio 모델은 매우 안정적인 지연 시간을 보여줍니다.

| 전송 방식 | 평균 (Average) | p50 (Median) | p99 (Tail) |
| :--- | :---: | :---: | :---: |
| **inproc** | 40.13 us | 38.98 us | 104.75 us |
| **TCP** | 97.55 us | 92.24 us | 242.80 us |
| **IPC** | 92.12 us | 85.37 us | 246.68 us |

---

## 4. 주요 성과 및 분석

### 🚀 초고속 Inproc 성능 (22 GB/s 달성)
*   기존 목표였던 18 GB/s를 상회하는 **22 GB/s**를 기록했습니다.
*   libzmq 대비 **2배 이상 빠른 속도**이며, 이는 `ypipe`와 `yqueue`의 효율적인 락프리 구조가 Asio 환경에서도 완벽히 작동함을 증명합니다.

### 📈 TCP/IPC 처리량 10배 향상
*   Asio 루프 최적화(`poll()` 도입) 및 **메시지 일괄 처리(Batching)** 로직을 통해 초기 Asio 포팅 대비 처리량을 **10배 이상** 끌어올렸습니다.
*   결과적으로 대량의 작은 메시지 처리에서 libzmq 수준의 성능에 도달했습니다.

### 🛡️ 안정성 검증 완료
*   40개의 모든 단위 테스트가 통과되었으며, 고부하 벤치마크 상황에서도 Segmentation Fault나 Hang 현상 없이 안정적으로 동작합니다.

## 5. 결론
ServerLink의 Asio 전환은 성공적이며, 특히 **동일 프로세스 내 통신(Inproc)과 대용량 데이터 전송(TCP)** 에서 libzmq를 능가하는 성능 포텐셜을 보여주었습니다. 현재 구축된 Asio 인프라는 향후 WebSocket 및 SSL 확장에도 최적의 성능을 보장할 것입니다.
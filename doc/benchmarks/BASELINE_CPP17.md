# ServerLink C++17 베이스라인 성능 측정

**측정일**: 2026-01-02
**C++ 표준**: C++17
**빌드 타입**: Release (-O3)
**플랫폼**: Linux (WSL2)

---

## 1. 처리량 (Throughput) 벤치마크

### ROUTER-ROUTER 패턴

| Transport | Message Size | Message Count | Time | Throughput | Bandwidth |
|-----------|-------------|---------------|------|------------|-----------|
| **TCP** | 64 bytes | 100,000 | 21.43 ms | **4,667,387 msg/s** | 284.87 MB/s |
| **inproc** | 64 bytes | 100,000 | 23.61 ms | **4,235,510 msg/s** | 258.52 MB/s |
| **IPC** | 64 bytes | 100,000 | 20.84 ms | **4,798,712 msg/s** | 292.89 MB/s |
| TCP | 1,024 bytes | 50,000 | 58.74 ms | 851,166 msg/s | 831.22 MB/s |
| inproc | 1,024 bytes | 50,000 | 34.91 ms | 1,432,124 msg/s | 1,398.56 MB/s |
| IPC | 1,024 bytes | 50,000 | 48.88 ms | 1,022,885 msg/s | 998.91 MB/s |
| TCP | 8,192 bytes | 10,000 | 53.39 ms | 187,291 msg/s | 1,463.21 MB/s |
| inproc | 8,192 bytes | 10,000 | 14.18 ms | 704,976 msg/s | **5,507.63 MB/s** |
| IPC | 8,192 bytes | 10,000 | 48.40 ms | 206,609 msg/s | 1,614.14 MB/s |
| TCP | 65,536 bytes | 1,000 | 15.27 ms | 65,483 msg/s | 4,092.67 MB/s |
| inproc | 65,536 bytes | 1,000 | 4.61 ms | 216,842 msg/s | **13,552.62 MB/s** |
| IPC | 65,536 bytes | 1,000 | 18.71 ms | 53,454 msg/s | 3,340.87 MB/s |

---

## 2. 지연시간 (Latency) 벤치마크

### Round-Trip Time (RTT)

| Transport | Message Size | Average | p50 | p95 | p99 |
|-----------|-------------|---------|-----|-----|-----|
| **TCP** | 64 bytes | 78.77 μs | 60.61 μs | 133.40 μs | 278.90 μs |
| **inproc** | 64 bytes | **30.41 μs** | **25.20 μs** | 48.12 μs | 71.32 μs |
| **IPC** | 64 bytes | 73.97 μs | 58.80 μs | 116.27 μs | 254.08 μs |
| TCP | 1,024 bytes | 88.61 μs | 84.89 μs | 141.91 μs | 250.52 μs |
| inproc | 1,024 bytes | **43.20 μs** | **40.54 μs** | 74.98 μs | 115.40 μs |
| IPC | 1,024 bytes | 73.78 μs | 57.31 μs | 135.60 μs | 245.16 μs |
| TCP | 8,192 bytes | 102.14 μs | 94.11 μs | 177.42 μs | 316.43 μs |
| inproc | 8,192 bytes | **48.84 μs** | **42.66 μs** | 78.78 μs | 144.16 μs |
| IPC | 8,192 bytes | 77.58 μs | 62.81 μs | 132.24 μs | 274.94 μs |

> Note: 단방향 지연시간 ≈ RTT / 2

---

## 3. PUB/SUB 벤치마크

### 단일 PUB → 단일 SUB

| Transport | Message Size | Message Count | Time | Throughput | Bandwidth |
|-----------|-------------|---------------|------|------------|-----------|
| **TCP** | 64 bytes | 100,000 | 19.59 ms | **5,103,522 msg/s** | 311.49 MB/s |
| **inproc** | 64 bytes | 100,000 | 19.82 ms | **5,045,692 msg/s** | 307.96 MB/s |
| **IPC** | 64 bytes | 100,000 | 18.82 ms | **5,313,050 msg/s** | 324.28 MB/s |
| TCP | 1,024 bytes | 50,000 | 55.25 ms | 904,960 msg/s | 883.75 MB/s |
| inproc | 1,024 bytes | 50,000 | 41.42 ms | 1,207,041 msg/s | 1,178.75 MB/s |
| IPC | 1,024 bytes | 50,000 | 48.61 ms | 1,028,644 msg/s | 1,004.54 MB/s |
| TCP | 8,192 bytes | 10,000 | 61.70 ms | 162,075 msg/s | 1,266.21 MB/s |
| inproc | 8,192 bytes | 10,000 | 12.41 ms | 805,792 msg/s | **6,295.25 MB/s** |
| IPC | 8,192 bytes | 10,000 | 49.71 ms | 201,169 msg/s | 1,571.63 MB/s |
| TCP | 65,536 bytes | 1,000 | 15.51 ms | 64,459 msg/s | 4,028.71 MB/s |
| inproc | 65,536 bytes | 1,000 | 6.92 ms | 144,593 msg/s | **9,037.04 MB/s** |
| IPC | 65,536 bytes | 1,000 | 13.72 ms | 72,894 msg/s | 4,555.89 MB/s |

### Fan-out (1 PUB → N SUB)

| Transport | Subscribers | Message Size | Total Msgs | Throughput | Bandwidth |
|-----------|-------------|--------------|------------|------------|-----------|
| TCP | 2 | 64 bytes | 20,000 | 6,873,840 msg/s | 419.55 MB/s |
| inproc | 2 | 64 bytes | 20,000 | 7,922,182 msg/s | 483.53 MB/s |
| TCP | 4 | 64 bytes | 40,000 | 7,894,048 msg/s | 481.81 MB/s |
| inproc | 4 | 64 bytes | 40,000 | **11,705,483 msg/s** | 714.45 MB/s |
| TCP | 8 | 64 bytes | 80,000 | 8,780,491 msg/s | 535.92 MB/s |
| inproc | 8 | 64 bytes | 80,000 | **11,057,433 msg/s** | 674.89 MB/s |

---

## 4. 핵심 지표 요약

### 최고 성능 기록

| 지표 | 값 | 조건 |
|------|-----|------|
| **최대 처리량** | 5,313,050 msg/s | IPC, 64B, PUB/SUB |
| **최대 대역폭** | 13,552.62 MB/s | inproc, 64KB, ROUTER |
| **최저 지연시간** | 25.20 μs (p50) | inproc, 64B |
| **최대 Fan-out** | 11,705,483 msg/s | inproc, 4 SUBs |

### 전송 계층별 비교 (64B 메시지 기준)

| 전송 | ROUTER 처리량 | PUB/SUB 처리량 | RTT (p50) |
|------|--------------|----------------|-----------|
| TCP | 4.67M msg/s | 5.10M msg/s | 60.61 μs |
| inproc | 4.24M msg/s | 5.05M msg/s | 25.20 μs |
| IPC | 4.80M msg/s | 5.31M msg/s | 58.80 μs |

---

## 5. C++20 포팅 후 비교 기준

이 문서는 C++20 포팅 후 성능 비교의 베이스라인으로 사용됩니다.

### 허용 범위
- Phase 1 (빌드 설정만 변경): ±1%
- Phase 2 (Concepts): ±2%
- Phase 3 (Ranges): ±3%
- 최종: ±5% 또는 성능 향상

### 성능 회귀 판단 기준
- 처리량 5% 이상 감소 → 회귀
- 지연시간 10% 이상 증가 → 회귀

---

**다음 측정**: C++20 빌드 설정 후 (`phase1_cpp20_build.md`)

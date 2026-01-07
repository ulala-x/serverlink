#!/bin/bash
# stress_router_cluster.sh: ROUTER 클러스터 가용성 및 복구 테스트

PORT=5566
ENDPOINT="tcp://127.0.0.1:$PORT"
NUM_PEERS=500
RUN_TIME=120 # 5분간 집중 테스트 (롱텀 시뮬레이션)

# 1. 중앙 서버 실행 (Background)
./build-asio/tests/benchmark/bench_latency $ENDPOINT 64 99999999 > server.log 2>&1 &
SERVER_PID=$!
sleep 2

echo "Server started (PID: $SERVER_PID). Connecting $NUM_PEERS peers..."

# 2. 자원 모니터링 시작
./scripts/monitor_resource.sh $SERVER_PID router_stability.csv &
MONITOR_PID=$!

# 3. 500개 피어 연결 (각 피어는 1초마다 하트비트 전송)
for i in $(seq 1 $NUM_PEERS); do
    ./build-asio/examples/test_simple $ENDPOINT 100 > /dev/null 2>&1 &
    PEER_PIDS[$i]=$!
done

echo "All peers connected. Starting churn simulation..."

# 4. Churn 시뮬레이션 (일부 피어 무작위 종료 후 재시작)
START_TIME=$(date +%s)
while [ $(($(date +%s) - START_TIME)) -lt $RUN_TIME ]; do
    # 50개 피어 랜덤 종료
    for j in $(seq 1 50); do
        IDX=$((RANDOM % NUM_PEERS + 1))
        KILL_PID=${PEER_PIDS[$IDX]}
        if kill -0 $KILL_PID 2>/dev/null; then
            kill -9 $KILL_PID
            # 즉시 재시작
            ./build-asio/examples/test_simple $ENDPOINT 100 > /dev/null 2>&1 &
            PEER_PIDS[$IDX]=$!
        fi
    done
    sleep 10
done

echo "Stress test finished. Cleaning up..."
kill -9 $SERVER_PID $MONITOR_PID
pkill -9 test_simple

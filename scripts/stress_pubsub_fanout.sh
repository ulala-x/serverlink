#!/bin/bash
# stress_pubsub_fanout.sh: PUB/SUB 대규모 팬아웃 및 토픽 스트레스 테스트

ENDPOINT="tcp://127.0.0.1:5577"
NUM_SUBS=1000
RUN_TIME=120

# 1. Publisher 실행
./build-asio/tests/benchmark/bench_pubsub $ENDPOINT 64 100000000 1 > pub.log 2>&1 &
PUB_PID=$!
sleep 2

# 2. 자원 모니터링 시작
./scripts/monitor_resource.sh $PUB_PID pubsub_stability.csv &
MONITOR_PID=$!

# 3. 1,000개 Subscriber 연결
for i in $(seq 1 $NUM_SUBS); do
    ./build-asio/examples/test_simple $ENDPOINT 100 > /dev/null 2>&1 &
done

echo "1,000 subscribers connected. Running for $RUN_TIME seconds..."
sleep $RUN_TIME

echo "Cleaning up..."
kill -9 $PUB_PID $MONITOR_PID
pkill -9 test_simple

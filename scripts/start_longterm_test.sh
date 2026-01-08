#!/bin/bash
# start_longterm_test.sh: Full automated stability test

NUM_WORKERS=50
LOG_DIR="stability_logs_$(date +%Y%m%d_%H%M%S)"
mkdir -p $LOG_DIR

echo "Starting ServerLink Long-Term Stability Test..."
echo "Workers: $NUM_WORKERS"
echo "Logs: $LOG_DIR"

# 1. Build and Start Cluster
docker compose up -d --scale worker=$NUM_WORKERS --build

# 2. Get Master Container ID and PID (on host)
MASTER_CID=$(docker compose ps -q master)
# Note: Finding host PID of container process can be tricky depending on Docker version
# We'll use docker stats instead for monitoring if possible, or just monitor inside the container.
# For simplicity, we'll monitor the container from outside using docker stats.

# 3. Start Resource Monitor (using docker stats)
echo "Timestamp,Container,CPU(%),Mem(%),Mem(Usage),Net(In/Out)" > $LOG_DIR/docker_stats.csv
(
    while true; do
        docker stats --no-stream --format "{{.Pids}},{{.Name}},{{.CPUPerc}},{{.MemPerc}},{{.MemUsage}},{{.NetIO}}" >> $LOG_DIR/docker_stats.csv
        sleep 10
    done
) &
MONITOR_PID=$!

# 4. Start Chaos Monkey
./scripts/chaos_monkey.sh $NUM_WORKERS >> $LOG_DIR/chaos.log 2>&1 &
CHAOS_PID=$!

echo "Test is running. Use 'docker compose logs -f' to see activity."
echo "Master PID: $MASTER_CID"
echo "Monitor PID: $MONITOR_PID"
echo "Chaos PID: $CHAOS_PID"
echo ""
echo "To stop the test, run: ./scripts/stop_longterm_test.sh $MONITOR_PID $CHAOS_PID"

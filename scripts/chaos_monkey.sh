#!/bin/bash
# chaos_monkey.sh: Randomly kill and restart worker containers

NUM_WORKERS=$1
INTERVAL=60

echo "Chaos monkey started with $NUM_WORKERS workers every $INTERVAL seconds."

while true; do
    # Pick a random worker number between 1 and NUM_WORKERS
    IDX=$((RANDOM % NUM_WORKERS + 1))
    CONTAINER="serverlink-worker-$IDX"
    
    echo "$(date +%H:%M:%S) [Chaos] Killing $CONTAINER..."
    docker stop $CONTAINER > /dev/null 2>&1
    
    sleep 5
    
    echo "$(date +%H:%M:%S) [Chaos] Restarting $CONTAINER..."
    docker start $CONTAINER > /dev/null 2>&1
    
    sleep $INTERVAL
done

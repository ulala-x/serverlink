#!/bin/bash
# stop_longterm_test.sh: Stop all testing components

MONITOR_PID=$1
CHAOS_PID=$2

echo "Stopping ServerLink Stability Test..."

if [ -n "$MONITOR_PID" ]; then
    kill $MONITOR_PID
fi

if [ -n "$CHAOS_PID" ]; then
    kill $CHAOS_PID
fi

docker compose down

echo "Cleanup complete. Check logs for results."

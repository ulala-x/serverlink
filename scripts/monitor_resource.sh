#!/bin/bash
# monitor_resource.sh: 실시간 자원 사용량 추적 스크립트

PID=$1
LOG_FILE=$2
echo "Timestamp,RSS(KB),CPU(%),FD_Count" > $LOG_FILE

while kill -0 $PID 2>/dev/null; do
    TIMESTAMP=$(date +%H:%M:%S)
    RSS=$(ps -o rss= -p $PID)
    CPU=$(ps -o %cpu= -p $PID)
    FD_COUNT=$(lsof -p $PID | wc -l)
    
    echo "$TIMESTAMP,$RSS,$CPU,$FD_COUNT" >> $LOG_FILE
    sleep 2
done

#!/bin/bash
set -e
BIN_DIR="./tests/benchmarkwithzmq/bin"
sizes=(64 256 1024 65536 131072 262144)
patterns=("pair PAIR" "dealer_dealer D-D" "dealer_router D-R" "router_router R-R" "pubsub PUB-SUB")

# 임시 데이터 저장소
THR_DATA=""
LAT_DATA=""

echo "Measuring performance matrix (this may take a minute)..." >&2

for pattern_info in "${patterns[@]}"; do
    read -r p name <<< "$pattern_info"
    for s in "${sizes[@]}"; do
        # Throughput (msg/s) - 2000 msgs for speed
        slk_thr=$($BIN_DIR/slk_$p inproc://thr_$p $s 2000 0)
        zmq_thr=$($BIN_DIR/zmq_$p inproc://thr_z_$p $s 2000 0)
        diff_thr=$(awk "BEGIN {printf \"%.1f\", ($slk_thr - $zmq_thr) / $zmq_thr * 100}")
        THR_DATA+="$name|${s}B|$slk_thr|$zmq_thr|$diff_thr\n"
        
        # Latency (us) - 200 samples
        slk_lat=$($BIN_DIR/slk_$p inproc://lat_$p $s 200 1)
        zmq_lat=$($BIN_DIR/zmq_$p inproc://lat_z_$p $s 200 1)
        diff_lat=$(awk "BEGIN {printf \"%.1f\", ($zmq_lat - $slk_lat) / $zmq_lat * 100}")
        LAT_DATA+="$name|${s}B|$slk_lat|$zmq_lat|$diff_lat\n"
    done
done

echo -e "\n=========================================================================="
echo "📊 [GROUP 1] THROUGHPUT COMPARISON (Higher is better)"
echo "=========================================================================="
printf "| %-12s | %-8s | %12s | %12s | %10s |\n" "Pattern" "Size" "ServerLink" "libzmq" "Diff (%)"
echo "|--------------|----------|--------------|--------------|------------|"
echo -e "$THR_DATA" | while IFS='|' read -r p s slk zmq d; do
    if [ -n "$p" ]; then
        printf "| %-12s | %-8s | %10.2fM | %10.2fM | %+9s%% |\n" "$p" "$s" $(bc -l <<< "$slk/1000000") $(bc -l <<< "$zmq/1000000") "$d"
    fi
done

echo -e "\n=========================================================================="
echo "⏱️ [GROUP 2] LATENCY COMPARISON (Lower is better)"
echo "=========================================================================="
printf "| %-12s | %-8s | %12s | %12s | %10s |\n" "Pattern" "Size" "ServerLink" "libzmq" "Diff (%)"
echo "|--------------|----------|--------------|--------------|------------|"
echo -e "$LAT_DATA" | while IFS='|' read -r p s slk zmq d; do
    if [ -n "$p" ]; then
        printf "| %-12s | %-8s | %10s us | %10s us | %+9s%% |\n" "$p" "$s" "$slk" "$zmq" "$d"
    fi
done
echo "=========================================================================="

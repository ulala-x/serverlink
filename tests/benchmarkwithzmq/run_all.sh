#!/bin/bash
set -e
BIN_DIR="./tests/benchmarkwithzmq/bin"
sizes=(64 1024 65536)
patterns=("pair Pair-to-Pair" "dealer_dealer Dealer-to-Dealer" "dealer_router Dealer-to-Router" "router_router Router-to-Router" "pubsub Pub-Sub")
transports=("tcp" "ipc" "inproc")

get_port() { python3 -c 'import socket; s=socket.socket(); s.bind(("", 0)); print(s.getsockname()[1]); s.close()'; }

# Throughput Report
echo -e "\n==========================================================="
echo -e "  [REPORT 1] THROUGHPUT COMPARISON (msg/s)"
echo -e "==========================================================="

for patt_info in "${patterns[@]}"; do
    read -r p name <<< "$patt_info"
    echo -e "\n## [PATTERN] $name (Throughput)"
    echo "| Transport | Size | ServerLink | libzmq | Diff |"
    echo "|:---|:---|:---|:---|:---|"
    for t in "${transports[@]}"; do
        for s in "${sizes[@]}"; do
            port=$(get_port); addr="$t://127.0.0.1:$port"
            if [ "$t" == "ipc" ]; then addr="ipc://bench_$p.ipc"; rm -f bench_$p.ipc; fi
            if [ "$t" == "inproc" ]; then addr="inproc://bench_$p"; fi
            
            s_val=$(timeout 5 $BIN_DIR/slk_$p "$addr" $s 5000 0 || echo "0")
            z_val=$(timeout 5 $BIN_DIR/zmq_$p "$addr" $s 5000 0 || echo "0")
            
            # 0 나누기 방지 및 N/A 처리
            if [[ "$z_val" == "0" || "$s_val" == "0" ]]; then
                printf "| %-9s | %8dB | %10s | %10s | %8s |\n" "$t" "$s" "TIMEOUT" "TIMEOUT" "N/A"
            else
                diff=$(awk "BEGIN {printf \"%.1f\", ($s_val - $z_val) / $z_val * 100}")
                printf "| %-9s | %8dB | %8.2fM | %8.2fM | %+6.1f%% |\n" "$t" "$s" $(bc -l <<< "$s_val/1000000") $(bc -l <<< "$z_val/1000000") "$diff"
            fi
        done
        echo "|-----------|----------|--------------|--------------|-------|"
    done
done

# Latency Report
echo -e "\n\n==========================================================="
echo -e "  [REPORT 2] LATENCY COMPARISON (RTT, μs)"
echo -e "==========================================================="

for patt_info in "${patterns[@]}"; do
    read -r p name <<< "$patt_info"
    echo -e "\n## [PATTERN] $name (Latency)"
    echo "| Transport | Size | ServerLink | libzmq | Diff |"
    echo "|:---|:---|:---|:---|:---|"
    for t in "${transports[@]}"; do
        for s in "${sizes[@]}"; do
            port=$(get_port); addr="$t://127.0.0.1:$port"
            if [ "$t" == "ipc" ]; then addr="ipc://lat_$p.ipc"; rm -f lat_$p.ipc; fi
            if [ "$t" == "inproc" ]; then addr="inproc://lat_$p"; fi
            
            s_val=$(timeout 5 $BIN_DIR/slk_$p "$addr" $s 500 1 || echo "0")
            z_val=$(timeout 5 $BIN_DIR/zmq_$p "$addr" $s 500 1 || echo "0")
            
            if [[ "$z_val" == "0" || "$s_val" == "0" ]]; then
                printf "| %-9s | %8dB | %10s | %10s | %8s |\n" "$t" "$s" "TIMEOUT" "TIMEOUT" "N/A"
            else
                diff=$(awk "BEGIN {printf \"%.1f\", ($z_val - $s_val) / $z_val * 100}")
                printf "| %-9s | %8dB | %10.2fus | %10.2fus | %+6.1f%% |\n" "$t" "$s" "$s_val" "$z_val" "$diff"
            fi
        done
        echo "|-----------|----------|--------------|--------------|-------|"
    done
done

#!/bin/bash
# High-Reliability Performance Comparison: ServerLink vs libzmq
set -e

# Configuration
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SUITE_DIR="$ROOT_DIR/tests/benchmark/comparison_suite"
LIBZMQ_DIR="$ROOT_DIR/../libzmq-ref"
BUILD_DIR="$ROOT_DIR/build-asio"
export LD_LIBRARY_PATH="$BUILD_DIR:$LIBZMQ_DIR/build/lib:$LD_LIBRARY_PATH"

# Sizes to test
SIZES=(64 256 1024 65536 262144)

# 1. Compile
echo "Compiling modular tests..."
FILES=("pair" "dealer_router" "router_router" "pubsub")
for name in "${FILES[@]}"; do
    g++ -O3 "$SUITE_DIR/${name}_slk.cpp" -I"$ROOT_DIR/include" -L"$BUILD_DIR" -lserverlink -lpthread -o "$SUITE_DIR/${name}_slk"
    g++ -O3 "$SUITE_DIR/${name}_zmq.cpp" -I"$LIBZMQ_DIR/include" -L"$LIBZMQ_DIR/build/lib" -lzmq -lpthread -o "$SUITE_DIR/${name}_zmq"
done

# Formatting helpers
fmt_thr() { echo "$1" | awk '{if($1>=1000000) printf "%.2fM", $1/1000000; else if($1>=1000) printf "%.1fK", $1/1000; else printf "%.0f", $1}'; }
fmt_lat() { printf "%.1f us" "$1"; }

run_scenario() {
    local name=$1; local mode=$2; # mode: 0=thr, 1=lat, 2=pubsub
    local mode_label="Throughput"; [ "$mode" -eq 1 ] && mode_label="Latency"
    
    echo ""
    echo "### $name ($mode_label)"
    echo "| Size | ServerLink | libzmq | Diff |"
    echo "|------|------------|--------|------|"

    for size in "${SIZES[@]}"; do
        size_str="${size}B"; [ "$size" -ge 1024 ] && size_str="$((size/1024))KB"
        
        if [ "$mode" -eq 2 ]; then
            slk_res=$(timeout --foreground 30s "$SUITE_DIR/${name}_slk" "$size" || echo "0")
            zmq_res=$(timeout --foreground 30s "$SUITE_DIR/${name}_zmq" "$size" || echo "0")
        else
            slk_res=$(timeout --foreground 30s "$SUITE_DIR/${name}_slk" "$size" "$mode" || echo "0")
            zmq_res=$(timeout --foreground 30s "$SUITE_DIR/${name}_zmq" "$size" "$mode" || echo "0")
        fi
        
        if [[ "$slk_res" == "0" || "$zmq_res" == "0" ]]; then
            echo "| $size_str | FAIL | FAIL | N/A |"
            continue
        fi

        if [ "$mode" -eq 1 ]; then
            diff=$(awk -v s="$slk_res" -v z="$zmq_res" 'BEGIN {if(s>0) printf "%.1f", (z-s)/s*100; else print "0.0"}')
            slk_fmt=$(fmt_lat "$slk_res"); zmq_fmt=$(fmt_lat "$zmq_res")
        else
            diff=$(awk -v s="$slk_res" -v z="$zmq_res" 'BEGIN {if(z>0) printf "%.1f", (s-z)/z*100; else print "0.0"}')
            slk_fmt=$(fmt_thr "$slk_res"); zmq_fmt=$(fmt_thr "$zmq_res")
        fi
        echo "| $size_str | $slk_fmt | $zmq_fmt | $diff% |"
    done
}

echo "============================================================================"
echo "           Total Modular Performance Comparison Report"
echo "============================================================================"

run_scenario "pair" 0
run_scenario "pair" 1
run_scenario "pubsub" 2
run_scenario "dealer_router" 0
run_scenario "dealer_router" 1
run_scenario "router_router" 0
run_scenario "router_router" 1

echo ""
echo "============================================================================"
echo "Report generation completed."

#!/bin/bash
# Strict Performance Comparison Suite
# Terminates immediately on any failure or timeout.

set -e

# Configuration
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SUITE_DIR="$ROOT_DIR/tests/benchmark/comparison_suite"
LIBZMQ_DIR="$ROOT_DIR/../libzmq-ref"
BUILD_DIR="$ROOT_DIR/build-asio"

export LD_LIBRARY_PATH="$BUILD_DIR:$LIBZMQ_DIR/build/lib:$LD_LIBRARY_PATH"

# Type codes
PAIR=0; PUB=1; SUB=2; DEALER=5; ROUTER=6
SIZES=(64 256 1024 65536 262144)

# 1. Compile
echo "Compiling benchmarks..."
g++ -O3 "$SUITE_DIR/slk_perf.cpp" -I"$ROOT_DIR/include" -L"$BUILD_DIR" -lserverlink -lpthread -o "$SUITE_DIR/slk_perf"
gcc -O3 "$SUITE_DIR/zmq_perf.cpp" -I"$LIBZMQ_DIR/include" -L"$LIBZMQ_DIR/build/lib" -lzmq -lpthread -o "$SUITE_DIR/zmq_perf"

# Formatting helpers
fmt_thr() { echo "$1" | awk '{if($1>=1000000) printf "%.2fM", $1/1000000; else if($1>=1000) printf "%.1fK", $1/1000; else printf "%.0f", $1}'; }
fmt_lat() { printf "%.1f us" "$1"; }

run_test() {
    local cmd=$1; local s=$2; local c=$3; local sz=$4; local m=$5
    # 20 second timeout. If timed out, this will return non-zero and 'set -e' will stop the script.
    local res=$(timeout --foreground 20s "$cmd" $s $c $sz $m)
    echo "$res"
}

run_comparison() {
    local s_type=$1; local c_type=$2; local name=$3; local mode=$4
    local mode_label="Throughput"; [ "$mode" -eq 1 ] && mode_label="Latency"

    echo ""
    echo "### $name ($mode_label)"
    echo "| Size | ServerLink | libzmq | Diff |"
    echo "|------|------------|--------|------|"

    for size in "${SIZES[@]}"; do
        size_str="${size}B"; [ "$size" -ge 1024 ] && size_str="$((size/1024))KB"
        
        # If run_test fails (timeout), the script will exit here due to 'set -e'
        slk_res=$(run_test "$SUITE_DIR/slk_perf" "$s_type" "$c_type" "$size" "$mode")
        zmq_res=$(run_test "$SUITE_DIR/zmq_perf" "$s_type" "$c_type" "$size" "$mode")
        
        if [ "$mode" -eq 0 ]; then
            diff=$(awk -v s="$slk_res" -v z="$zmq_res" 'BEGIN {if(z>0) printf "%.1f", (s-z)/z*100; else print "0.0"}')
            slk_fmt=$(fmt_thr "$slk_res"); zmq_fmt=$(fmt_thr "$zmq_res")
        else
            diff=$(awk -v s="$slk_res" -v z="$zmq_res" 'BEGIN {if(s>0) printf "%.1f", (z-s)/s*100; else print "0.0"}')
            slk_fmt=$(fmt_lat "$slk_res"); zmq_fmt=$(fmt_lat "$zmq_res")
        fi
        echo "| $size_str | $slk_fmt | $zmq_fmt | $diff% |"
    done
}

echo "============================================================================"
echo "           Total Performance Comparison Report (STRICT MODE)"
echo "============================================================================"

run_comparison $PAIR $PAIR "PAIR to PAIR" 0
run_comparison $PAIR $PAIR "PAIR to PAIR" 1
run_comparison $SUB $PUB "PUB to SUB" 0
run_comparison $ROUTER $DEALER "DEALER to ROUTER" 0
run_comparison $ROUTER $DEALER "DEALER to ROUTER" 1
run_comparison $ROUTER $ROUTER "ROUTER to ROUTER" 0
run_comparison $ROUTER $ROUTER "ROUTER to ROUTER" 1

echo ""
echo "============================================================================"
echo "Benchmark Completed Successfully."
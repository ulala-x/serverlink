#!/bin/bash
# Total Performance Suite: ServerLink vs libzmq

set -e

# Path setup
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIBZMQ_DIR="$ROOT_DIR/../libzmq-ref"
BUILD_DIR="$ROOT_DIR/build-asio"
export LD_LIBRARY_PATH="$BUILD_DIR:$LIBZMQ_DIR/build/lib:$LD_LIBRARY_PATH"

# Type constants
PAIR=0; PUB=1; SUB=2; DEALER=5; ROUTER=6
SIZES=(64 256 1024 65536 262144)

echo "Building benchmarks..."
cmake --build build-asio --parallel 8 --target bench_slk_perf > /dev/null
gcc -O3 tmp_bench/bench_zmq_perf.c -I$LIBZMQ_DIR/include -L$LIBZMQ_DIR/build/lib -lzmq -lpthread -o tmp_bench/zmq_perf

# Pretty formatting helpers
fmt_thr() {
    local n=$1
    if [ -z "$n" ]; then echo "FAIL"; return; fi
    echo "$n" | awk '{if($1>=1000000) printf "%.2fM", $1/1000000; else if($1>=1000) printf "%.1fK", $1/1000; else printf "%.0f", $1}'
}

fmt_lat() {
    local n=$1
    if [ -z "$n" ]; then echo "FAIL"; return; fi
    printf "%.1f us" "$n"
}

run_and_compare() {
    local s_type=$1; local c_type=$2; local name=$3; local mode=$4; # 0=thr, 1=lat
    
    local mode_name="Throughput"
    if [ $mode -eq 1 ]; then mode_name="Latency"; fi

    echo ""
    echo "## $name ($mode_name)"
    printf "| %-10s | %15s | %15s | %10s |\n" "Size" "ServerLink" "libzmq" "Diff"
    printf "|------------|-----------------|-----------------|------------|\n"

    for size in "${SIZES[@]}"; do
        size_str="${size}B"; if [ $size -ge 1024 ]; then size_str="$((size/1024))KB"; fi
        
        # Run tests
        slk_res=$(./build-asio/tests/bench_slk_perf $s_type $c_type $size $mode || echo "0")
        zmq_res=$(./tmp_bench/zmq_perf $s_type $c_type $size $mode || echo "0")
        
        # Calculate Diff
        if [ $mode -eq 0 ]; then # Thr: higher is better
            diff=$(awk -v s="$slk_res" -v z="$zmq_res" 'BEGIN {if(z>0) printf "%.1f", (s-z)/z*100; else print "0.0"}')
            slk_fmt="$(fmt_thr $slk_res)"; zmq_fmt="$(fmt_thr $zmq_res)"
        else # Lat: lower is better
            diff=$(awk -v s="$slk_res" -v z="$zmq_res" 'BEGIN {if(s>0) printf "%.1f", (z-s)/s*100; else print "0.0"}')
            slk_fmt="$(fmt_lat $slk_res)"; zmq_fmt="$(fmt_lat $zmq_res)"
        fi
        
        printf "| %10s | %13s | %13s | %9s%% |\n" "$size_str" "$slk_fmt" "$zmq_fmt" "$diff"
    done
    printf "|------------|-----------------|-----------------|------------|\n"
}

echo "=========================================================================================="
echo "           ServerLink vs libzmq: Multi-Dimension Comparison"
echo "=========================================================================================="

run_and_compare $PAIR $PAIR "PAIR to PAIR" 0
run_and_compare $PAIR $PAIR "PAIR to PAIR" 1
run_and_compare $SUB $PUB "PUB to SUB" 0
run_and_compare $ROUTER $DEALER "DEALER to ROUTER" 0
run_and_compare $ROUTER $DEALER "DEALER to ROUTER" 1
run_and_compare $ROUTER $ROUTER "ROUTER to ROUTER" 0
run_and_compare $ROUTER $ROUTER "ROUTER to ROUTER" 1

echo ""
echo "=========================================================================================="
echo "Benchmark Completed."

#!/bin/bash
# ServerLink vs libzmq Performance Comparison
# Usage: ./run_comparison.sh [router|pubsub|all]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../../build/tests/benchmark"

# libzmq paths
LIBZMQ_INCLUDE="/home/ulalax/project/ulalax/libzmq-native/deps/linux-x64/zeromq-4.3.5/include"
LIBZMQ_LIB="/home/ulalax/project/ulalax/libzmq-native/dist/linux-x64"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Temp files
TEMP_DIR=$(mktemp -d)
trap "rm -rf ${TEMP_DIR}" EXIT

SL_ROUTER="${TEMP_DIR}/sl_router.txt"
SL_PUBSUB="${TEMP_DIR}/sl_pubsub.txt"
ZMQ_ROUTER="${TEMP_DIR}/zmq_router.txt"
ZMQ_PUBSUB="${TEMP_DIR}/zmq_pubsub.txt"

# Compile libzmq benchmarks if needed
compile_zmq() {
    local name=$1
    local src="${SCRIPT_DIR}/${name}.cpp"
    local bin="${BUILD_DIR}/${name}"

    if [ ! -f "${src}" ]; then
        return 1
    fi

    if [ ! -f "${bin}" ] || [ "${src}" -nt "${bin}" ]; then
        echo -e "${YELLOW}Compiling ${name}...${NC}"
        g++ -O3 -std=c++20 -o "${bin}" "${src}" \
            -L"${LIBZMQ_LIB}" -lzmq \
            -Wl,-rpath,"${LIBZMQ_LIB}" \
            -lpthread
    fi
    return 0
}

# Parse benchmark output: extract "transport size throughput" lines
parse_output() {
    grep -E "^(TCP|inproc|IPC|PUB/SUB)" | \
    sed 's/PUB\/SUB //' | \
    awk -F'|' '{
        gsub(/^[ \t]+|[ \t]+$/, "", $1);  # transport
        gsub(/[^0-9]/, "", $2);            # size (bytes only)
        gsub(/[^0-9]/, "", $5);            # throughput (number only)
        if ($5 != "") print $1, $2, $5
    }'
}

# Print comparison table
print_table() {
    local pattern=$1
    local sl_file=$2
    local zmq_file=$3

    echo ""
    echo -e "${CYAN}## ${pattern} Comparison${NC}"
    echo ""
    printf "| %-8s | %-8s | %12s | %12s | %10s |\n" "Transport" "Size" "ServerLink" "libzmq" "Diff"
    printf "|----------|----------|--------------|--------------|------------|\n"

    # Read both files and match by transport+size
    while read -r transport size sl_thr; do
        zmq_thr=$(grep "^${transport} ${size} " "${zmq_file}" 2>/dev/null | awk '{print $3}')

        if [ -n "${zmq_thr}" ] && [ "${zmq_thr}" -gt 0 ]; then
            # Calculate difference percentage
            diff=$(awk "BEGIN {printf \"%.1f\", (${sl_thr} - ${zmq_thr}) / ${zmq_thr} * 100}")

            # Format numbers with K/M suffix
            sl_fmt=$(awk "BEGIN {
                if (${sl_thr} >= 1000000) printf \"%.2fM\", ${sl_thr}/1000000
                else if (${sl_thr} >= 1000) printf \"%.0fK\", ${sl_thr}/1000
                else printf \"%d\", ${sl_thr}
            }")
            zmq_fmt=$(awk "BEGIN {
                if (${zmq_thr} >= 1000000) printf \"%.2fM\", ${zmq_thr}/1000000
                else if (${zmq_thr} >= 1000) printf \"%.0fK\", ${zmq_thr}/1000
                else printf \"%d\", ${zmq_thr}
            }")

            # Size formatting
            size_fmt="${size}B"
            if [ "${size}" -ge 1024 ]; then
                size_fmt="$((size / 1024))KB"
            fi

            # Color for diff
            if (( $(echo "${diff} > 5" | bc -l) )); then
                diff_color="${GREEN}"
            elif (( $(echo "${diff} < -5" | bc -l) )); then
                diff_color="${RED}"
            else
                diff_color="${NC}"
            fi

            printf "| %-8s | %8s | %10s/s | %10s/s | ${diff_color}%+9.1f%%${NC} |\n" \
                "${transport}" "${size_fmt}" "${sl_fmt}" "${zmq_fmt}" "${diff}"
        fi
    done < "${sl_file}"
}

# Main
echo -e "${BLUE}=======================================${NC}"
echo -e "${BLUE}  ServerLink vs libzmq Comparison${NC}"
echo -e "${BLUE}=======================================${NC}"
echo ""

MODE="${1:-all}"

run_router_comparison() {
    echo -e "${YELLOW}Running ROUTER-ROUTER benchmarks...${NC}"

    # Run ServerLink
    if [ -x "${BUILD_DIR}/bench_throughput" ]; then
        "${BUILD_DIR}/bench_throughput" 2>/dev/null | parse_output > "${SL_ROUTER}"
    else
        echo "ERROR: bench_throughput not found"
        return 1
    fi

    # Run libzmq
    if compile_zmq "bench_zmq_router"; then
        "${BUILD_DIR}/bench_zmq_router" 2>/dev/null | parse_output > "${ZMQ_ROUTER}"
    else
        echo "ERROR: bench_zmq_router compilation failed"
        return 1
    fi

    print_table "ROUTER-ROUTER" "${SL_ROUTER}" "${ZMQ_ROUTER}"
}

run_pubsub_comparison() {
    echo -e "${YELLOW}Running PUB-SUB benchmarks...${NC}"

    # Run ServerLink
    if [ -x "${BUILD_DIR}/bench_pubsub" ]; then
        "${BUILD_DIR}/bench_pubsub" 2>/dev/null | parse_output > "${SL_PUBSUB}"
    else
        echo "ERROR: bench_pubsub not found"
        return 1
    fi

    # Run libzmq
    if compile_zmq "bench_zmq_pubsub"; then
        "${BUILD_DIR}/bench_zmq_pubsub" 2>/dev/null | parse_output > "${ZMQ_PUBSUB}"
    else
        echo "ERROR: bench_zmq_pubsub compilation failed"
        return 1
    fi

    print_table "PUB-SUB" "${SL_PUBSUB}" "${ZMQ_PUBSUB}"
}

case "${MODE}" in
    router)
        run_router_comparison
        ;;
    pubsub)
        run_pubsub_comparison
        ;;
    all)
        run_router_comparison
        echo ""
        run_pubsub_comparison
        ;;
    *)
        echo "Usage: $0 [router|pubsub|all]"
        exit 1
        ;;
esac

echo ""
echo -e "${GREEN}Legend:${NC} Green = ServerLink faster, Red = libzmq faster"
echo ""

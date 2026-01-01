#!/bin/bash
# Performance Comparison: ServerLink vs libzmq
# ROUTER-to-ROUTER pattern benchmark

set -e

SERVERLINK_DIR="/home/ulalax/project/ulalax/serverlink"
LIBZMQ_DIR="/home/ulalax/project/ulalax/libzmq-ref"

SERVERLINK_THR="${SERVERLINK_DIR}/build/tests/benchmark/bench_throughput"
LIBZMQ_THR="${LIBZMQ_DIR}/perf/router_bench/build/router_throughput"

echo "=============================================="
echo "  Performance Comparison: ServerLink vs libzmq"
echo "  Pattern: ROUTER-to-ROUTER"
echo "=============================================="
echo ""

# Test configurations
MESSAGE_SIZES=(64 256 1024 4096)
MESSAGE_COUNTS=(100000 100000 50000 10000)

# Create results directory
RESULTS_DIR="${SERVERLINK_DIR}/benchmark_results"
mkdir -p "${RESULTS_DIR}"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_FILE="${RESULTS_DIR}/comparison_${TIMESTAMP}.txt"

{
    echo "Performance Comparison Results"
    echo "Date: $(date)"
    echo "=============================================="
    echo ""

    for i in "${!MESSAGE_SIZES[@]}"; do
        MSG_SIZE="${MESSAGE_SIZES[$i]}"
        MSG_COUNT="${MESSAGE_COUNTS[$i]}"

        echo "=============================================="
        echo "Test Configuration: ${MSG_SIZE} bytes, ${MSG_COUNT} messages"
        echo "=============================================="
        echo ""

        # TCP Transport
        echo "---------- TCP Transport ----------"
        echo ""
        echo "--- ServerLink ROUTER-ROUTER TCP ---"
        timeout 60 "${SERVERLINK_THR}" 2>&1 | grep -A1 "TCP.*${MSG_SIZE} bytes" || echo "ServerLink TCP test skipped"
        echo ""

        echo "--- libzmq ROUTER-ROUTER TCP ---"
        timeout 60 "${LIBZMQ_THR}" tcp "${MSG_SIZE}" "${MSG_COUNT}" 2>&1 || echo "libzmq TCP test failed"
        echo ""

        # inproc Transport
        echo "---------- inproc Transport ----------"
        echo ""
        echo "--- ServerLink ROUTER-ROUTER inproc ---"
        timeout 60 "${SERVERLINK_THR}" 2>&1 | grep -A1 "inproc.*${MSG_SIZE} bytes" || echo "ServerLink inproc test skipped"
        echo ""

        echo "--- libzmq ROUTER-ROUTER inproc ---"
        timeout 60 "${LIBZMQ_THR}" inproc "${MSG_SIZE}" "${MSG_COUNT}" 2>&1 || echo "libzmq inproc test failed"
        echo ""

        # IPC Transport (Linux only)
        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            echo "---------- IPC Transport ----------"
            echo ""
            echo "--- ServerLink ROUTER-ROUTER IPC ---"
            timeout 60 "${SERVERLINK_THR}" 2>&1 | grep -A1 "IPC.*${MSG_SIZE} bytes" || echo "ServerLink IPC test skipped"
            echo ""

            echo "--- libzmq ROUTER-ROUTER IPC ---"
            timeout 60 "${LIBZMQ_THR}" ipc "${MSG_SIZE}" "${MSG_COUNT}" 2>&1 || echo "libzmq IPC test failed"
            echo ""
        fi

        echo ""
    done

} | tee "${RESULTS_FILE}"

echo "=============================================="
echo "Results saved to: ${RESULTS_FILE}"
echo "=============================================="

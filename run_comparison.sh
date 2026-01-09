#!/bin/bash
# Comprehensive Performance Comparison: ServerLink vs libzmq
# ROUTER-to-ROUTER pattern benchmark

set -e

SERVERLINK_DIR="/home/ulalax/project/ulalax/serverlink"
LIBZMQ_DIR="/home/ulalax/project/ulalax/libzmq-ref"

export LD_LIBRARY_PATH="${LIBZMQ_DIR}/build/lib:${SERVERLINK_DIR}/build-asio:${LD_LIBRARY_PATH}"

LIBZMQ_THR="${LIBZMQ_DIR}/perf/router_bench/build/router_throughput"
LIBZMQ_LAT="${LIBZMQ_DIR}/perf/router_bench/build/router_latency"

echo ""
echo "============================================================================"
echo "           Performance Comparison: ServerLink vs libzmq v4.3.5"
echo "                      Pattern: ROUTER-to-ROUTER"
echo "============================================================================"
echo ""

# Create results directory
RESULTS_DIR="${SERVERLINK_DIR}/benchmark_results"
mkdir -p "${RESULTS_DIR}"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_FILE="${RESULTS_DIR}/comparison_${TIMESTAMP}.txt"

{
    echo "Performance Comparison Results"
    echo "Date: $(date)"
    echo "System: $(uname -a)"
    echo "GCC: $(gcc --version | head -1)"
    echo ""
    echo "============================================================================"
    echo ""

    #
    # THROUGHPUT BENCHMARKS
    #
    echo "############################################################################"
    echo "#                         THROUGHPUT BENCHMARKS                            #"
    echo "############################################################################"
    echo ""

    # Test configurations matching ServerLink defaults
    declare -A TESTS=(
        ["64_100000"]="64 100000"
        ["256_100000"]="256 100000"
        ["1024_50000"]="1024 50000"
        ["4096_10000"]="4096 10000"
    )

    for test_key in "64_100000" "256_100000" "1024_50000" "4096_10000"; do
        read -r MSG_SIZE MSG_COUNT <<< "${TESTS[$test_key]}"

        echo "============================================================================"
        echo "  Throughput Test: ${MSG_SIZE} bytes × ${MSG_COUNT} messages"
        echo "============================================================================"
        echo ""

        # TCP Transport
        echo "─────────────────────── TCP Transport ───────────────────────"
        echo ""
        echo "[libzmq]"
        timeout 60 "${LIBZMQ_THR}" tcp "${MSG_SIZE}" "${MSG_COUNT}" 2>&1 || echo "FAILED"
        echo ""

        # inproc Transport
        echo "─────────────────────── inproc Transport ────────────────────"
        echo ""
        echo "[libzmq]"
        timeout 60 "${LIBZMQ_THR}" inproc "${MSG_SIZE}" "${MSG_COUNT}" 2>&1 || echo "FAILED"
        echo ""

        # IPC Transport (Linux only)
        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            echo "─────────────────────── IPC Transport ───────────────────────"
            echo ""
            echo "[libzmq]"
            timeout 60 "${LIBZMQ_THR}" ipc "${MSG_SIZE}" "${MSG_COUNT}" 2>&1 || echo "FAILED"
            echo ""
        fi

        echo ""
    done

    # Run ServerLink throughput benchmark
    echo "============================================================================"
    echo "  ServerLink Full Throughput Benchmark"
    echo "============================================================================"
    echo ""
    cd "${SERVERLINK_DIR}/build-asio"
    ./tests/benchmark/bench_throughput 2>&1
    echo ""

    #
    # LATENCY BENCHMARKS
    #
    echo ""
    echo "############################################################################"
    echo "#                          LATENCY BENCHMARKS                              #"
    echo "############################################################################"
    echo ""

    # Latency test configurations
    declare -A LAT_TESTS=(
        ["64_10000"]="64 10000"
        ["256_10000"]="256 10000"
        ["1024_10000"]="1024 10000"
        ["4096_1000"]="4096 1000"
    )

    for test_key in "64_10000" "256_10000" "1024_10000" "4096_1000"; do
        read -r MSG_SIZE ROUNDTRIPS <<< "${LAT_TESTS[$test_key]}"

        echo "============================================================================"
        echo "  Latency Test: ${MSG_SIZE} bytes × ${ROUNDTRIPS} roundtrips"
        echo "============================================================================"
        echo ""

        # TCP Transport
        echo "─────────────────────── TCP Transport ───────────────────────"
        echo ""
        echo "[libzmq]"
        timeout 60 "${LIBZMQ_LAT}" tcp "${MSG_SIZE}" "${ROUNDTRIPS}" 2>&1 || echo "FAILED"
        echo ""

        # inproc Transport
        echo "─────────────────────── inproc Transport ────────────────────"
        echo ""
        echo "[libzmq]"
        timeout 60 "${LIBZMQ_LAT}" inproc "${MSG_SIZE}" "${ROUNDTRIPS}" 2>&1 || echo "FAILED"
        echo ""

        # IPC Transport (Linux only)
        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            echo "─────────────────────── IPC Transport ───────────────────────"
            echo ""
            echo "[libzmq]"
            timeout 60 "${LIBZMQ_LAT}" ipc "${MSG_SIZE}" "${ROUNDTRIPS}" 2>&1 || echo "FAILED"
            echo ""
        fi

        echo ""
    done

    # Run ServerLink latency benchmark
    echo "============================================================================"
    echo "  ServerLink Full Latency Benchmark"
    echo "============================================================================"
    echo ""
    cd "${SERVERLINK_DIR}/build-asio"
    ./tests/benchmark/bench_latency 2>&1
    echo ""

    echo "============================================================================"
    echo "                         BENCHMARK COMPLETED"
    echo "============================================================================"
    echo ""

} 2>&1 | tee "${RESULTS_FILE}"

echo ""
echo "Results saved to: ${RESULTS_FILE}"
echo ""

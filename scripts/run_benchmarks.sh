#!/bin/bash
set -euo pipefail

# ServerLink Benchmark Runner Script (Linux/macOS)
# Runs all benchmarks and outputs results in JSON format

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Default values
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build}"
OUTPUT_FILE="${OUTPUT_FILE:-benchmark_results.json}"

echo "============================================"
echo "ServerLink Benchmark Suite"
echo "============================================"
echo "Build Directory: ${BUILD_DIR}"
echo "Output File:     ${OUTPUT_FILE}"
echo "============================================"
echo ""

# Check if build directory exists
if [ ! -d "${BUILD_DIR}" ]; then
    echo "ERROR: Build directory not found: ${BUILD_DIR}"
    echo "Please build the project first or set BUILD_DIR environment variable"
    exit 1
fi

# Find benchmark executables
BENCHMARK_DIR="${BUILD_DIR}/tests/benchmark"

if [ ! -d "${BENCHMARK_DIR}" ]; then
    echo "ERROR: Benchmark directory not found: ${BENCHMARK_DIR}"
    echo "Please build with -DBUILD_TESTS=ON"
    exit 1
fi

# Find all benchmark executables
BENCHMARKS=$(find "${BENCHMARK_DIR}" -name "bench_*" -type f -executable 2>/dev/null || true)

if [ -z "${BENCHMARKS}" ]; then
    echo "ERROR: No benchmark executables found in ${BENCHMARK_DIR}"
    exit 1
fi

echo "Found benchmarks:"
echo "${BENCHMARKS}" | while read -r bench; do
    echo "  - $(basename ${bench})"
done
echo ""

# Create temporary directory for individual results
TEMP_DIR=$(mktemp -d)
trap "rm -rf ${TEMP_DIR}" EXIT

# Run each benchmark
BENCH_COUNT=0
for BENCH in ${BENCHMARKS}; do
    BENCH_NAME=$(basename "${BENCH}")
    echo "Running ${BENCH_NAME}..."

    # Run benchmark and capture output
    OUTPUT_FILE_TMP="${TEMP_DIR}/${BENCH_NAME}.txt"

    if "${BENCH}" > "${OUTPUT_FILE_TMP}" 2>&1; then
        echo "  ✓ ${BENCH_NAME} completed"
        BENCH_COUNT=$((BENCH_COUNT + 1))
    else
        echo "  ✗ ${BENCH_NAME} failed"
        cat "${OUTPUT_FILE_TMP}"
    fi

    echo ""
done

echo "============================================"
echo "Benchmark Results"
echo "============================================"
echo "Completed: ${BENCH_COUNT} benchmarks"
echo ""

# Combine results into JSON format using Python formatter
if [ -f "${SCRIPT_DIR}/format_benchmark.py" ]; then
    echo "Formatting results to JSON..."
    python3 "${SCRIPT_DIR}/format_benchmark.py" "${TEMP_DIR}" "${OUTPUT_FILE}"

    if [ -f "${OUTPUT_FILE}" ]; then
        echo "Results written to: ${OUTPUT_FILE}"
        echo ""

        # Show summary
        if command -v jq &> /dev/null; then
            echo "Summary:"
            jq -r '.benchmarks[] | "  \(.name): \(.throughput_msg_per_sec // .latency_us // "N/A") \(.unit // "")"' "${OUTPUT_FILE}"
        else
            echo "Install 'jq' to see formatted summary"
        fi
    fi
else
    echo "WARNING: format_benchmark.py not found, skipping JSON formatting"
    echo "Raw results are in: ${TEMP_DIR}"
fi

echo ""
echo "✓ Benchmark suite completed"
exit 0

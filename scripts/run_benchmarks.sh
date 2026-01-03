#!/bin/bash
set -euo pipefail

# ServerLink Benchmark Runner Script (Linux/macOS)
# Runs all benchmarks and outputs results in JSON format

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Default values
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build}"
OUTPUT_FILE="${OUTPUT_FILE:-benchmark_results.json}"
PLATFORM="${PLATFORM:-}"

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

# Collect system information
echo "Collecting system information..."
SYSINFO_FILE="${TEMP_DIR}/sysinfo.json"

# Get OS info
if [ -f /etc/os-release ]; then
    OS_NAME=$(grep '^PRETTY_NAME=' /etc/os-release | cut -d'"' -f2)
elif [ "$(uname)" == "Darwin" ]; then
    OS_NAME="macOS $(sw_vers -productVersion)"
else
    OS_NAME=$(uname -s)
fi

# Get CPU info
if [ "$(uname)" == "Darwin" ]; then
    CPU_MODEL=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "Apple Silicon")
    CPU_CORES=$(sysctl -n hw.ncpu)
    MEMORY_GB=$(( $(sysctl -n hw.memsize) / 1024 / 1024 / 1024 ))
else
    CPU_MODEL=$(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d':' -f2 | xargs || echo "Unknown")
    CPU_CORES=$(nproc 2>/dev/null || echo "Unknown")
    MEMORY_GB=$(( $(grep MemTotal /proc/meminfo 2>/dev/null | awk '{print $2}') / 1024 / 1024 ))
fi

# Get architecture
ARCH=$(uname -m)

# Write system info to JSON
cat > "${SYSINFO_FILE}" << EOF
{
  "os": "${OS_NAME}",
  "arch": "${ARCH}",
  "cpu": "${CPU_MODEL}",
  "cores": ${CPU_CORES:-0},
  "memory_gb": ${MEMORY_GB:-0},
  "kernel": "$(uname -r)"
}
EOF

echo "System Info:"
cat "${SYSINFO_FILE}"
echo ""

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
    PLATFORM_ARG=""
    if [ -n "${PLATFORM}" ]; then
        PLATFORM_ARG="--platform ${PLATFORM}"
    fi
    python3 "${SCRIPT_DIR}/format_benchmark.py" "${TEMP_DIR}" "${OUTPUT_FILE}" ${PLATFORM_ARG}

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

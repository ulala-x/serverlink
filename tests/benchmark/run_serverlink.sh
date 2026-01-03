#!/bin/bash
# ServerLink Benchmark Runner
# Usage: ./run_serverlink.sh [router|pubsub|all]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../../build/tests/benchmark"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== ServerLink Benchmark ===${NC}"
echo ""

# Check build directory
if [ ! -d "${BUILD_DIR}" ]; then
    echo "ERROR: Build directory not found: ${BUILD_DIR}"
    echo "Please build the project first:"
    echo "  cmake -B build -S . -DBUILD_TESTS=ON"
    echo "  cmake --build build"
    exit 1
fi

MODE="${1:-all}"

run_router() {
    if [ -x "${BUILD_DIR}/bench_throughput" ]; then
        echo -e "${GREEN}[ROUTER-ROUTER Throughput]${NC}"
        "${BUILD_DIR}/bench_throughput"
    else
        echo "bench_throughput not found"
    fi
}

run_pubsub() {
    if [ -x "${BUILD_DIR}/bench_pubsub" ]; then
        echo -e "${GREEN}[PUB-SUB Throughput]${NC}"
        "${BUILD_DIR}/bench_pubsub"
    else
        echo "bench_pubsub not found"
    fi
}

run_latency() {
    if [ -x "${BUILD_DIR}/bench_latency" ]; then
        echo -e "${GREEN}[Latency]${NC}"
        "${BUILD_DIR}/bench_latency"
    else
        echo "bench_latency not found"
    fi
}

case "${MODE}" in
    router)
        run_router
        ;;
    pubsub)
        run_pubsub
        ;;
    latency)
        run_latency
        ;;
    all)
        run_router
        echo ""
        run_pubsub
        echo ""
        run_latency
        ;;
    *)
        echo "Usage: $0 [router|pubsub|latency|all]"
        exit 1
        ;;
esac

echo ""
echo "Done."

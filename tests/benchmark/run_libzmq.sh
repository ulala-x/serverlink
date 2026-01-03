#!/bin/bash
# libzmq Benchmark Runner
# Usage: ./run_libzmq.sh [router|pubsub|all]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../../build/tests/benchmark"

# libzmq paths
LIBZMQ_INCLUDE="/home/ulalax/project/ulalax/libzmq-native/deps/linux-x64/zeromq-4.3.5/include"
LIBZMQ_LIB="/home/ulalax/project/ulalax/libzmq-native/dist/linux-x64"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}=== libzmq Benchmark ===${NC}"
echo ""

# Check libzmq
if [ ! -f "${LIBZMQ_INCLUDE}/zmq.h" ]; then
    echo "ERROR: libzmq headers not found at ${LIBZMQ_INCLUDE}"
    exit 1
fi

if [ ! -f "${LIBZMQ_LIB}/libzmq.so" ]; then
    echo "ERROR: libzmq library not found at ${LIBZMQ_LIB}"
    exit 1
fi

# Compile function
compile_if_needed() {
    local name=$1
    local src="${SCRIPT_DIR}/${name}.cpp"
    local bin="${BUILD_DIR}/${name}"

    if [ ! -f "${src}" ]; then
        echo "Source not found: ${src}"
        return 1
    fi

    # Recompile if source is newer than binary
    if [ ! -f "${bin}" ] || [ "${src}" -nt "${bin}" ]; then
        echo -e "${YELLOW}Compiling ${name}...${NC}"
        g++ -O3 -std=c++20 -o "${bin}" "${src}" \
            -L"${LIBZMQ_LIB}" -lzmq \
            -Wl,-rpath,"${LIBZMQ_LIB}" \
            -lpthread
    fi

    return 0
}

MODE="${1:-all}"

run_router() {
    if compile_if_needed "bench_zmq_router"; then
        echo -e "${GREEN}[ROUTER-ROUTER Throughput]${NC}"
        "${BUILD_DIR}/bench_zmq_router"
    fi
}

run_pubsub() {
    if compile_if_needed "bench_zmq_pubsub"; then
        echo -e "${GREEN}[PUB-SUB Throughput]${NC}"
        "${BUILD_DIR}/bench_zmq_pubsub"
    fi
}

case "${MODE}" in
    router)
        run_router
        ;;
    pubsub)
        run_pubsub
        ;;
    all)
        run_router
        echo ""
        run_pubsub
        ;;
    *)
        echo "Usage: $0 [router|pubsub|all]"
        exit 1
        ;;
esac

echo ""
echo "Done."

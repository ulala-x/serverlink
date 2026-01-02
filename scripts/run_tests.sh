#!/bin/bash
set -euo pipefail

# ServerLink Test Runner Script (Linux/macOS)
# Runs all CTest tests with proper output formatting

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Default values
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build}"
TEST_TIMEOUT="${TEST_TIMEOUT:-300}"
VERBOSE="${VERBOSE:-0}"

echo "============================================"
echo "ServerLink Test Suite"
echo "============================================"
echo "Build Directory: ${BUILD_DIR}"
echo "Test Timeout:    ${TEST_TIMEOUT}s"
echo "============================================"
echo ""

# Check if build directory exists
if [ ! -d "${BUILD_DIR}" ]; then
    echo "ERROR: Build directory not found: ${BUILD_DIR}"
    echo "Please build the project first or set BUILD_DIR environment variable"
    exit 1
fi

# Change to build directory
cd "${BUILD_DIR}"

# Check if tests were built
if [ ! -f "CTestTestfile.cmake" ]; then
    echo "ERROR: No tests found in build directory"
    echo "Please build with -DBUILD_TESTS=ON"
    exit 1
fi

# Run CTest
echo "Running tests..."
echo ""

CTEST_ARGS=("--output-on-failure" "--timeout" "${TEST_TIMEOUT}")

if [ "${VERBOSE}" = "1" ]; then
    CTEST_ARGS+=("--verbose")
fi

# Run all tests
if ctest "${CTEST_ARGS[@]}"; then
    echo ""
    echo "============================================"
    echo "✓ All tests PASSED"
    echo "============================================"
    echo ""

    # Show test summary
    echo "Test summary:"
    ctest -N | grep "^  Test" | wc -l | xargs -I {} echo "Total tests: {}"
    echo ""

    exit 0
else
    EXIT_CODE=$?
    echo ""
    echo "============================================"
    echo "✗ Some tests FAILED"
    echo "============================================"
    echo ""

    # Show failed tests
    echo "Failed tests:"
    ctest --rerun-failed --output-on-failure || true
    echo ""

    exit ${EXIT_CODE}
fi

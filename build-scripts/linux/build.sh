#!/bin/bash
set -euo pipefail

# ServerLink Linux Build Script
# Builds ServerLink for Linux (x64 or ARM64)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Default values
ARCH="${1:-x64}"
BUILD_TYPE="${2:-Release}"
BUILD_SHARED="${3:-ON}"

echo "============================================"
echo "ServerLink Linux Build"
echo "============================================"
echo "Architecture: ${ARCH}"
echo "Build Type:   ${BUILD_TYPE}"
echo "Shared Libs:  ${BUILD_SHARED}"
echo "============================================"
echo ""

# Read version from VERSION file
if [ -f "${PROJECT_ROOT}/VERSION" ]; then
    source "${PROJECT_ROOT}/VERSION"
    echo "Version: ${SERVERLINK_VERSION}"
fi

# Determine build directory and install prefix
BUILD_DIR="${PROJECT_ROOT}/build-${ARCH}"
DIST_DIR="${PROJECT_ROOT}/dist/linux-${ARCH}"

# Clean previous build if requested
if [ "${CLEAN_BUILD:-0}" = "1" ]; then
    echo "Cleaning previous build..."
    rm -rf "${BUILD_DIR}"
    rm -rf "${DIST_DIR}"
fi

# Create directories
mkdir -p "${BUILD_DIR}"
mkdir -p "${DIST_DIR}"

# Configure CMake
echo "Configuring CMake..."
cd "${BUILD_DIR}"

cmake "${PROJECT_ROOT}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DBUILD_SHARED_LIBS="${BUILD_SHARED}" \
    -DBUILD_TESTS=ON \
    -DBUILD_EXAMPLES=ON \
    -DCMAKE_INSTALL_PREFIX="${DIST_DIR}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
echo ""
echo "Building ServerLink..."
cmake --build . --config "${BUILD_TYPE}" --parallel $(nproc)

# Install to dist directory
echo ""
echo "Installing to ${DIST_DIR}..."
cmake --build . --target install

# Copy additional files
echo ""
echo "Copying additional files..."
cp -f "${PROJECT_ROOT}/LICENSE" "${DIST_DIR}/"
cp -f "${PROJECT_ROOT}/README.md" "${DIST_DIR}/"

if [ -f "${BUILD_DIR}/compile_commands.json" ]; then
    cp -f "${BUILD_DIR}/compile_commands.json" "${DIST_DIR}/"
fi

# Copy test binaries for distribution
if [ -d "${BUILD_DIR}/tests" ]; then
    mkdir -p "${DIST_DIR}/tests"
    find "${BUILD_DIR}/tests" -type f -executable -exec cp {} "${DIST_DIR}/tests/" \; 2>/dev/null || true
fi

# Copy benchmark binaries
if [ -d "${BUILD_DIR}/tests/benchmark" ]; then
    mkdir -p "${DIST_DIR}/benchmarks"
    find "${BUILD_DIR}/tests/benchmark" -name "bench_*" -type f -executable -exec cp {} "${DIST_DIR}/benchmarks/" \; 2>/dev/null || true
fi

# Print library info
echo ""
echo "============================================"
echo "Build completed successfully!"
echo "============================================"
echo "Output directory: ${DIST_DIR}"
echo ""

if [ "${BUILD_SHARED}" = "ON" ]; then
    LIBFILE="${DIST_DIR}/lib/libserverlink.so"
    if [ -f "${LIBFILE}" ]; then
        echo "Shared library: ${LIBFILE}"
        file "${LIBFILE}"
        echo ""
        echo "Library dependencies:"
        ldd "${LIBFILE}" || true
    fi
else
    LIBFILE="${DIST_DIR}/lib/libserverlink.a"
    if [ -f "${LIBFILE}" ]; then
        echo "Static library: ${LIBFILE}"
        file "${LIBFILE}"
        echo ""
        ar -t "${LIBFILE}" | head -20
    fi
fi

echo ""
echo "Directory structure:"
ls -lh "${DIST_DIR}/"
echo ""

# Generate build info
cat > "${DIST_DIR}/BUILD_INFO.txt" <<EOF
ServerLink Build Information
=============================

Version:        ${SERVERLINK_VERSION:-unknown}
Build Date:     $(date -u +"%Y-%m-%d %H:%M:%S UTC")
Platform:       Linux ${ARCH}
Build Type:     ${BUILD_TYPE}
Shared Library: ${BUILD_SHARED}
Compiler:       $(${CC:-gcc} --version | head -n1)
CMake:          $(cmake --version | head -n1)

Build Directory: ${BUILD_DIR}
Install Prefix:  ${DIST_DIR}
EOF

echo "Build info written to ${DIST_DIR}/BUILD_INFO.txt"
echo ""

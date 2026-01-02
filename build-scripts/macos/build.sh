#!/bin/bash
set -euo pipefail

# ServerLink macOS Build Script
# Builds ServerLink for macOS (x86_64 or arm64)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Default values
ARCH="${1:-arm64}"
BUILD_TYPE="${2:-Release}"
BUILD_SHARED="${3:-ON}"

echo "============================================"
echo "ServerLink macOS Build"
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
DIST_DIR="${PROJECT_ROOT}/dist/macos-${ARCH}"

# Clean previous build if requested
if [ "${CLEAN_BUILD:-0}" = "1" ]; then
    echo "Cleaning previous build..."
    rm -rf "${BUILD_DIR}"
    rm -rf "${DIST_DIR}"
fi

# Create directories
mkdir -p "${BUILD_DIR}"
mkdir -p "${DIST_DIR}"

# Set architecture-specific flags
case "${ARCH}" in
    x86_64)
        CMAKE_OSX_ARCHITECTURES="x86_64"
        ;;
    arm64)
        CMAKE_OSX_ARCHITECTURES="arm64"
        ;;
    *)
        echo "Error: Unsupported architecture '${ARCH}'"
        echo "Supported: x86_64, arm64"
        exit 1
        ;;
esac

# Detect macOS SDK
OSX_SDK_PATH=$(xcrun --sdk macosx --show-sdk-path)
OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-11.0}"

echo "macOS SDK: ${OSX_SDK_PATH}"
echo "Deployment Target: ${OSX_DEPLOYMENT_TARGET}"
echo ""

# Configure CMake
echo "Configuring CMake..."
cd "${BUILD_DIR}"

cmake "${PROJECT_ROOT}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DBUILD_SHARED_LIBS="${BUILD_SHARED}" \
    -DBUILD_TESTS=ON \
    -DBUILD_EXAMPLES=ON \
    -DCMAKE_INSTALL_PREFIX="${DIST_DIR}" \
    -DCMAKE_OSX_ARCHITECTURES="${CMAKE_OSX_ARCHITECTURES}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${OSX_DEPLOYMENT_TARGET}" \
    -DCMAKE_OSX_SYSROOT="${OSX_SDK_PATH}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
echo ""
echo "Building ServerLink..."
cmake --build . --config "${BUILD_TYPE}" --parallel $(sysctl -n hw.ncpu)

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
    find "${BUILD_DIR}/tests" -type f -perm +111 -exec cp {} "${DIST_DIR}/tests/" \; 2>/dev/null || true
fi

# Copy benchmark binaries
if [ -d "${BUILD_DIR}/tests/benchmark" ]; then
    mkdir -p "${DIST_DIR}/benchmarks"
    find "${BUILD_DIR}/tests/benchmark" -name "bench_*" -type f -perm +111 -exec cp {} "${DIST_DIR}/benchmarks/" \; 2>/dev/null || true
fi

# Print library info
echo ""
echo "============================================"
echo "Build completed successfully!"
echo "============================================"
echo "Output directory: ${DIST_DIR}"
echo ""

if [ "${BUILD_SHARED}" = "ON" ]; then
    LIBFILE="${DIST_DIR}/lib/libserverlink.dylib"
    if [ -f "${LIBFILE}" ]; then
        echo "Shared library: ${LIBFILE}"
        file "${LIBFILE}"
        echo ""
        echo "Architecture verification:"
        lipo -info "${LIBFILE}"
        echo ""
        echo "Library dependencies:"
        otool -L "${LIBFILE}" || true
    fi
else
    LIBFILE="${DIST_DIR}/lib/libserverlink.a"
    if [ -f "${LIBFILE}" ]; then
        echo "Static library: ${LIBFILE}"
        file "${LIBFILE}"
        echo ""
        echo "Architecture verification:"
        lipo -info "${LIBFILE}"
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
Platform:       macOS ${ARCH}
Build Type:     ${BUILD_TYPE}
Shared Library: ${BUILD_SHARED}
Deployment:     ${OSX_DEPLOYMENT_TARGET}+
Compiler:       $(${CC:-clang} --version | head -n1)
CMake:          $(cmake --version | head -n1)

Build Directory: ${BUILD_DIR}
Install Prefix:  ${DIST_DIR}
EOF

echo "Build info written to ${DIST_DIR}/BUILD_INFO.txt"
echo ""

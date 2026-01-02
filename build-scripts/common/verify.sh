#!/bin/bash
set -euo pipefail

# ServerLink Build Verification Script
# Verifies that build artifacts exist and are correctly formatted

DIST_DIR="${1:?Usage: $0 <dist_directory>}"
PLATFORM="${2:-unknown}"

echo "============================================"
echo "ServerLink Build Verification"
echo "============================================"
echo "Platform:     ${PLATFORM}"
echo "Directory:    ${DIST_DIR}"
echo "============================================"
echo ""

# Check if dist directory exists
if [ ! -d "${DIST_DIR}" ]; then
    echo "ERROR: Distribution directory not found: ${DIST_DIR}"
    exit 1
fi

# Initialize counters
ERRORS=0
WARNINGS=0

# Function to check file exists
check_file() {
    local file="$1"
    local description="$2"

    if [ -f "${file}" ]; then
        echo "✓ ${description}: ${file}"
        return 0
    else
        echo "✗ ${description}: ${file} (NOT FOUND)"
        ((ERRORS++))
        return 1
    fi
}

# Function to check directory exists
check_dir() {
    local dir="$1"
    local description="$2"

    if [ -d "${dir}" ]; then
        echo "✓ ${description}: ${dir}"
        return 0
    else
        echo "⚠ ${description}: ${dir} (NOT FOUND)"
        ((WARNINGS++))
        return 1
    fi
}

echo "Checking required files..."
echo ""

# Check LICENSE and README
check_file "${DIST_DIR}/LICENSE" "License file"
check_file "${DIST_DIR}/README.md" "README file"
check_file "${DIST_DIR}/BUILD_INFO.txt" "Build info"

echo ""
echo "Checking library files..."
echo ""

# Platform-specific library checks
case "${PLATFORM}" in
    linux-*)
        # Check for shared or static library
        if [ -f "${DIST_DIR}/lib/libserverlink.so" ]; then
            check_file "${DIST_DIR}/lib/libserverlink.so" "Shared library"

            # Verify ELF format
            echo ""
            echo "Library information:"
            file "${DIST_DIR}/lib/libserverlink.so"

            # Check dependencies
            echo ""
            echo "Library dependencies:"
            ldd "${DIST_DIR}/lib/libserverlink.so" || true

        elif [ -f "${DIST_DIR}/lib/libserverlink.a" ]; then
            check_file "${DIST_DIR}/lib/libserverlink.a" "Static library"

            echo ""
            echo "Library information:"
            file "${DIST_DIR}/lib/libserverlink.a"
        else
            echo "✗ No library found (libserverlink.so or libserverlink.a)"
            ((ERRORS++))
        fi
        ;;

    windows-*)
        # Check for DLL or static library
        if [ -f "${DIST_DIR}/serverlink.dll" ] || [ -f "${DIST_DIR}/bin/serverlink.dll" ]; then
            if [ -f "${DIST_DIR}/serverlink.dll" ]; then
                check_file "${DIST_DIR}/serverlink.dll" "Shared library (DLL)"
            else
                check_file "${DIST_DIR}/bin/serverlink.dll" "Shared library (DLL)"
            fi

        elif [ -f "${DIST_DIR}/lib/serverlink.lib" ]; then
            check_file "${DIST_DIR}/lib/serverlink.lib" "Static library"
        else
            echo "✗ No library found (serverlink.dll or serverlink.lib)"
            ((ERRORS++))
        fi
        ;;

    macos-*)
        # Check for dylib or static library
        if [ -f "${DIST_DIR}/lib/libserverlink.dylib" ]; then
            check_file "${DIST_DIR}/lib/libserverlink.dylib" "Shared library (dylib)"

            echo ""
            echo "Library information:"
            file "${DIST_DIR}/lib/libserverlink.dylib"

            echo ""
            echo "Architecture:"
            lipo -info "${DIST_DIR}/lib/libserverlink.dylib" || true

            echo ""
            echo "Library dependencies:"
            otool -L "${DIST_DIR}/lib/libserverlink.dylib" || true

        elif [ -f "${DIST_DIR}/lib/libserverlink.a" ]; then
            check_file "${DIST_DIR}/lib/libserverlink.a" "Static library"

            echo ""
            echo "Library information:"
            file "${DIST_DIR}/lib/libserverlink.a"

            echo ""
            echo "Architecture:"
            lipo -info "${DIST_DIR}/lib/libserverlink.a" || true
        else
            echo "✗ No library found (libserverlink.dylib or libserverlink.a)"
            ((ERRORS++))
        fi
        ;;

    *)
        echo "⚠ Unknown platform: ${PLATFORM}"
        ((WARNINGS++))
        ;;
esac

echo ""
echo "Checking header files..."
echo ""

check_dir "${DIST_DIR}/include" "Include directory"
if [ -d "${DIST_DIR}/include/serverlink" ]; then
    check_file "${DIST_DIR}/include/serverlink/serverlink.h" "Main header"
    check_file "${DIST_DIR}/include/serverlink/serverlink_export.h" "Export header"
    check_file "${DIST_DIR}/include/serverlink/config.h" "Config header"
fi

echo ""
echo "Checking optional components..."
echo ""

check_dir "${DIST_DIR}/tests" "Test binaries (optional)"
check_dir "${DIST_DIR}/benchmarks" "Benchmark binaries (optional)"

echo ""
echo "============================================"
echo "Verification Summary"
echo "============================================"
echo "Errors:   ${ERRORS}"
echo "Warnings: ${WARNINGS}"
echo ""

if [ ${ERRORS} -eq 0 ]; then
    echo "✓ Build verification PASSED"
    echo ""

    # Print file sizes
    echo "Distribution size:"
    du -sh "${DIST_DIR}"
    echo ""

    # List all files
    echo "Files in distribution:"
    find "${DIST_DIR}" -type f | sort
    echo ""

    exit 0
else
    echo "✗ Build verification FAILED"
    echo ""
    exit 1
fi

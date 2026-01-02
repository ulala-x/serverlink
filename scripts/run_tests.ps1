# ServerLink Test Runner Script (Windows)
# Runs all CTest tests with proper output formatting

param(
    [string]$BuildDir = "",
    [string]$BuildType = "Release",
    [int]$TestTimeout = 300,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Resolve-Path (Join-Path $ScriptDir "..")

# Determine build directory
if ($BuildDir -eq "") {
    $BuildDir = Join-Path $ProjectRoot "build"
}

Write-Host "============================================"
Write-Host "ServerLink Test Suite"
Write-Host "============================================"
Write-Host "Build Directory: $BuildDir"
Write-Host "Build Type:      $BuildType"
Write-Host "Test Timeout:    ${TestTimeout}s"
Write-Host "============================================"
Write-Host ""

# Check if build directory exists
if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory not found: $BuildDir"
    Write-Host "Please build the project first or set BUILD_DIR environment variable"
    exit 1
}

# Change to build directory
Push-Location $BuildDir

try {
    # Check if tests were built
    if (-not (Test-Path "CTestTestfile.cmake")) {
        Write-Error "No tests found in build directory"
        Write-Host "Please build with -DBUILD_TESTS=ON"
        exit 1
    }

    # Run CTest
    Write-Host "Running tests..."
    Write-Host ""

    $CtestArgs = @(
        "-C", "$BuildType",
        "--output-on-failure",
        "--timeout", "$TestTimeout"
    )

    if ($Verbose) {
        $CtestArgs += "--verbose"
    }

    # Set PATH to include build output directories for DLLs
    $env:PATH = "$BuildDir\Release;$BuildDir\Debug;$env:PATH"

    # Run all tests
    $TestResult = $true
    try {
        & ctest $CtestArgs
        if ($LASTEXITCODE -ne 0) {
            $TestResult = $false
        }
    } catch {
        $TestResult = $false
    }

    Write-Host ""

    if ($TestResult) {
        Write-Host "============================================"
        Write-Host "✓ All tests PASSED"
        Write-Host "============================================"
        Write-Host ""

        # Show test summary
        $TestCount = (ctest -C $BuildType -N | Select-String "^  Test").Count
        Write-Host "Test summary:"
        Write-Host "Total tests: $TestCount"
        Write-Host ""

        exit 0
    } else {
        Write-Host "============================================"
        Write-Host "✗ Some tests FAILED"
        Write-Host "============================================"
        Write-Host ""

        # Show failed tests
        Write-Host "Failed tests:"
        ctest -C $BuildType --rerun-failed --output-on-failure
        Write-Host ""

        exit 1
    }
} finally {
    Pop-Location
}

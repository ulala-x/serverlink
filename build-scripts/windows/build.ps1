# ServerLink Windows Build Script
# Builds ServerLink for Windows (x64 or ARM64)

param(
    [string]$Architecture = "x64",
    [string]$BuildType = "Release",
    [string]$BuildShared = "ON",
    [string]$Generator = "Visual Studio 17 2022"
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Resolve-Path (Join-Path $ScriptDir "..\..")

Write-Host "============================================"
Write-Host "ServerLink Windows Build"
Write-Host "============================================"
Write-Host "Architecture: $Architecture"
Write-Host "Build Type:   $BuildType"
Write-Host "Shared Libs:  $BuildShared"
Write-Host "Generator:    $Generator"
Write-Host "============================================"
Write-Host ""

# Read version from VERSION file
$VersionFile = Join-Path $ProjectRoot "VERSION"
$ServerLinkVersion = "0.1.0"

if (Test-Path $VersionFile) {
    Get-Content $VersionFile | ForEach-Object {
        if ($_ -match '^SERVERLINK_VERSION=(.+)$') {
            $ServerLinkVersion = $Matches[1]
        }
    }
    Write-Host "Version: $ServerLinkVersion"
}

# Determine build directory and install prefix
$BuildDir = Join-Path $ProjectRoot "build-$Architecture"
$DistDir = Join-Path $ProjectRoot "dist\windows-$Architecture"

# Clean previous build if requested
if ($env:CLEAN_BUILD -eq "1") {
    Write-Host "Cleaning previous build..."
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
    }
    if (Test-Path $DistDir) {
        Remove-Item -Recurse -Force $DistDir
    }
}

# Create directories
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

# Map architecture to CMake platform
$CMakePlatform = switch ($Architecture) {
    "x64"   { "x64" }
    "arm64" { "ARM64" }
    default { throw "Unsupported architecture: $Architecture" }
}

# Configure CMake
Write-Host "Configuring CMake..."
Push-Location $BuildDir

try {
    cmake $ProjectRoot `
        -G $Generator `
        -A $CMakePlatform `
        -DCMAKE_BUILD_TYPE="$BuildType" `
        -DBUILD_SHARED_LIBS="$BuildShared" `
        -DBUILD_TESTS=ON `
        -DBUILD_EXAMPLES=ON `
        -DCMAKE_INSTALL_PREFIX="$DistDir"

    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }

    # Build
    Write-Host ""
    Write-Host "Building ServerLink..."
    cmake --build . --config $BuildType --parallel

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }

    # Install to dist directory
    Write-Host ""
    Write-Host "Installing to $DistDir..."
    cmake --build . --target install --config $BuildType

    if ($LASTEXITCODE -ne 0) {
        throw "Install failed"
    }

    # Copy additional files
    Write-Host ""
    Write-Host "Copying additional files..."
    Copy-Item (Join-Path $ProjectRoot "LICENSE") $DistDir -Force
    Copy-Item (Join-Path $ProjectRoot "README.md") $DistDir -Force

    $CompileCommandsJson = Join-Path $BuildDir "compile_commands.json"
    if (Test-Path $CompileCommandsJson) {
        Copy-Item $CompileCommandsJson $DistDir -Force
    }

    # Copy test binaries
    $TestsDir = Join-Path $BuildDir "tests\$BuildType"
    if (Test-Path $TestsDir) {
        $DistTestsDir = Join-Path $DistDir "tests"
        New-Item -ItemType Directory -Force -Path $DistTestsDir | Out-Null
        Get-ChildItem -Path $TestsDir -Filter "*.exe" | Copy-Item -Destination $DistTestsDir
    }

    # Copy benchmark binaries
    $BenchmarkDir = Join-Path $BuildDir "tests\benchmark\$BuildType"
    if (Test-Path $BenchmarkDir) {
        $DistBenchmarkDir = Join-Path $DistDir "benchmarks"
        New-Item -ItemType Directory -Force -Path $DistBenchmarkDir | Out-Null
        Get-ChildItem -Path $BenchmarkDir -Filter "bench_*.exe" | Copy-Item -Destination $DistBenchmarkDir
    }

    # Copy runtime DLLs to dist root for convenience
    $BinDir = Join-Path $DistDir "bin"
    if (Test-Path $BinDir) {
        Get-ChildItem -Path $BinDir -Filter "*.dll" | Copy-Item -Destination $DistDir
    }

    # Print library info
    Write-Host ""
    Write-Host "============================================"
    Write-Host "Build completed successfully!"
    Write-Host "============================================"
    Write-Host "Output directory: $DistDir"
    Write-Host ""

    if ($BuildShared -eq "ON") {
        $LibFile = Join-Path $DistDir "serverlink.dll"
        if (Test-Path $LibFile) {
            Write-Host "Shared library: $LibFile"
            Write-Host ""
            Write-Host "DLL dependencies:"
            dumpbin /DEPENDENTS $LibFile 2>$null
        }
    } else {
        $LibFile = Join-Path $DistDir "lib\serverlink.lib"
        if (Test-Path $LibFile) {
            Write-Host "Static library: $LibFile"
        }
    }

    Write-Host ""
    Write-Host "Directory structure:"
    Get-ChildItem $DistDir | Format-Table -Property Mode, Length, Name
    Write-Host ""

    # Generate build info
    $BuildInfo = @"
ServerLink Build Information
=============================

Version:        $ServerLinkVersion
Build Date:     $(Get-Date -Format "yyyy-MM-dd HH:mm:ss UTC")
Platform:       Windows $Architecture
Build Type:     $BuildType
Shared Library: $BuildShared
Compiler:       MSVC (Visual Studio 2022)
CMake:          $(cmake --version | Select-Object -First 1)

Build Directory: $BuildDir
Install Prefix:  $DistDir
"@

    $BuildInfo | Out-File -FilePath (Join-Path $DistDir "BUILD_INFO.txt") -Encoding UTF8
    Write-Host "Build info written to $(Join-Path $DistDir 'BUILD_INFO.txt')"
    Write-Host ""

} finally {
    Pop-Location
}

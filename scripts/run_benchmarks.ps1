# ServerLink Benchmark Runner Script (Windows)
# Runs all benchmarks and outputs results in JSON format

param(
    [string]$BuildDir = "",
    [string]$OutputFile = "benchmark_results.json",
    [string]$Platform = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Resolve-Path (Join-Path $ScriptDir "..")

# Determine build directory
if ($BuildDir -eq "") {
    $BuildDir = Join-Path $ProjectRoot "build"
}

Write-Host "============================================"
Write-Host "ServerLink Benchmark Suite"
Write-Host "============================================"
Write-Host "Build Directory: $BuildDir"
Write-Host "Output File:     $OutputFile"
Write-Host "============================================"
Write-Host ""

# Check if build directory exists
if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory not found: $BuildDir"
    Write-Host "Please build the project first or set BUILD_DIR environment variable"
    exit 1
}

# Find benchmark executables
$BenchmarkDirs = @(
    (Join-Path $BuildDir "tests\benchmark\Release"),
    (Join-Path $BuildDir "tests\benchmark\Debug"),
    (Join-Path $BuildDir "tests\benchmark")
)

$BenchmarkDir = $null
foreach ($Dir in $BenchmarkDirs) {
    if (Test-Path $Dir) {
        $BenchmarkDir = $Dir
        break
    }
}

if ($null -eq $BenchmarkDir) {
    Write-Error "Benchmark directory not found"
    Write-Host "Please build with -DBUILD_TESTS=ON"
    exit 1
}

# Find all benchmark executables
$Benchmarks = Get-ChildItem -Path $BenchmarkDir -Filter "bench_*.exe" -File

if ($Benchmarks.Count -eq 0) {
    Write-Error "No benchmark executables found in $BenchmarkDir"
    exit 1
}

Write-Host "Found benchmarks:"
foreach ($Bench in $Benchmarks) {
    Write-Host "  - $($Bench.Name)"
}
Write-Host ""

# Set PATH to include DLLs
$env:PATH = "$BuildDir\Release;$BuildDir\Debug;$env:PATH"

# Create temporary directory for individual results
$TempDir = Join-Path $env:TEMP "serverlink_bench_$(Get-Random)"
New-Item -ItemType Directory -Path $TempDir | Out-Null

# Collect system information
Write-Host "Collecting system information..."
$SysInfoFile = Join-Path $TempDir "sysinfo.json"

try {
    $OS = Get-CimInstance -ClassName Win32_OperatingSystem
    $CPU = Get-CimInstance -ClassName Win32_Processor | Select-Object -First 1
    $CS = Get-CimInstance -ClassName Win32_ComputerSystem

    $SysInfo = @{
        os = "$($OS.Caption) $($OS.Version)"
        arch = $env:PROCESSOR_ARCHITECTURE
        cpu = $CPU.Name
        cores = $CPU.NumberOfCores
        logical_processors = $CPU.NumberOfLogicalProcessors
        memory_gb = [math]::Round($CS.TotalPhysicalMemory / 1GB, 1)
        windows_build = $OS.BuildNumber
    }

    $SysInfo | ConvertTo-Json | Out-File -FilePath $SysInfoFile -Encoding UTF8

    Write-Host "System Info:"
    $SysInfo | ConvertTo-Json
    Write-Host ""
} catch {
    Write-Warning "Failed to collect system info: $_"
}

try {
    # Run each benchmark
    $BenchCount = 0
    foreach ($Bench in $Benchmarks) {
        $BenchName = $Bench.Name
        Write-Host "Running $BenchName..."

        # Run benchmark and capture output
        $OutputFileTmp = Join-Path $TempDir "$BenchName.txt"

        try {
            & $Bench.FullName | Out-File -FilePath $OutputFileTmp -Encoding UTF8
            Write-Host "  ✓ $BenchName completed"
            $BenchCount++
        } catch {
            Write-Host "  ✗ $BenchName failed"
            Get-Content $OutputFileTmp
        }

        Write-Host ""
    }

    Write-Host "============================================"
    Write-Host "Benchmark Results"
    Write-Host "============================================"
    Write-Host "Completed: $BenchCount benchmarks"
    Write-Host ""

    # Combine results into JSON format using Python formatter
    $FormatterScript = Join-Path $ScriptDir "format_benchmark.py"
    if (Test-Path $FormatterScript) {
        Write-Host "Formatting results to JSON..."
        $PlatformArgs = @()
        if ($Platform -ne "") {
            $PlatformArgs = @("--platform", $Platform)
        }
        & python $FormatterScript $TempDir $OutputFile @PlatformArgs

        if (Test-Path $OutputFile) {
            Write-Host "Results written to: $OutputFile"
            Write-Host ""

            # Show summary (if jq is available)
            if (Get-Command jq -ErrorAction SilentlyContinue) {
                Write-Host "Summary:"
                & jq -r '.benchmarks[] | "  \(.name): \(.throughput_msg_per_sec // .latency_us // "N/A") \(.unit // "")"' $OutputFile
            } else {
                Write-Host "Install 'jq' to see formatted summary"
            }
        }
    } else {
        Write-Warning "format_benchmark.py not found, skipping JSON formatting"
        Write-Host "Raw results are in: $TempDir"
    }

    Write-Host ""
    Write-Host "✓ Benchmark suite completed"
    exit 0

} finally {
    # Clean up temporary directory
    if (Test-Path $TempDir) {
        Remove-Item -Recurse -Force $TempDir
    }
}

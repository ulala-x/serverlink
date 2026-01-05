@echo off
setlocal

REM Try VS 2022 first, then fallback
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    if errorlevel 1 (
        echo Failed to setup VS environment
        exit /b 1
    )
)

echo.
echo ========================================
echo ServerLink IOCP Benchmark Build
echo ========================================
echo.

REM Clean and reconfigure with CMake
echo [1/4] Reconfiguring CMake for IOCP support...
cd /d D:\project\ulalax\serverlink
if exist build\CMakeCache.txt del build\CMakeCache.txt

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -A x64
if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)

echo.
echo [2/4] Building serverlink library...
cmake --build build --config Release --target serverlink -- /v:m
if errorlevel 1 (
    echo Library build failed
    exit /b 1
)

echo.
echo [3/4] Building benchmarks...
cmake --build build --config Release --target bench_throughput bench_latency bench_pubsub -- /v:m
if errorlevel 1 (
    echo Benchmark build failed
    exit /b 1
)

echo.
echo [4/4] Running benchmarks...
echo.
echo ========================================
echo Throughput Benchmark
echo ========================================
build\tests\benchmark\Release\bench_throughput.exe

echo.
echo ========================================
echo Latency Benchmark
echo ========================================
build\tests\benchmark\Release\bench_latency.exe

echo.
echo ========================================
echo PubSub Benchmark
echo ========================================
build\tests\benchmark\Release\bench_pubsub.exe

echo.
echo ========================================
echo Benchmarks completed!
echo ========================================

@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    if errorlevel 1 (
        echo Failed to setup VS environment
        exit /b 1
    )
)
echo Building serverlink...
msbuild D:\project\ulalax\serverlink\build\serverlink.sln /t:serverlink /p:Configuration=Release /v:m
if errorlevel 1 (
    echo Build failed
    exit /b 1
)
echo Building benchmarks...
msbuild D:\project\ulalax\serverlink\build\serverlink.sln /t:bench_throughput;bench_latency;bench_pubsub /p:Configuration=Release /v:m
if errorlevel 1 (
    echo Benchmark build failed
    exit /b 1
)
echo Build completed successfully

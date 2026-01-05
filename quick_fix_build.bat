@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\project\ulalax\serverlink\build-iocp
msbuild serverlink.sln /t:serverlink /p:Configuration=Release /v:minimal /nologo
if errorlevel 1 (
    echo Build FAILED
    exit /b 1
)
echo serverlink.dll build completed
msbuild serverlink.sln /t:test_router_handover /p:Configuration=Release /v:minimal /nologo
if errorlevel 1 (
    echo Test build FAILED
    exit /b 1
)
copy /Y Release\serverlink.dll tests\Release\ >nul 2>&1
echo Test executables rebuilt

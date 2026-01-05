@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\project\ulalax\serverlink
echo Rebuilding serverlink.dll...
msbuild build-iocp\serverlink.sln /t:serverlink /p:Configuration=Release /v:minimal /nologo
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)
echo.
echo Copying DLL...
copy /Y build-iocp\Release\serverlink.dll build-iocp\tests\Release\ >nul
echo.
echo Rebuilding test_ctx...
msbuild build-iocp\serverlink.sln /t:test_ctx /p:Configuration=Release /v:minimal /nologo
if errorlevel 1 (
    echo Test build failed!
    exit /b 1
)
echo.
echo Build completed successfully!
echo.
echo Running test_ctx with socket debug logging...
cd build-iocp\tests\Release
test_ctx.exe 2>socket_debug.log
echo.
echo Test completed. Check socket_debug.log for output.

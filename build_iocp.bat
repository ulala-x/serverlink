@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\project\ulalax\serverlink
echo Building serverlink DLL...
msbuild build-iocp\serverlink.sln /t:serverlink /p:Configuration=Release /v:m
if errorlevel 1 (
    echo Build failed
    exit /b 1
)
echo Building test_ctx...
msbuild build-iocp\serverlink.sln /t:test_ctx /p:Configuration=Release /v:m
if errorlevel 1 (
    echo Test build failed
    exit /b 1
)
echo Copying DLL...
copy /Y build-iocp\Release\serverlink.dll build-iocp\tests\Release\
echo Build completed successfully

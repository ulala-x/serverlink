@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\project\ulalax\serverlink
msbuild build-iocp\serverlink.sln /t:serverlink /p:Configuration=Debug /v:minimal /nologo
if errorlevel 1 exit /b 1
msbuild build-iocp\serverlink.sln /t:test_ctx /p:Configuration=Debug /v:minimal /nologo
if errorlevel 1 exit /b 1
copy /Y build-iocp\Debug\serverlink.dll build-iocp\tests\Debug\ > nul
echo Build completed successfully

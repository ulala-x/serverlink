@echo off
echo Building serverlink.dll with socket creation logging...
cmake --build build-iocp --config Release --target serverlink
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo Copying DLL to test directory...
copy /Y build-iocp\Release\serverlink.dll build-iocp\tests\Release\
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo Building test_ctx...
cmake --build build-iocp --config Release --target test_ctx
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo Running test_ctx (redirecting stderr to log)...
cd build-iocp\tests\Release
test_ctx.exe 2> socket_debug.log
set TEST_RESULT=%ERRORLEVEL%

echo.
echo Test exit code: %TEST_RESULT%
echo.
echo Last 50 lines of socket_debug.log:
type socket_debug.log | findstr /N "^" | findstr /R ".*" | findstr /R "[0-9]:[0-9]*:[[]" > temp_numbered.txt
for /f "tokens=*" %%a in ('type temp_numbered.txt ^| findstr /R ".*"') do set LAST_LINE=%%a
del temp_numbered.txt

echo.
echo Full log at: build-iocp\tests\Release\socket_debug.log
type socket_debug.log

@echo off
echo Building ServerLink with IOCP debug logging...
cd /d D:\project\ulalax\serverlink
cmake --build build-iocp --config Release --parallel 8
if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)

echo.
echo Running test_ctx_socket...
cd build-iocp\Release
test_ctx_socket.exe > test_output.log 2>&1
set TEST_EXIT=%errorlevel%

echo.
echo Test exit code: %TEST_EXIT%
echo.
echo ========== Test Output ==========
type test_output.log
echo.
echo ========== End of Test Output ==========

exit /b %TEST_EXIT%

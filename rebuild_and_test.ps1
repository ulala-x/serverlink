Write-Host "Building serverlink.dll..."
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-iocp --config Release --target serverlink

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!"
    exit 1
}

Write-Host "`nCopying DLL..."
Copy-Item -Force build-iocp\Release\serverlink.dll build-iocp\tests\Release\

Write-Host "`nBuilding test_ctx..."
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-iocp --config Release --target test_ctx

if ($LASTEXITCODE -ne 0) {
    Write-Host "Test build failed!"
    exit 1
}

Write-Host "`nRunning test..."
Set-Location build-iocp\tests\Release
& ".\test_ctx.exe" 2>&1 | Tee-Object socket_debug.log
$testResult = $LASTEXITCODE

Write-Host "`nTest exit code: $testResult"
Write-Host "`nLog contents:"
Get-Content socket_debug.log

$ErrorActionPreference = "Stop"

Write-Host "Building serverlink..."
cmake --build build-iocp --config Release --target serverlink
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "`nCopying DLL..."
Copy-Item build-iocp\Release\serverlink.dll build-iocp\tests\Release\ -Force

Write-Host "`nBuilding test..."
cmake --build build-iocp --config Release --target test_ctx
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "`nDone!"

# Setup Visual Studio environment
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64

# Navigate to build directory
Set-Location D:\project\ulalax\serverlink\build-iocp

# Build serverlink DLL
Write-Host "Building serverlink.dll..."
msbuild serverlink.sln /t:serverlink /p:Configuration=Release /v:minimal /nologo
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build FAILED" -ForegroundColor Red
    exit 1
}

Write-Host "Building test_router_handover..."
msbuild serverlink.sln /t:test_router_handover /p:Configuration=Release /v:minimal /nologo
if ($LASTEXITCODE -ne 0) {
    Write-Host "Test build FAILED" -ForegroundColor Red
    exit 1
}

# Copy DLL
Copy-Item Release\serverlink.dll tests\Release\ -Force
Write-Host "Build completed successfully" -ForegroundColor Green

@echo off
echo Testing IOCP signaler integration build...

REM Find Visual Studio
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set VS_PATH=%%i
)

if not defined VS_PATH (
    echo Visual Studio not found
    exit /b 1
)

REM Setup Visual Studio environment
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"

REM Test compile modified files
echo.
echo Compiling signaler.cpp...
cl /nologo /c /EHsc /std:c++20 /I. /Isrc /DSL_USE_IOCP /D_WIN32 /DWIN32 /D_WINDOWS src\io\signaler.cpp /Fo:test_signaler.obj

if %ERRORLEVEL% NEQ 0 (
    echo signaler.cpp compilation failed
    exit /b 1
)

echo.
echo Compiling iocp.cpp...
cl /nologo /c /EHsc /std:c++20 /I. /Isrc /DSL_USE_IOCP /D_WIN32 /DWIN32 /D_WINDOWS src\io\iocp.cpp /Fo:test_iocp.obj

if %ERRORLEVEL% NEQ 0 (
    echo iocp.cpp compilation failed
    exit /b 1
)

echo.
echo Compiling io_thread.cpp...
cl /nologo /c /EHsc /std:c++20 /I. /Isrc /DSL_USE_IOCP /D_WIN32 /DWIN32 /D_WINDOWS src\io\io_thread.cpp /Fo:test_io_thread.obj

if %ERRORLEVEL% NEQ 0 (
    echo io_thread.cpp compilation failed
    exit /b 1
)

echo.
echo Compiling mailbox.cpp...
cl /nologo /c /EHsc /std:c++20 /I. /Isrc /DSL_USE_IOCP /D_WIN32 /DWIN32 /D_WINDOWS src\io\mailbox.cpp /Fo:test_mailbox.obj

if %ERRORLEVEL% NEQ 0 (
    echo mailbox.cpp compilation failed
    exit /b 1
)

echo.
echo All test compilations successful!
echo.

REM Cleanup
del test_*.obj 2>nul

exit /b 0

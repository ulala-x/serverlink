@echo off
echo Running test_spot_basic...
D:\project\ulalax\serverlink\build\tests\Release\test_spot_basic.exe
echo Exit code: %ERRORLEVEL%
echo.
echo Running test_spot_local...
D:\project\ulalax\serverlink\build\tests\Release\test_spot_local.exe
echo Exit code: %ERRORLEVEL%
echo.
echo Running test_spot_remote...
D:\project\ulalax\serverlink\build\tests\Release\test_spot_remote.exe
echo Exit code: %ERRORLEVEL%
echo.
echo Running test_spot_cluster...
D:\project\ulalax\serverlink\build\tests\Release\test_spot_cluster.exe
echo Exit code: %ERRORLEVEL%
echo.
echo Running test_spot_mixed...
D:\project\ulalax\serverlink\build\tests\Release\test_spot_mixed.exe
echo Exit code: %ERRORLEVEL%

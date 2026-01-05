@echo off
cmake --build build-iocp --config Release --target serverlink
copy /Y build-iocp\Release\serverlink.dll build-iocp\tests\Release\
cmake --build build-iocp --config Release --target test_ctx

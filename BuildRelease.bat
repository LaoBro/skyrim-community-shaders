@echo off

cmake -S . --preset=ALL --check-stamp-file "build\CMakeFiles\generate.stamp"
if %ERRORLEVEL% NEQ 0 exit 1
cmake --build build --config Release
pause
if %ERRORLEVEL% NEQ 0 exit 1

pause
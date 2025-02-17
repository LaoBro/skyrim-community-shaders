@echo off

set preset="ALL"
if NOT "%1" == "" (
    set preset=%1
)

echo Running preset %preset%

cmake -S . --preset=%preset% --check-stamp-file "build\CMakeFiles\generate.stamp"
if %ERRORLEVEL% NEQ 0 exit 1
cmake --build build --config Release
pause
if %ERRORLEVEL% NEQ 0 exit 1

pause
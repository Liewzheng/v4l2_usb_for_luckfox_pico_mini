@echo off
REM Windows compilation script for v4l2_usb_pc

echo Compiling V4L2 USB PC Client for Windows...

REM Check if gcc is available
where gcc >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: gcc not found in PATH
    echo Please install MinGW-w64 or TDM-GCC
    echo Download from: https://www.mingw-w64.org/ or https://jmeubank.github.io/tdm-gcc/
    pause
    exit /b 1
)

REM Compile the program
gcc -Wall -Wextra -O2 -std=c99 -o v4l2_usb_pc.exe v4l2_usb_pc.c -lws2_32

if %ERRORLEVEL% eq 0 (
    echo Build successful: v4l2_usb_pc.exe
    echo.
    echo Usage examples:
    echo   v4l2_usb_pc.exe --help
    echo   v4l2_usb_pc.exe -s 172.32.0.93 -p 8888
    echo   v4l2_usb_pc.exe -s 172.32.0.93 -o ./frames
    echo.
) else (
    echo Build failed!
    pause
    exit /b 1
)

pause

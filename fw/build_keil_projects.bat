@echo off
setlocal enabledelayedexpansion
REM Switch code page to UTF-8
chcp 65001 >nul

REM ===========================================================================
REM Set Keil UV4 executable path
REM If your installation path is different, please modify the variable below
REM ===========================================================================
set "UV4_PATH=C:\Keil_v5\UV4\UV4.exe"

REM Attempt to auto-detect other common installation paths
if not exist "!UV4_PATH!" set "UV4_PATH=C:\Users\tester\AppData\Local\Keil_v5\UV4\UV4.exe"


REM Check if Keil executable exists
if not exist "!UV4_PATH!" (
    echo [Error] Keil UV4 executable not found: "!UV4_PATH!"
    echo Please edit this bat file and correct the UV4_PATH.
    pause
    exit /b 1
)

echo ==========================================================================
echo Searching and compiling Keil projects in the directory...
echo Root Directory: %CD%
echo ==========================================================================

REM Recursively search for all .uvprojx and .uvproj files
for /R %%f in (*.uvprojx *.uvproj) do (
    echo.
    echo ----------------------------------------------------------------------
    echo Compiling project: %%~nxf
    echo Path: %%~dpf
    
    REM Execute build command
    REM -b: Build
    REM -j0: Hide GUI window
    REM -o: Output to log file
    "!UV4_PATH!" -b "%%f" -j0 -o "%%~dpnxf_build.log"
    
    REM Check return value (0=Success, 1=Warning, >1=Error)
    if !errorlevel! gtr 1 (
        echo [FAILED] Build error, please check log: %%~nxf_build.log
    ) else if !errorlevel! equ 1 (
        echo [WARNING] Build completed with warnings, please check log: %%~nxf_build.log
    ) else (
        echo [SUCCESS] Build completed.
    )
)

echo.
echo ==========================================================================
echo All projects processed.
pause
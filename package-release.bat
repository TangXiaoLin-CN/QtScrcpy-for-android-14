@echo off
REM ============================================================================
REM QtScrcpy One-Click Packaging Script for Qt6 MinGW (Release Version)
REM ============================================================================

setlocal enabledelayedexpansion

echo.
echo ============================================================================
echo QtScrcpy Release Packaging Script
echo ============================================================================
echo.

REM Get script directory
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

REM Configuration - You can modify these
set BUILD_TYPE=Release
set CPU_ARCH=x64
set OUTPUT_DIR=%SCRIPT_DIR%output\%CPU_ARCH%\%BUILD_TYPE%
set PACKAGE_DIR=%SCRIPT_DIR%QtScrcpy-Release
set KEYMAP_DIR=%SCRIPT_DIR%keymap
set CONFIG_DIR=%SCRIPT_DIR%config

REM Try to auto-detect build directory
set BUILD_DIR=
if exist "%SCRIPT_DIR%build\Desktop_Qt_6_10_0_MinGW_64_bit-Release\CMakeCache.txt" (
    set BUILD_DIR=%SCRIPT_DIR%build\Desktop_Qt_6_10_0_MinGW_64_bit-Release
) else (
    echo Error: Release build directory not found!
    echo Expected: %SCRIPT_DIR%build\Desktop_Qt_6_10_0_MinGW_64_bit-Release
    echo.
    echo Please build the Release version first:
    echo 1. Open Qt Creator
    echo 2. Select "Release" build configuration
    echo 3. Build the project
    echo.
    echo Or use package.bat to package the Debug version instead.
    echo.
    pause
    exit /b 1
)

REM Check if build output exists
if not exist "%OUTPUT_DIR%\QtScrcpy.exe" (
    echo Error: QtScrcpy.exe not found in %OUTPUT_DIR%
    echo Please build the project first using CMake
    echo.
    pause
    exit /b 1
)

echo Build output found: %OUTPUT_DIR%
echo Build type: %BUILD_TYPE%
echo.

REM Try to find Qt path from CMake cache
set QT_PATH=
if exist "%BUILD_DIR%\CMakeCache.txt" (
    echo Reading Qt path from CMake configuration...
    for /f "tokens=2 delims==" %%i in ('findstr /C:"CMAKE_PREFIX_PATH:PATH=" "%BUILD_DIR%\CMakeCache.txt"') do (
        set QT_PATH=%%i
    )
)

if not defined QT_PATH (
    echo Warning: Could not find Qt path from CMake cache
    set QT_PATH=D:/software/QT/6.10.0/mingw_64
    echo Using default Qt path: !QT_PATH!
)

echo Qt installation path: %QT_PATH%
echo.

REM Clean previous package
if exist "%PACKAGE_DIR%" (
    echo Cleaning previous package...
    rmdir /s /q "%PACKAGE_DIR%"
)

REM Create package directory
echo Creating package directory...
mkdir "%PACKAGE_DIR%"

REM Copy main executable
echo Copying QtScrcpy.exe...
copy "%OUTPUT_DIR%\QtScrcpy.exe" "%PACKAGE_DIR%\" >nul

REM Copy dependencies from output directory
echo Copying project dependencies...
if exist "%OUTPUT_DIR%\*.dll" copy "%OUTPUT_DIR%\*.dll" "%PACKAGE_DIR%\" >nul
if exist "%OUTPUT_DIR%\adb.exe" copy "%OUTPUT_DIR%\adb.exe" "%PACKAGE_DIR%\" >nul
if exist "%OUTPUT_DIR%\scrcpy-server" copy "%OUTPUT_DIR%\scrcpy-server" "%PACKAGE_DIR%\" >nul
if exist "%OUTPUT_DIR%\*.apk" copy "%OUTPUT_DIR%\*.apk" "%PACKAGE_DIR%\" >nul
if exist "%OUTPUT_DIR%\*.bat" copy "%OUTPUT_DIR%\*.bat" "%PACKAGE_DIR%\" >nul

REM Copy keymap directory
if exist "%KEYMAP_DIR%" (
    echo Copying keymap files...
    xcopy "%KEYMAP_DIR%" "%PACKAGE_DIR%\keymap\" /E /I /Y >nul
) else (
    echo Warning: keymap directory not found
)

REM Copy config directory
if exist "%CONFIG_DIR%" (
    echo Copying config files...
    xcopy "%CONFIG_DIR%" "%PACKAGE_DIR%\config\" /E /I /Y >nul
) else (
    echo Warning: config directory not found
)

REM Use windeployqt from Qt installation
echo.
echo Deploying Qt dependencies...
set WINDEPLOYQT=%QT_PATH%\bin\windeployqt.exe

if exist "%WINDEPLOYQT%" (
    echo Running windeployqt...
    "%WINDEPLOYQT%" "%PACKAGE_DIR%\QtScrcpy.exe" --no-translations --no-system-d3d-compiler --no-opengl-sw --compiler-runtime
    if %errorlevel% equ 0 (
        echo Qt dependencies deployed successfully
    ) else (
        echo Warning: windeployqt encountered errors
    )
) else (
    echo Error: windeployqt not found at %WINDEPLOYQT%
    echo Please check your Qt installation path
    pause
    exit /b 1
)

REM Create qt.conf to help Qt find plugins
echo Creating qt.conf...
(
echo [Paths]
echo Plugins = .
echo Libraries = .
) > "%PACKAGE_DIR%\qt.conf"

REM Create README
echo Creating README.txt...
(
echo QtScrcpy - Android Screen Mirroring Tool
echo ========================================
echo.
echo This is a portable package of QtScrcpy built with Qt6.10 and MinGW.
echo.
echo How to use:
echo 1. Connect your Android device via USB
echo 2. Enable USB debugging on your device
echo 3. Run QtScrcpy.exe
echo.
echo Package contents:
echo - QtScrcpy.exe: Main application
echo - adb.exe: Android Debug Bridge
echo - scrcpy-server: Server component for Android
echo - keymap/: Keyboard mapping configurations
echo - config/: Application configurations
echo - Qt6 DLLs and plugins: Required runtime libraries
echo.
echo Build information:
echo - Build type: %BUILD_TYPE%
echo - Qt version: 6.10.0
echo - Compiler: MinGW 64-bit
echo - Build date: %date% %time%
echo.
echo For more information, visit: https://github.com/barry-ran/QtScrcpy
) > "%PACKAGE_DIR%\README.txt"

echo.
echo ============================================================================
echo Packaging completed successfully!
echo ============================================================================
echo.
echo Package location: %PACKAGE_DIR%
echo Package size:
for /f "tokens=3" %%a in ('dir "%PACKAGE_DIR%" /s /-c ^| findstr /C:"bytes"') do set SIZE=%%a
echo Approximately %SIZE% bytes
echo.
echo Package contents:
dir /b "%PACKAGE_DIR%" | findstr /v /c:"keymap" /c:"config"
echo   + keymap\ (directory)
echo   + config\ (directory)
echo.
echo You can now distribute the contents of this folder.
echo The package includes all necessary Qt6 and MinGW runtime DLLs.
echo.
echo To test the package, run: %PACKAGE_DIR%\QtScrcpy.exe
echo.
pause

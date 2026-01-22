@echo off
REM ============================================================================
REM QtScrcpy One-Click Packaging Script for Qt6 MinGW
REM ============================================================================

setlocal enabledelayedexpansion

echo.
echo ============================================================================
echo QtScrcpy Packaging Script
echo ============================================================================
echo.

REM Get script directory
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

REM Configuration
set BUILD_TYPE=Debug
set CPU_ARCH=x64
set OUTPUT_DIR=%SCRIPT_DIR%output\%CPU_ARCH%\%BUILD_TYPE%
set PACKAGE_DIR=%SCRIPT_DIR%QtScrcpy-Package
set KEYMAP_DIR=%SCRIPT_DIR%keymap
set CONFIG_DIR=%SCRIPT_DIR%config
set BUILD_DIR=%SCRIPT_DIR%build\Desktop_Qt_6_10_0_MinGW_64_bit-Debug

REM Check if build output exists
if not exist "%OUTPUT_DIR%\QtScrcpy.exe" (
    echo Error: QtScrcpy.exe not found in %OUTPUT_DIR%
    echo Please build the project first using CMake
    pause
    exit /b 1
)

echo Build output found: %OUTPUT_DIR%
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

REM Build android-mouse-cursor APK
echo.
echo ============================================================================
echo Building android-mouse-cursor APK...
echo ============================================================================
set ANDROID_PROJECT_DIR=%SCRIPT_DIR%..\android-mouse-cursor
if exist "%ANDROID_PROJECT_DIR%\gradlew.bat" (
    pushd "%ANDROID_PROJECT_DIR%"

    REM Set JAVA_HOME if not already set
    if not defined JAVA_HOME (
        if exist "D:\software\AndroidStudio\jbr\bin\java.exe" (
            set "JAVA_HOME=D:\software\AndroidStudio\jbr"
            echo Using Java from: !JAVA_HOME!
        )
    )

    echo Cleaning previous build...
    call gradlew.bat clean >nul 2>&1
    echo Building release APK...
    call gradlew.bat assembleRelease
    if errorlevel 1 (
        echo Warning: Android APK build failed, will use existing APK if available
    ) else (
        echo Android APK built successfully
        REM Copy the newly built APK to output directory
        if exist "app\build\outputs\apk\release\app-release.apk" (
            copy "app\build\outputs\apk\release\app-release.apk" "%OUTPUT_DIR%\vmouse.apk" >nul
            echo APK copied to output directory as vmouse.apk
        ) else if exist "app\build\outputs\apk\release\app-release-unsigned.apk" (
            copy "app\build\outputs\apk\release\app-release-unsigned.apk" "%OUTPUT_DIR%\vmouse.apk" >nul
            echo APK copied to output directory as vmouse.apk
        )
    )
    popd
) else (
    echo Warning: android-mouse-cursor project not found at %ANDROID_PROJECT_DIR%
    echo Will use existing APK if available
)
echo.

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
)

REM Copy config directory
if exist "%CONFIG_DIR%" (
    echo Copying config files...
    xcopy "%CONFIG_DIR%" "%PACKAGE_DIR%\config\" /E /I /Y >nul
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
        goto :qt_deployed
    ) else (
        echo Warning: windeployqt encountered errors, trying manual copy...
    )
)

REM Manual copy fallback
echo Warning: windeployqt not available or failed
echo Manually copying Qt DLLs...

    REM Manually copy essential Qt6 DLLs
    if exist "%QT_PATH%\bin" (
        echo Copying Qt6 core DLLs...
        copy "%QT_PATH%\bin\Qt6Core.dll" "%PACKAGE_DIR%\" >nul 2>&1
        copy "%QT_PATH%\bin\Qt6Gui.dll" "%PACKAGE_DIR%\" >nul 2>&1
        copy "%QT_PATH%\bin\Qt6Widgets.dll" "%PACKAGE_DIR%\" >nul 2>&1
        copy "%QT_PATH%\bin\Qt6Network.dll" "%PACKAGE_DIR%\" >nul 2>&1
        copy "%QT_PATH%\bin\Qt6OpenGL.dll" "%PACKAGE_DIR%\" >nul 2>&1
        copy "%QT_PATH%\bin\Qt6OpenGLWidgets.dll" "%PACKAGE_DIR%\" >nul 2>&1

        echo Copying MinGW runtime DLLs...
        copy "%QT_PATH%\bin\libgcc_s_seh-1.dll" "%PACKAGE_DIR%\" >nul 2>&1
        copy "%QT_PATH%\bin\libstdc++-6.dll" "%PACKAGE_DIR%\" >nul 2>&1
        copy "%QT_PATH%\bin\libwinpthread-1.dll" "%PACKAGE_DIR%\" >nul 2>&1

        REM Copy Qt plugins
        echo Copying Qt plugins...
        if not exist "%PACKAGE_DIR%\platforms" mkdir "%PACKAGE_DIR%\platforms"
        copy "%QT_PATH%\plugins\platforms\qwindows.dll" "%PACKAGE_DIR%\platforms\" >nul 2>&1

        if not exist "%PACKAGE_DIR%\styles" mkdir "%PACKAGE_DIR%\styles"
        copy "%QT_PATH%\plugins\styles\qwindowsvistastyle.dll" "%PACKAGE_DIR%\styles\" >nul 2>&1

        if not exist "%PACKAGE_DIR%\imageformats" mkdir "%PACKAGE_DIR%\imageformats"
        copy "%QT_PATH%\plugins\imageformats\qjpeg.dll" "%PACKAGE_DIR%\imageformats\" >nul 2>&1
        copy "%QT_PATH%\plugins\imageformats\qpng.dll" "%PACKAGE_DIR%\imageformats\" >nul 2>&1

        echo Manual DLL copy completed
    ) else (
        echo Error: Qt bin directory not found at %QT_PATH%\bin
        pause
        exit /b 1
    )

:qt_deployed

REM Create qt.conf to help Qt find plugins
echo Creating qt.conf...
(
echo [Paths]
echo Plugins = .
echo Libraries = .
) > "%PACKAGE_DIR%\qt.conf"

echo.
echo ============================================================================
echo Packaging completed successfully!
echo ============================================================================
echo.
echo Package location: %PACKAGE_DIR%
echo.
echo Package contents:
dir /b "%PACKAGE_DIR%" | findstr /v /c:"keymap" /c:"config"
echo   + keymap\ (directory)
echo   + config\ (directory)
echo.
echo You can now distribute the contents of this folder.
echo The package includes all necessary Qt6 and MinGW runtime DLLs.
echo.
pause

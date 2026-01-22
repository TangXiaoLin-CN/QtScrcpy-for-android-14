@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Virtual Mouse APK Installer
echo ========================================
echo.

set VMOUSE_PORT=9999
set VMOUSE_APK=vmouse.apk
set ADB=adb.exe
set PACKAGE_NAME=com.chetbox.mousecursor
set ACTIVITY_NAME=.MainActivity
set MAX_RETRY=3

REM Parse command line arguments
if not "%1"=="" (
    set serial=-s %1
    echo Using device: %1
)
if not "%2"=="" (
    set VMOUSE_PORT=%2
    echo Using port: %2
)
echo.

REM Check if ADB exists
if not exist "%ADB%" (
    echo Error: %ADB% not found in current directory
    goto :error
)

REM Check if APK exists
if not exist "%VMOUSE_APK%" (
    echo Error: %VMOUSE_APK% not found in current directory
    goto :error
)

REM Wait for device with timeout
echo [1/5] Waiting for device %serial%...
%ADB% %serial% wait-for-device
if errorlevel 1 (
    echo Error: Failed to detect device
    goto :error
)
echo Device connected
echo.

REM Check if app is installed
echo [2/5] Checking if %PACKAGE_NAME% is installed...
%ADB% %serial% shell pm path %PACKAGE_NAME% >nul 2>&1
if errorlevel 1 (
    echo App not installed, installing...
    goto :install_app
) else (
    echo App already installed

    REM Check APK version/timestamp to decide if reinstall is needed
    echo Checking if update is needed...
    %ADB% %serial% shell pm dump %PACKAGE_NAME% | findstr "versionName" >nul 2>&1

    REM Always reinstall to ensure latest version
    echo Reinstalling to ensure latest version...
    goto :install_app
)

:install_app
echo [3/5] Installing %VMOUSE_APK%...

REM Try to uninstall first (ignore errors if not installed)
%ADB% %serial% uninstall %PACKAGE_NAME% >nul 2>&1

REM Install with retries
set retry_count=0
:install_retry
set /a retry_count+=1
echo Installing attempt %retry_count%/%MAX_RETRY%...
%ADB% %serial% install -t -r -g "%VMOUSE_APK%"
if errorlevel 1 (
    if %retry_count% lss %MAX_RETRY% (
        echo Install failed, retrying...
        timeout /T 2 /NOBREAK >nul
        goto :install_retry
    ) else (
        echo Error: Failed to install %VMOUSE_APK% after %MAX_RETRY% attempts
        goto :error
    )
)
echo Install successful
echo.

:setup_forward
echo [4/5] Setting up port forwarding %VMOUSE_PORT%...
%ADB% %serial% forward tcp:%VMOUSE_PORT% tcp:%VMOUSE_PORT%
if errorlevel 1 (
    echo Warning: Port forwarding failed, but continuing...
)
echo.

:start_app
echo [5/5] Starting %PACKAGE_NAME%...

REM Grant SYSTEM_ALERT_WINDOW permission
echo Granting overlay permission...
%ADB% %serial% shell appops set %PACKAGE_NAME% SYSTEM_ALERT_WINDOW allow >nul 2>&1

REM Enable accessibility service
echo Enabling accessibility service...
%ADB% %serial% shell settings put secure enabled_accessibility_services %PACKAGE_NAME%/.MouseAccessibilityService >nul 2>&1
%ADB% %serial% shell settings put secure accessibility_enabled 1 >nul 2>&1

REM Try to open accessibility settings for user to enable manually
echo Opening accessibility settings...
%ADB% %serial% shell am start -a android.settings.ACCESSIBILITY_SETTINGS >nul 2>&1

REM Wait a moment for settings to open
timeout /T 2 /NOBREAK >nul

REM Check if service is running
%ADB% %serial% shell pidof %PACKAGE_NAME% >nul 2>&1
if errorlevel 1 (
    echo.
    echo ========================================
    echo IMPORTANT: Manual Action Required
    echo ========================================
    echo.
    echo The accessibility service needs to be enabled manually.
    echo Please follow these steps on your device:
    echo.
    echo 1. Find "Mouse Cursor" in the accessibility settings
    echo 2. Toggle it ON
    echo 3. Confirm any permission dialogs
    echo.
    echo If you see permission errors, also grant:
    echo - Display over other apps permission
    echo.
    echo Press any key after enabling the service...
    pause >nul
)

REM Force stop first to ensure clean start
%ADB% %serial% shell am force-stop %PACKAGE_NAME% >nul 2>&1
timeout /T 1 /NOBREAK >nul

REM Start the app
%ADB% %serial% shell am start -n %PACKAGE_NAME%/%ACTIVITY_NAME% >nul 2>&1

REM Wait for app to start
echo Waiting for service to start...
set wait_count=0
:check_start
set /a wait_count+=1
if %wait_count% gtr 15 (
    echo Warning: Service may not have started properly
    echo Please check if accessibility service is enabled in settings
    goto :done
)

timeout /T 1 /NOBREAK >nul
%ADB% %serial% shell pidof %PACKAGE_NAME% >nul 2>&1
if errorlevel 1 (
    goto :check_start
)

echo.
echo ========================================
echo Virtual Mouse started successfully!
echo Port: %VMOUSE_PORT%
echo Package: %PACKAGE_NAME%
echo ========================================
goto :done

:error
echo.
echo ========================================
echo Error: Virtual Mouse setup failed
echo ========================================
echo.
echo Troubleshooting:
echo 1. Make sure USB debugging is enabled on your device
echo 2. Check if device is properly connected: adb devices
echo 3. Try running: adb kill-server ^&^& adb start-server
echo 4. Make sure %VMOUSE_APK% exists in current directory
echo.
exit /b 1

:done
echo.
exit /b 0
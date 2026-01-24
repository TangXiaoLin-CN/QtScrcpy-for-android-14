@echo off

echo Begin Runing...
set VMOUSE_PORT=9999
set VMOUSE_APK=vmouse.apk
set ADB=adb.exe

if not "%1"=="" (
    set serial=-s %1
)
if not "%2"=="" (
    set VMOUSE_PORT=%2
)

echo Waiting for device %serial...
%ADB% %serial% wait-for-device || goto :error
echo Find device %serial

for /f "delims=" %%i in ('%ADB% %serial% shell pm path com.chetbox.mousecursor') do set vmouse_installed=%%i
if "%vmouse_installed%"=="" (
    echo Install %VMOUSE_APK%... 
    %ADB% %serial% uninstall com.chetbox.mousecursor || echo uninstall failed
    %ADB% %serial% install -t -r -g %VMOUSE_APK% || goto :error
    echo Install %VMOUSE_APK% success
)

echo Forward port %serial% %VMOUSE_PORT% ...
%ADB% %serial% forward tcp:%VMOUSE_PORT% tcp:%VMOUSE_PORT% || goto :error

%ADB% %serial% shell am force-stop com.chetbox.mousecursor

echo Start %VMOUSE_APK%...
%ADB% %serial% shell am start com.chetbox.mousecursor/.MainActivity || goto :error

:check_start
echo Waiting %VMOUSE_APK% start...
::timeout /T 1 /NOBREAK > nul
%ADB% %serial% shell sleep 0.1
for /f "delims=" %%i in ("%ADB% shell ps | findstr com.chetbox.mousecursor") do set vmouse_started=%%i
if "%vmouse_started%"=="" (
    goto :check_start
)
echo %VMOUSE_APK% started...
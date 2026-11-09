@echo off
setlocal EnableExtensions
title MeshCrossway - Android ADB Backup
cd /d "%~dp0.." || exit /b 1

rem Python is needed for both full backup and verification.
set "PY=py -3"
py -3 --version >nul 2>&1
if errorlevel 1 (
  set "PY=python"
  python --version >nul 2>&1
  if errorlevel 1 (
    echo ERROR: Python 3.9 or newer is required.
    echo Download it from https://www.python.org/downloads/windows/
    pause
    exit /b 1
  )
)

if /I "%~1"=="verify" goto VERIFY
if not "%~1"=="" goto BADARG

rem No automatic downloads, root, cloud upload or APK uninstall.
set "ADB_BIN=adb"
if exist "%~dp0..\platform-tools\adb.exe" set "ADB_BIN=%~dp0..\platform-tools\adb.exe"
where "%ADB_BIN%" >nul 2>&1
if errorlevel 1 (
  echo.
  echo ERROR: Android Platform Tools / adb.exe was not found.
  echo Install Google's Platform Tools and add adb to PATH, or
  echo place platform-tools inside the project root.
  echo https://developer.android.com/tools/releases/platform-tools
  pause
  exit /b 1
)

echo ============================================================
echo MeshCrossway - ADB backup of the INSTALLED Android beta
echo ============================================================
echo 1. Unlock phone, enable USB debugging and connect USB.
echo 2. Authorize this computer on the phone when prompted.
echo 3. KEEP THE CURRENT APP INSTALLED during the backup.
echo 4. The utility briefly stops MeshCrossway to copy databases.
echo.
echo The ZIP is stored only on this PC; it contains private
echo messages and recorded GPS positions. Store it securely.
echo.
set "BACKUP_DIR=%USERPROFILE%\MeshCrossway-Backups"
if not exist "%BACKUP_DIR%" mkdir "%BACKUP_DIR%"
if errorlevel 1 (
  echo ERROR: Could not create "%BACKUP_DIR%"
  pause
  exit /b 1
)

%PY% "%~dp0migrate_android_debug_install.py" --adb "%ADB_BIN%" --output-dir "%BACKUP_DIR%"
set "RESULT=%ERRORLEVEL%"
echo.
if %RESULT% NEQ 0 (
  echo BACKUP FAILED. DO NOT UNINSTALL THE CURRENT APP.
) else (
  echo ZIP verified. Check "%BACKUP_DIR%".
  echo KEEP the current app until successful import in the new APK.
)
echo.
pause
exit /b %RESULT%

:VERIFY
if "%~2"=="" goto BADARG
%PY% "%~dp0migrate_android_debug_install.py" --verify-only "%~2"
set "RESULT=%ERRORLEVEL%"
echo.
pause
exit /b %RESULT%

:BADARG
echo Usage: MeshCrossway-Backup-Windows.cmd
echo        MeshCrossway-Backup-Windows.cmd verify "C:\path\backup.zip"
pause
exit /b 2

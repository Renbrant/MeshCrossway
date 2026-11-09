@echo off
setlocal EnableExtensions
title MeshCrossway - Android ADB Backup
cd /d "%~dp0.." || exit /b 1

rem Use an explicitly installed Android Platform Tools binary. No downloads,
rem root access, remote uploads, uninstall or automatic APK installation.
set "ADB_BIN=adb"
if exist "%~dp0..\platform-tools\adb.exe" set "ADB_BIN=%~dp0..\platform-tools\adb.exe"
where "%ADB_BIN%" >nul 2>&1
if errorlevel 1 (
  echo.
  echo ERROR: Android Platform Tools / adb.exe was not found.
  echo Install the official Google Platform Tools and add them to your PATH,
  echo or place the platform-tools folder in the project root.
  echo https://developer.android.com/tools/releases/platform-tools
  echo.
  pause
  exit /b 1
)

set "PY=py -3"
py -3 --version >nul 2>&1
if errorlevel 1 (
  set "PY=python"
  python --version >nul 2>&1
  if errorlevel 1 (
    echo ERROR: Python 3.9 or newer is required. Install Python for Windows.
    echo https://www.python.org/downloads/windows/
    pause
    exit /b 1
  )
)

if /I "%~1"=="verify" (
  if "%~2"=="" (
    echo Usage: MeshCrossway-Backup-Windows.cmd verify "C:\path\backup.zip"
    exit /b 2
  )
  %PY% "%~dp0migrate_android_debug_install.py" --verify-only "%~2"
  set "RESULT=%ERRORLEVEL%"
  echo.
  pause
  exit /b %RESULT%
)

if not "%~1"=="" (
  echo Unexpected argument. Run this file without arguments to back up,
  echo or use: MeshCrossway-Backup-Windows.cmd verify "C:\path\backup.zip"
  pause
  exit /b 2
)

echo ============================================================
echo  MeshCrossway - ADB full backup of the INSTALLED beta
echo ============================================================
echo  1. Unlock phone, enable USB debugging and connect USB.
echo  2. Authorize the computer on the phone when prompted.
echo  3. Keep the current app INSTALLED.
echo  4. We will temporarily stop MeshCrossway to copy databases.
echo.
echo  The backup is stored on your Windows PC, not in the app.
echo  It includes PRIVATE messages and GPS locations. Protect it.
echo.
set "BACKUP_DIR=%USERPROFILE%\MeshCrossway-Backups"
if not exist "%BACKUP_DIR%" mkdir "%BACKUP_DIR%"
if errorlevel 1 (
  echo ERROR: Could not create backup folder "%BACKUP_DIR%"
  pause
  exit /b 1
)

%PY% "%~dp0migrate_android_debug_install.py" --adb "%ADB_BIN%" --output-dir "%BACKUP_DIR%"
set "RESULT=%ERRORLEVEL%"
echo.
if %RESULT% NEQ 0 (
  echo Backup failed. DO NOT uninstall the current MeshCrossway app.
) else (
  echo Archive verification succeeded. Check this folder for the ZIP:
  echo "%BACKUP_DIR%"
  echo Keep the current app until the ZIP is imported and verified.
)
echo.
pause
exit /b %RESULT%

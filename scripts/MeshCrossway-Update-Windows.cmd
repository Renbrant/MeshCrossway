@echo off
setlocal EnableExtensions
title MeshCrossway - ADB Update Without Uninstall
cd /d "%~dp0.." || exit /b 1
if "%~1"=="" goto USAGE
set "APK=%~f1"
if not exist "%APK%" (
  echo ERROR: APK file not found: "%APK%"
  pause
  exit /b 2
)
set "ADB_BIN=adb"
if exist "%~dp0..\platform-tools\adb.exe" set "ADB_BIN=%~dp0..\platform-tools\adb.exe"
where "%ADB_BIN%" >nul 2>&1
if errorlevel 1 (
  echo ERROR: adb.exe not found. Install Google's official Android Platform Tools.
  echo https://developer.android.com/tools/releases/platform-tools
  pause
  exit /b 1
)

echo ============================================================
echo MeshCrossway - ADB in-place update
echo ============================================================
echo This uses adb install -r. It DOES NOT uninstall or clear data.
echo The update must have the SAME Android package/signing identity.
echo A mismatch will be rejected by Android, preserving the old app.
echo.
echo Connect and unlock exactly one phone, and authorize USB debugging.
echo.
"%ADB_BIN%" install -r "%APK%"
set "RESULT=%ERRORLEVEL%"
echo.
if %RESULT% NEQ 0 (
  echo UPDATE REJECTED. DO NOT UNINSTALL YOUR EXISTING APP.
  echo Check the signing certificate, versionCode and the backup first.
) else (
  echo APK installed over the existing app, without an uninstall.
  echo Reopen MeshCrossway and confirm tracks, favorites, maps and settings.
)
echo.
pause
exit /b %RESULT%

:USAGE
echo Drag a compatible signed MeshCrossway APK onto this .cmd file,
echo or run: MeshCrossway-Update-Windows.cmd "C:\path\MeshCrossway-0.2.4.apk"
echo Never uninstall to force an update after an ADB signing error.
pause
exit /b 2

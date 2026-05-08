@echo off
rem ============================================================
rem  REST API integration tests (Jest / supertest).
rem
rem  Mirrors .github/actions/rest-test/action.yaml so the same
rem  suite runs locally via tests\run-all-tests.bat. Expects to
rem  be invoked from the build/target folder containing nscp.exe;
rem  run-all-tests.bat sets that as the working directory.
rem
rem  Requires:
rem    * node + npm on PATH (Node 20+ matches CI)
rem    * a built nscp.exe in the current working directory
rem ============================================================
setlocal EnableDelayedExpansion

set "rest_dir=%~dp0"
set "rest_dir=%rest_dir:~0,-1%"
set "settings=%rest_dir%\nsclient.ini"
set "target_dir=%cd%"

where npm >nul 2>&1
if errorlevel 1 (
    echo ! npm not found on PATH; skipping REST tests.
    echo   Install Node.js 20+ to enable: https://nodejs.org/
    exit /b 0
)

if not exist "%target_dir%\nscp.exe" (
    echo ! nscp.exe not found in %target_dir%; cannot run REST tests.
    exit /b 1
)

echo ------------------------------
echo Installing REST test deps
echo ------------------------------
pushd "%rest_dir%" >nul
call npm install
set "rc=!errorlevel!"
popd >nul
if not "!rc!"=="0" (
    echo ! npm install failed with exit code !rc!
    exit /b 1
)

echo ------------------------------
echo Starting nscp.exe (background)
echo ------------------------------
if exist "%target_dir%\nsclient.log" del /q "%target_dir%\nsclient.log"
start "nscp-rest-test" /b nscp test --settings "%settings%"

rem Give the WEBServer a moment to bind 8443 before Jest connects.
timeout /t 3 /nobreak >nul

echo ------------------------------
echo Running Jest suite
echo ------------------------------
pushd "%rest_dir%" >nul
call npm run test
set "rc=!errorlevel!"
popd >nul

echo ------------------------------
echo Stopping nscp.exe
echo ------------------------------
taskkill /F /im nscp.exe >nul 2>&1

if exist "%target_dir%\nsclient.log" (
    echo --- nsclient.log ---
    type "%target_dir%\nsclient.log"
    echo --- end log ---
)

if not "!rc!"=="0" (
    echo ! REST tests FAILED with exit code !rc!
    exit /b 1
)
echo + REST tests OK
exit /b 0

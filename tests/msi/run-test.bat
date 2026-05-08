@echo off
rem ============================================================
rem  MSI installer integration tests.
rem
rem  Mirrors .github/actions/installer-test/action.yaml so the
rem  same suite runs locally via tests\run-all-tests.bat.
rem
rem  Requires:
rem    * Python 3 on PATH
rem    * Built MSI(s) under installers\installer-NSCP\ relative
rem      to the current working directory (typically the
rem      build/target folder)
rem    * Administrator privileges (the script installs and
rem      uninstalls the MSI)
rem
rem  When the MSI artifacts are not present (e.g. running the
rem  test bundle on a developer machine that didn't build them)
rem  the suite is skipped with a clear message rather than
rem  failed, so run-all-tests.bat stays green for partial builds.
rem ============================================================
setlocal EnableDelayedExpansion

set "msi_dir=%~dp0"
set "msi_dir=%msi_dir:~0,-1%"
set "target_dir=%cd%"

where python >nul 2>&1
if errorlevel 1 (
    echo ! python not found on PATH; skipping MSI installer tests.
    exit /b 0
)

if not exist "%target_dir%\installers\installer-NSCP\*.msi" (
    echo - No MSI files under %target_dir%\installers\installer-NSCP; skipping MSI installer tests.
    exit /b 0
)

echo ------------------------------
echo Installing MSI test deps
echo ------------------------------
pushd "%msi_dir%" >nul
python -m pip install -r requirements.txt
set "rc=!errorlevel!"
popd >nul
if not "!rc!"=="0" (
    echo ! pip install failed with exit code !rc!
    exit /b 1
)

echo ------------------------------
echo Running MSI installer tests
echo ------------------------------
python "%msi_dir%\test-install.py"
set "rc=!errorlevel!"

if not "!rc!"=="0" (
    echo ! MSI installer tests FAILED with exit code !rc!
    exit /b 1
)
echo + MSI installer tests OK
exit /b 0

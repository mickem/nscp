@echo off
rem ============================================================
rem  Runs every test batch file under tests\ in sequence.
rem
rem  Picks up:
rem    * tests\acceptance-tests.bat
rem    * tests\<sub>\run-test.bat   (one per protocol/feature)
rem
rem  Every test is invoked from the *current working directory*
rem  at the time this script is launched, which is expected to
rem  be the build/target folder containing nscp.exe (the test
rem  scripts call "nscp ..." without a path).
rem
rem  The script keeps going after a failure so we get a full
rem  report, but exits non-zero if any individual test failed.
rem
rem  Usage (from the build/target folder containing nscp.exe):
rem    <path-to>\tests\run-all-tests.bat           run everything
rem    <path-to>\tests\run-all-tests.bat --ci      pass --ci
rem                                                through to
rem                                                acceptance-tests.bat
rem ============================================================
setlocal EnableDelayedExpansion

set "script_folder=%~dp0"
set "self=%~f0"
set "args=%*"

rem The folder the user launched us from is treated as the
rem build/target folder. All tests are executed with this as
rem their working directory so "nscp" resolves to the local
rem nscp.exe and any relative paths the tests create (spool
rem dirs, generated configs, etc.) end up alongside it.
set "target_dir=%cd%"

if not exist "%target_dir%\nscp.exe" (
    echo WARNING: nscp.exe not found in %target_dir%.
    echo          Tests expect to be launched from the build/target folder
    echo          containing nscp.exe.
)

set /a total=0
set /a failed=0
set "failed_list="

call :run_test "%script_folder%acceptance-tests.bat" %args%

rem Discover and run every run-test.bat in any direct or nested
rem subdirectory of the tests folder.
for /r "%script_folder%" %%f in (run-test.bat) do (
    call :run_test "%%~ff"
)

echo ============================================================
echo  Test summary: !total! run, !failed! failed
if !failed! gtr 0 (
    echo  Failed: !failed_list!
)
echo ============================================================

if !failed! gtr 0 exit /b 1
exit /b 0


:run_test
rem %1 = full path to .bat file, %2.. = optional args to pass
set "test_script=%~1"
if /i "%test_script%"=="%self%" goto :eof
if not exist "%test_script%" goto :eof

set /a total+=1
echo ============================================================
echo  Running: %test_script%
echo  From   : %target_dir%
echo ============================================================

pushd "%target_dir%" >nul
call "%test_script%" %2 %3 %4 %5 %6 %7 %8 %9
set "rc=!errorlevel!"
popd >nul

if not "!rc!"=="0" (
    set /a failed+=1
    set "failed_list=!failed_list! %~nx1 [%~dp1]"
    echo ! %test_script% FAILED with exit code !rc!
) else (
    echo + %test_script% OK
)
goto :eof


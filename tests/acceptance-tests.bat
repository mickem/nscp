@echo off
setlocal enabledelayedexpansion

set CI_MODE=0
if "%1"=="--ci" set CI_MODE=1

echo Running NSCA tests...
for %%c in (none xor des 3des cast128 xtea blowfish twofish rc2 aes aes256 aes192 aes128 serpent gost 3way) do (
    echo Running test_nsca case: %%c
    call :retry_unit --language python --script test_nsca --case %%c
    if errorlevel 1 goto :failed
)

echo Running NRPE tests...
call :retry_unit --language python --script test_nrpe
if errorlevel 1 goto :failed

rem echo Running Lua NRPE tests...
rem call :retry_unit --language lua --script test_nrpe.lua --log error
rem if errorlevel 1 goto :failed

echo Running Python tests...
call :retry_unit --language python --script test_python
if errorlevel 1 goto :failed

echo Running Log File tests...
call :retry_unit --language python --script test_log_file
if errorlevel 1 goto :failed

echo Running External Script tests...
call :retry_unit --language python --script test_external_script
if errorlevel 1 goto :failed

echo Running Scheduler tests...
call :retry_unit --language python --script test_scheduler
if errorlevel 1 goto :failed

echo Running Windows System tests...
call :retry_unit --language python --script test_w32_file
if errorlevel 1 goto :failed

echo Running Windows EventLog tests...
call :retry_unit --language python --script test_eventlog
if errorlevel 1 goto :failed

echo Running Windows Task Scheduler tests...
call :retry_unit --language python --script test_w32_schetask
if errorlevel 1 goto :failed

if "%CI_MODE%"=="1" goto :skip_w32_system
echo Running Windows System tests...
call :retry_unit --language python --script test_w32_system
if errorlevel 1 goto :failed
goto :done_w32_system

:skip_w32_system
echo Skipping Windows System tests [not compatible with CI]...

:done_w32_system

echo All tests passed successfully.
exit /b 0

rem ---------------------------------------------------------------------------
rem :retry_unit
rem   Run "nscp unit <args...>" up to 2 times, only failing if both attempts
rem   fail. This neutralises transient socket / handshake races in
rem   network-sensitive tests (test_nsca cipher cases, test_nrpe port binds).
rem ---------------------------------------------------------------------------
:retry_unit
nscp unit %*
if not errorlevel 1 exit /b 0
echo [retry_unit] attempt 1 of 2 failed for: nscp unit %* 1>&2
nscp unit %*
if not errorlevel 1 exit /b 0
echo [retry_unit] attempt 2 of 2 failed for: nscp unit %* 1>&2
exit /b 1

:failed
echo Tests failed.
exit /b 1

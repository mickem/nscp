@echo off
setlocal

set CI_MODE=0
if "%1"=="--ci" set CI_MODE=1

echo Running NSCA tests...
for %%c in (none xor des 3des cast128 xtea blowfish twofish rc2 aes aes256 aes192 aes128 serpent gost 3way) do (
    echo Running test_nsca case: %%c
    nscp unit --language python --script test_nsca --case %%c
    if errorlevel 1 goto :failed
)

echo Running NRPE tests...
nscp unit --language python --script test_nrpe
if errorlevel 1 goto :failed

rem echo Running Lua NRPE tests...
rem nscp unit --language lua --script test_nrpe.lua --log error
rem if errorlevel 1 goto :failed

echo Running Python tests...
nscp unit --language python --script test_python
if errorlevel 1 goto :failed

echo Running Log File tests...
nscp unit --language python --script test_log_file
if errorlevel 1 goto :failed

echo Running External Script tests...
nscp unit --language python --script test_external_script
if errorlevel 1 goto :failed

echo Running Scheduler tests...
nscp unit --language python --script test_scheduler
if errorlevel 1 goto :failed

echo Running Windows System tests...
nscp unit --language python --script test_w32_file
if errorlevel 1 goto :failed

echo Running Windows Task Scheduler tests...
nscp unit --language python --script test_w32_schetask
if errorlevel 1 goto :failed

if "%CI_MODE%"=="1" goto :skip_w32_system
echo Running Windows System tests...
nscp unit --language python --script test_w32_system
if errorlevel 1 goto :failed
goto :done_w32_system

:skip_w32_system
echo Skipping Windows System tests [not compatible with CI]...

:done_w32_system

echo All tests passed successfully.
exit /b 0

:failed
echo Tests failed.
exit /b 1
@echo off
setlocal

set CI_MODE=0
if "%1"=="--ci" set CI_MODE=1

echo Running Python tests...
nscp unit --language python --script test_python
if errorlevel 1 goto :failed

echo Running Windows System tests...
nscp unit --language python --script test_w32_file
if errorlevel 1 goto :failed

echo Running Windows WMI tests...
nscp unit --language python --script test_w32_wmi
if errorlevel 1 goto :failed

echo Running Windows EventLog tests...
nscp unit --language python --script test_eventlog
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
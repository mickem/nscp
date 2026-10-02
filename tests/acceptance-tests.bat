@echo off
setlocal

rem --ci is still accepted: the Windows integration workflow passes it.

echo Running Python tests...
nscp unit --language python --script test_python
if errorlevel 1 goto :failed

echo All tests passed successfully.
exit /b 0

:failed
echo Tests failed.
exit /b 1

@echo off
rem Shipped by the `faults` role bundle and run by the agent as
rem scripts\fault.bat <fault>: hands over to fault.ps1 next to it.
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0fault.ps1" %1
exit /b %ERRORLEVEL%

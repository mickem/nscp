@echo off
ping -n 1 %1 -w 20000 >NUL
IF ERRORLEVEL 1 GOTO err
IF ERRORLEVEL 0 GOTO ok
GOTO unknown
 
:err
echo CRITICAL: Ping check failed
exit /B 1
 
:unknown
echo UNKNOWN: Something went wrong
exit /B 3
 
:ok
echo OK: Ping succeded
exit /B 0
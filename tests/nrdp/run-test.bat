@echo off
set script_folder=%~dp0
set current_dir=%cd%
goto :start

:test_submit
set CHECK_NAME=%1
set RESULT_CODE=%2
set MESSAGE=%~3
echo -----------------------------------
echo Submitting %CHECK_NAME% (result=%RESULT_CODE%)
echo -----------------------------------

nscp nrdp --address http://127.0.0.1:8080/nrdp/server/ --token=change_me --command %CHECK_NAME% --result %RESULT_CODE% --message "%MESSAGE%"
if not %errorlevel%==0 (
  echo ! NRDP submission failed, Error level was: %errorlevel%
  exit /b 1
)

rem Verify that the NRDP server has spooled a check result file containing our payload
findstr /s /m /c:"%CHECK_NAME%" %current_dir%\nrdp_test\* >nul
if not %errorlevel%==0 (
  echo ! Result for %CHECK_NAME% not found in spooled check results
  dir %current_dir%\nrdp_test
  exit /b 1
)
findstr /s /m /c:"%MESSAGE%" %current_dir%\nrdp_test\* >nul
if not %errorlevel%==0 (
  echo ! Message for %CHECK_NAME% not found in spooled check results
  exit /b 1
)

goto :eof

:start

if not exist nrdp_test mkdir nrdp_test

echo ------------------
echo Building container
echo ------------------

docker build -t nrdp_server -f %script_folder%Dockerfile %script_folder%
if not %errorlevel%==0 (
  echo ! Failed to build nrdp_server docker image, Error level was: %errorlevel%
  exit /b 1
)

del /q %current_dir%\nrdp_test\* >nul 2>nul

echo ------------------
echo Starting container
echo ------------------

docker run -d --rm --volume %current_dir%\nrdp_test:/nrdp/checkresults -p 8080:80 --name nrdp_server -e TOKEN=change_me nrdp_server
if not %errorlevel%==0 (
  echo ! Failed to start nrdp_server container, Error level was: %errorlevel%
  exit /b 1
)

rem Give Apache a moment to come up
timeout /t 5 /nobreak >nul

echo --------------------------------
echo Submitting passive check results
echo --------------------------------

call :test_submit ok-check 0 "Everything is fine"
if not %errorlevel%==0 goto :failed
call :test_submit warning-check 1 "Slightly worried"
if not %errorlevel%==0 goto :failed
call :test_submit critical-check 2 "Houston we have a problem"
if not %errorlevel%==0 goto :failed
call :test_submit unknown-check 3 "No idea"
if not %errorlevel%==0 goto :failed

echo -------------------------------
echo Testing invalid token rejection
echo -------------------------------
nscp nrdp --address http://127.0.0.1:8080/nrdp/server/ --token=wrong_token --command bad-token-check --result 0 --message "should not be accepted"
if %errorlevel%==0 (
  echo ! NRDP submission with invalid token unexpectedly succeeded
  goto :failed
)
findstr /s /m /c:"bad-token-check" %current_dir%\nrdp_test\* >nul
if %errorlevel%==0 (
  echo ! Result for bad-token-check unexpectedly found in spooled check results
  goto :failed
)

docker stop nrdp_server >nul 2>nul

echo -----------------
echo All tests passed
echo -----------------
exit /b 0

:failed
docker stop nrdp_server >nul 2>nul
echo -----------------
echo Tests failed
echo -----------------
exit /b 1

@echo off
set script_folder=%~dp0
set current_dir=%cd%
goto :start

:test_encryption_method
set ENCRYPTION_METHOD=%1
set ENCRYPTION_METHOD_INT=%2
echo -----------------------------------
echo Testing encryption method: %ENCRYPTION_METHOD% / %ENCRYPTION_METHOD_INT%
echo -----------------------------------

docker run -d --rm --volume %current_dir%\nsca_test:/nsca -p 5667:5667 --name nsca_server -e ENCRYPTION_METHOD=%ENCRYPTION_METHOD_INT% nsca_server
timeout /t 3 /nobreak >nul

nscp nsca --host=127.0.0.1 --password=change_me --encryption %ENCRYPTION_METHOD% --command encryption-%ENCRYPTION_METHOD%-%ENCRYPTION_METHOD_INT% --result 2
if not %errorlevel%==0 (
  echo ! NSCA submission failed, Error level was: %errorlevel%
  docker stop nsca_server
  exit /b 1
)
docker stop nsca_server

findstr /c:"encryption-%ENCRYPTION_METHOD%-%ENCRYPTION_METHOD_INT%;2" %current_dir%\nsca_test\results.txt
if not %errorlevel%==0 (
  echo ! Result for encryption method %ENCRYPTION_METHOD% not found in results.txt
  exit /b 1
)

goto :eof

:start

mkdir nsca_test

echo ------------------
echo Building container
echo ------------------

docker build -t nsca_server -f %script_folder%Dockerfile %script_folder%
if not %errorlevel%==0 (
  echo ! Failed to build nsca_server docker image, Error level was: %errorlevel%
  exit /b 1
)

del %current_dir%\nsca_test\results.txt >nul 2>nul


call :test_encryption_method none 0
if not %errorlevel%==0 exit /b 1
call :test_encryption_method xor 1
if not %errorlevel%==0 exit /b 1
call :test_encryption_method des 2
if not %errorlevel%==0 exit /b 1
call :test_encryption_method 3des 3
if not %errorlevel%==0 exit /b 1
call :test_encryption_method cast128 4
if not %errorlevel%==0 exit /b 1
call :test_encryption_method xtea 6
if not %errorlevel%==0 exit /b 1
rem 3way is not compatible between libmcrypt and nscp
rem call :test_encryption_method 3way 7
rem if not %errorlevel%==0 exit /b 1
call :test_encryption_method blowfish 8
if not %errorlevel%==0 exit /b 1
call :test_encryption_method twofish 9
if not %errorlevel%==0 exit /b 1
call :test_encryption_method rc2 11
if not %errorlevel%==0 exit /b 1
rem NSCA use (lib mcrypt) which use a non standard key size for rijndael
rem Thus we use aes256 to match 14 (aes 128). 15 and 16 are not supported
call :test_encryption_method aes256 14
if not %errorlevel%==0 exit /b 1
call :test_encryption_method serpent 20
if not %errorlevel%==0 exit /b 1
rem ghost is not compatible between libmcrypt and nscp
call :test_encryption_method gost 23
if not %errorlevel%==0 exit /b 1

echo -----------------
echo All tests passed
echo -----------------
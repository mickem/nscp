@ECHO OFF
SET ROOT=B:\master
SET SOURCE=c:\source\master

GOTO :start

:set_error
SET ERROR_NO=%1
SET ERROR_MSG=%1
echo ***********************************
echo * %ERROR_NO% - %ERROR_MSG%
echo ***********************************
GOTO :EOF

:mk_dirs
SETLOCAL
SET ENV=%1
echo Creating folders for %ENV%
title Creating folders for %ENV%
if not exist "%ROOT%" mkdir %ROOT%
if not exist "%ROOT%\%ENV%" mkdir %ROOT%\%ENV%
if not exist "%ROOT%\%ENV%\dist" mkdir %ROOT%\%ENV%\dist
ENDLOCAL
GOTO :EOF

:git_pull
SETLOCAL
ECHO Fetching code from server
title Fetching code from server
cd /D %SOURCE%
git checkout master installers/common/re-generate.bat
if %ERRORLEVEL% == 1 goto :error
git pull
if %ERRORLEVEL% == 1 goto :error
ENDLOCAL
GOTO :EOF

:git_push
SETLOCAL
ECHO Pushing code to server
title Pushing code to server
cd /D %SOURCE%
ECHO "+ Restore invalid files"
git checkout master installers/common/re-generate.bat
del docs\samples\index.rst
if %ERRORLEVEL% == 1 goto :error
ECHO "+ Add version.txt"
git add version.txt
if %ERRORLEVEL% == 1 goto :error
ECHO "+ Committing..."
git commit -m "Bumped version"
ECHO "+ Updating docs"
git add docs/*
if %ERRORLEVEL% == 1 goto :error
ECHO "+ Committing..."
git commit -m "Updated docs"
ECHO "+ Pushing..."
git push
if %ERRORLEVEL% == 1 goto :error
ENDLOCAL
GOTO :EOF

git checkout master installers\common\re-generate.bat
if %ERRORLEVEL% == 1 goto :error
git pull
if %ERRORLEVEL% == 1 goto :error
ENDLOCAL
GOTO :EOF

:bump_version
SETLOCAL
SET ENV=%1
SET GENERATOR=%2
ECHO Bumping version %ENV% (%GENERATOR%)
title Bumping version %ENV% (%GENERATOR%)
if %ERRORLEVEL% == 1 goto :error
cd /D %ROOT%\%ENV%\dist
cmake -D INCREASE_BUILD=1 -G %GENERATOR% -T v110_xp %SOURCE%
if %ERRORLEVEL% == 1 goto :error
ENDLOCAL
GOTO :EOF

:build
SETLOCAL
SET ENV=%1
SET GENERATOR=%2
ECHO Building %ENV% (%GENERATOR%)
title Building %ENV% (%GENERATOR%)
if %ERRORLEVEL% == 1 goto :error
msbuild /p:Configuration=RelWithDebInfo /p:Platform=%GENERATOR% %ROOT%\%ENV%\dist\NSCP.sln
if %ERRORLEVEL% == 1 goto :error
ECHO Packaging %ENV%
title Packaging %ENV%
cd /D %ROOT%\%ENV%\dist
cpack
if %ERRORLEVEL% == 1 goto :error
ENDLOCAL
GOTO :EOF


:post_build
SETLOCAL
SET ENV=%1
ECHO Post build %ENV%
title Post build %ENV%
if %ERRORLEVEL% == 1 goto :error
cd /D %ROOT%\%ENV%\dist
call %ROOT%\%ENV%\dist\postbuild.bat
if %ERRORLEVEL% == 1 goto :error
ENDLOCAL
GOTO :EOF

:run_test
SETLOCAL
SET ENV=%1
ECHO Running tests %ENV%
title Running tests %ENV%
cd /D %ROOT%\%ENV%\dist
ctest -C RelWithDebInfo --output-on-failure
if %ERRORLEVEL% == 1 goto :error
ENDLOCAL
GOTO :EOF


:configure
SETLOCAL
SET ENV=%1
SET GENERATOR=%2
title Configuring %ENV% using %GENERATOR% (%ROOT%)
ECHO Configuring %ENV% using %GENERATOR% (%ROOT%)
if %ERRORLEVEL% == 1 goto :error
cd /D %ROOT%\%ENV%\dist
cmake -D INCREASE_BUILD=0 -G %GENERATOR% -T v110_xp -B %ROOT%\%ENV%\dist %SOURCE%
if %ERRORLEVEL% == 1 goto :error
ENDLOCAL
GOTO :EOF


:start

call :mk_dirs x64
call :mk_dirs w32

IF "%1"=="post" GOTO do_post_build
IF "%1"=="build" GOTO only_build
IF "%1"=="same" GOTO no_bump

IF "%1"=="nogit" GOTO :no_git_1
call :git_pull || GOTO :error
:no_git_1

call :bump_version x64 "Visual Studio 11 Win64" || GOTO :error
:no_bump

call :configure x64 "Visual Studio 11 Win64" || GOTO :error
call :configure w32 "Visual Studio 11" || GOTO :error

:only_build
call :build x64 x64 || GOTO :error
call :build w32 Win32 || GOTO :error

IF "%1"=="nogit" GOTO :no_git_2
call :git_push || GOTO :error
:no_git_2

:do_post_build
call :post_build x64 || GOTO :error
call :post_build w32 Win32 || GOTO :error

call :run_test x64 || GOTO :error
rem call :run_test w32 || GOTO :error

title Done!

exit /b 0
goto :eof

:error
echo ***********************************
echo * Failed to build NSClient++: %ERRORLEVEL% *
echo ***********************************

exit /b 1
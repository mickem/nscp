@ECHO OFF
SET ROOT=c:\source\mbuild
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
mkdir %ROOT%
mkdir %ROOT%\%ENV%
mkdir %ROOT%\%ENV%\dist
ENDLOCAL
GOTO :EOF

:bump_version
SETLOCAL
SET ENV=%1
SET GENERATOR=%2
ECHO Bumping version %ENV% (%GENERATOR%)
title Bumping version %ENV% (%GENERATOR%)
cd %ROOT%\%ENV%\dist
if %ERRORLEVEL% == 1 goto :error
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
cd %ROOT%\%ENV%\dist
if %ERRORLEVEL% == 1 goto :error
msbuild /p:Configuration=RelWithDebInfo /p:Platform=%GENERATOR% NSCP.sln
if %ERRORLEVEL% == 1 goto :error
ECHO Packaging %ENV%
title Packaging %ENV%
cpack
if %ERRORLEVEL% == 1 goto :error
ENDLOCAL
GOTO :EOF


:post_build
SETLOCAL
SET ENV=%1
ECHO Post build %ENV%
title Post build %ENV%
cd %ROOT%\%ENV%\dist
if %ERRORLEVEL% == 1 goto :error
call postbuild.bat
if %ERRORLEVEL% == 1 goto :error
ENDLOCAL
GOTO :EOF


:configure
SETLOCAL
SET ENV=%1
SET GENERATOR=%2
title Configuring %ENV% using %GENERATOR% (%ROOT%)
ECHO Configuring %ENV% using %GENERATOR% (%ROOT%)
cd %ROOT%\%ENV%\dist
if %ERRORLEVEL% == 1 goto :error
cmake -D INCREASE_BUILD=0 -G %GENERATOR% -T v110_xp %SOURCE%
if %ERRORLEVEL% == 1 goto :error
ENDLOCAL
GOTO :EOF


:start

call :mk_dirs x64
call :mk_dirs w32

IF "%1"=="build" GOTO only_build

IF "%1"=="same" GOTO no_bump
call :bump_version x64 "Visual Studio 11 2012 Win64" || GOTO :error
:no_bump

call :configure x64 "Visual Studio 11 2012 Win64" || GOTO :error
call :configure w32 "Visual Studio 11 2012" || GOTO :error

:only_build
call :build x64 x64 || GOTO :error
call :build w32 Win32 || GOTO :error

call :post_build x64 || GOTO :error
call :post_build w32 Win32 || GOTO :error

title Done!

exit /b 0
goto :eof

:error
echo ***********************************
echo * Failed to build NSClient++: %ERRORLEVEL% *
echo ***********************************

exit /b 1
@ECHO OFF
SET ROOT=D:\source\build
SET SOURCE=D:\source\nscp

GOTO :start

:mk_dirs
SETLOCAL
SET ENV=%1
ECHO Creating folders for %ENV%
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

:mk_dirs x64
:mk_dirs w32

IF "%1"=="same" GOTO no_bump
call :bump_version x64 "Visual Studio 11 Win64"
:no_bump

call :configure x64 "Visual Studio 11 Win64"
call :configure w32 "Visual Studio 11"

call :build x64 x64
call :build w32 Win32

call :post_build x64
call :post_build w32 Win32

title Done!

exit /b 0
goto :eof

:error
echo ***********************************
echo * Failed to build NSClient++: %ERRORLEVEL% *
echo ***********************************

exit /b 1
@ECHO OFF
SET ROOT=D:\source\build
SET SOURCE=D:\source\nscp

mkdir %ROOT%
mkdir %ROOT%\x64
mkdir %ROOT%\w32
mkdir %ROOT%\x64\dist
mkdir %ROOT%\w32\dist

cd %ROOT%\x64\dist
if %ERRORLEVEL% == 1 goto :error

title Generating x64 
IF "%1"=="same" GOTO no_bump
echo "Bumping version"
cmake -D INCREASE_BUILD=1 -G "Visual Studio 11 Win64" -T v110_xp %SOURCE%
if %ERRORLEVEL% == 1 goto :error
goto done_cmake
:no_bump
cmake -D INCREASE_BUILD=0 -G "Visual Studio 11 Win64" -T v110_xp %SOURCE%
if %ERRORLEVEL% == 1 goto :error
:done_cmake

title Building x64 
msbuild /p:Configuration=RelWithDebInfo /p:Platform=x64 NSCP.sln
if %ERRORLEVEL% == 1 goto :error

title Packaging x64 
cpack
if %ERRORLEVEL% == 1 goto :error

title Postbuild x64
postbuild.py
if %ERRORLEVEL% == 1 goto :error

cd %ROOT%\w32\dist
if %ERRORLEVEL% == 1 goto :error

title Generating w32
cmake -D INCREASE_BUILD=0 -G "Visual Studio 11" -T v110_xp %SOURCE%
if %ERRORLEVEL% == 1 goto :error

title Building w32
msbuild /p:Configuration=RelWithDebInfo /p:Platform=Win32 NSCP.sln
if %ERRORLEVEL% == 1 goto :error

title Packaging w32
cpack
if %ERRORLEVEL% == 1 goto :error

title Postbuild w32
postbuild.py
if %ERRORLEVEL% == 1 goto :error

title Done!

exit /b 0
goto :eof

:error
echo ***********************************
echo * Failed to build NSClient++: %ERRORLEVEL% *
echo ***********************************

exit /b 1
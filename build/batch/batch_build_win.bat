@ECHO OFF
SET ROOT=d:\source\nscp\build

mkdir %ROOT%
mkdir %ROOT%\x64
mkdir %ROOT%\w32

cd %ROOT%\x64
if %ERRORLEVEL% == 1 goto :error

title Generating x64 
cmake -D INCREASE_BUILD=1 -G "Visual Studio 8 2005 Win64" ../../trunk
cmake -D INCREASE_BUILD=0 -G "Visual Studio 8 2005 Win64" ../../trunk
if %ERRORLEVEL% == 1 goto :error

title Building x64 
msbuild /p:Configuration=RelWithDebInfo NSCP.sln
if %ERRORLEVEL% == 1 goto :error

title Packaging x64 
cpack
if %ERRORLEVEL% == 1 goto :error

title Postbuild x64
postbuild.py
if %ERRORLEVEL% == 1 goto :error

cd %ROOT%\w32
if %ERRORLEVEL% == 1 goto :error

title Generating w32
cmake -D INCREASE_BUILD=0 -G "Visual Studio 8 2005" ../../trunk
cmake -D INCREASE_BUILD=0 -G "Visual Studio 8 2005" ../../trunk
if %ERRORLEVEL% == 1 goto :error

title Building w32
msbuild /p:Configuration=RelWithDebInfo NSCP.sln
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
@echo off
SET jam=D:\source\boost-jam-3.1.17\bin.ntx86\bjam.exe
%jam% --toolset=msvc --with-lua --with-openssl=%openssl% --with-boost=%boost% %* build-binaries
if %ERRORLEVEL% == 1 goto :error

%jam% --toolset=msvc --with-lua --with-openssl=%openssl% --with-boost=%boost% %* build-archives
if %ERRORLEVEL% == 1 goto :error

d:\tools\bjam.exe --toolset=wix %* build-installer
if %ERRORLEVEL% == 1 goto :error

exit /b 1
goto :eof

:error
echo *************
echo * E R R O R *
echo *************

exit /b -1
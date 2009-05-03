@echo off
SET jam=D:\source\boost-jam-3.1.17\bin.ntx86\bjam.exe

SET cmdline=%jam% --toolset=msvc --with-lua --with-openssl=%openssl% --with-boost=%boost% --with-cryptopp %* build-binaries
%jam% --toolset=msvc --with-lua --with-openssl=%openssl% --with-boost=%boost% --with-cryptopp %* build-binaries
if %ERRORLEVEL% == 1 goto :error

SET cmdline=%jam% --toolset=msvc --with-lua --with-openssl=%openssl% --with-boost=%boost% --with-cryptopp %* build-archives
%jam% --toolset=msvc --with-lua --with-openssl=%openssl% --with-boost=%boost% --with-cryptopp %* build-archives
if %ERRORLEVEL% == 1 goto :error

SET cmdline=%jam% --toolset=wix %* build-installer
%jam% --toolset=wix %* build-installer
if %ERRORLEVEL% == 1 goto :error

exit /b 1
goto :eof

:error
echo *************
echo * E R R O R *
echo *************
echo %cmdline%
echo *************

exit /b -1
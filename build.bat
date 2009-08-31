@echo off
@call env.bat

SET cmdline=%jam% --toolset=msvc --with-lua=%LUA_SOURCE% --with-openssl --include-path=%NSCP_INCLUDE% --with-boost --with-cryptopp=%CRYPTOPP_SOURCE% %* build-binaries
%jam% --toolset=msvc --with-lua=%LUA_SOURCE% --with-openssl --include-path=%NSCP_INCLUDE% --with-boost --with-cryptopp=%CRYPTOPP_SOURCE% warnings=off %* build-binaries
if %ERRORLEVEL% == 1 goto :error

SET cmdline=%jam% --toolset=msvc --with-lua=%LUA_SOURCE% --with-openssl --include-path=%NSCP_INCLUDE% --with-boost --with-cryptopp=%CRYPTOPP_SOURCE% warnings=off %* build-archives
%jam% --toolset=msvc --with-lua=%LUA_SOURCE% --with-openssl --include-path=%NSCP_INCLUDE% --with-boost --with-cryptopp=%CRYPTOPP_SOURCE% warnings=off %* build-archives
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
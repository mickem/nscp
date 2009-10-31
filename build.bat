@echo off
@call env.bat

echo :: %jam% --toolset=msvc --with-lua=%LUA_SOURCE% --with-openssl --include-path=%NSCP_INCLUDE% --with-boost --with-cryptopp=%CRYPTOPP_SOURCE% runtime-link=static warnings=off --with-psdk="%PLATTFORM_SDK_INCLUDE%" %* build-binaries >> build.log
%jam% --toolset=msvc --with-lua=%LUA_SOURCE% --with-openssl --include-path=%NSCP_INCLUDE% --with-boost --with-cryptopp=%CRYPTOPP_SOURCE% runtime-link=static warnings=off --with-psdk="%PLATTFORM_SDK_INCLUDE%" %* build-binaries
if %ERRORLEVEL% == 1 goto :error
echo :: Result: %ERRORLEVEL% >> build.log

echo :: %jam% --toolset=msvc warnings=off %* build-archives >> build.log
%jam% --toolset=msvc warnings=off %* build-archives
if %ERRORLEVEL% == 1 goto :error
echo :: Result: %ERRORLEVEL% >> build.log

echo :: %jam% --toolset=wix "--wix=%WIX_PATH%" %1=%2 %3=%4 build-installer >> build.log
%jam% --toolset=wix "--wix=%WIX_PATH%" %1=%2 %3=%4 build-installer
if %ERRORLEVEL% == 1 goto :error
echo :: Result: %ERRORLEVEL% >> build.log

exit /b 0
goto :eof

:error
echo :: Error: %ERRORLEVEL% >> build.log
echo *************
echo * E R R O R *
echo *************

exit /b -1
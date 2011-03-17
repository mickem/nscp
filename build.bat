@echo off
@call env.bat
set toolset=msvc-8.0

echo :: %jam% -j6 --toolset=%toolset% --with-lua=%LUA_SOURCE% --with-openssl --include-path=%NSCP_INCLUDE% --with-boost --with-cryptopp=%CRYPTOPP_SOURCE% runtime-link=static warnings=off "--with-psdk=%PLATTFORM_SDK_INCLUDE%" "--with-psdk61=%PLATTFORM_SDK_61_INCLUDE%" "--with-breakpad=%GOOGLE_BREAKPAD_INCLUDE%" %* build-binaries >> build.log
        %jam% -j6 --toolset=%toolset% --with-lua=%LUA_SOURCE% --with-openssl --include-path=%NSCP_INCLUDE% --with-boost --with-cryptopp=%CRYPTOPP_SOURCE% runtime-link=static warnings=off "--with-psdk=%PLATTFORM_SDK_INCLUDE%" "--with-psdk61=%PLATTFORM_SDK_61_INCLUDE%" "--with-breakpad=%GOOGLE_BREAKPAD_INCLUDE%" %* build-binaries
if %ERRORLEVEL% == 1 goto :error
echo :: Result: %ERRORLEVEL% >> build.log

echo :: %jam% --toolset=%toolset% warnings=off %* build-archives >> build.log
        %jam% --toolset=%toolset% warnings=off %* build-archives
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
@echo off

@call env.bat

SET version=crypto_hdr
SET cmdline=%jam% --toolset=msvc --with-cryptopp=%CRYPTOPP_SOURCE% --with-cryptopp-target=%NSCP_INCLUDE%\cryptopp cryptopp-headers
%jam% --toolset=msvc --with-cryptopp=%CRYPTOPP_SOURCE% --with-cryptopp-target=%NSCP_INCLUDE%\cryptopp cryptopp-headers
if %ERRORLEVEL% == -1 goto :error

REM build x86 binary
SET version=x86
SET cmdline=build.bat runtime-link=static variant=release address-model=32 --library-path="%TARGET_LIB_x86_DIR%" --with-psdk-lib=%"PLATTFORM_SDK_LIB_x86%" --with-psdk="%PLATTFORM_SDK_INCLUDE%" --wix="%WIX_PATH%"
call build.bat runtime-link=static variant=release address-model=32 --library-path="%TARGET_LIB_x86_DIR%" --with-psdk-lib="%PLATTFORM_SDK_LIB_x86%" --with-psdk="%PLATTFORM_SDK_INCLUDE%" --wix="%WIX_PATH%"
if %ERRORLEVEL% == -1 goto :error

REM build x64 (Amd64, Emt64) binary
SET version=x64
SET cmdline=build.bat runtime-link=static variant=release address-model=64 --library-path="%TARGET_LIB_x64_DIR%" --with-psdk-lib="%PLATTFORM_SDK_LIB_x64%" --with-psdk="%PLATTFORM_SDK_INCLUDE%" --wix="%WIX_PATH%"
call build.bat runtime-link=static variant=release address-model=64 --library-path="%TARGET_LIB_x64_DIR%" --with-psdk-lib="%PLATTFORM_SDK_LIB_x64%" --with-psdk="%PLATTFORM_SDK_INCLUDE%" --wix="%WIX_PATH%"
if %ERRORLEVEL% == -1 goto :error


REM TODO: Add IA64 version
rem version=IA64
rem call build.bat runtime-link=static variant=release architecture=ia64 --library-path=%TARGET_LIB_IA64_DIR%
rem if %ERRORLEVEL% == -1 goto :error

echo *************
echo *           *
echo *    O K    *
echo *           *
echo *************

goto :eof

:error
echo *********************
echo *                   *
echo *    E R R O R      *
echo *                   *
echo * When building %version% *
echo *                   *
echo *********************
echo %cmdline%

@echo off
@call env.bat

SET _ERROR=
SET w32=0
SET x64=0
SET IA64=0
SET hdr=0
SET src=0
SET clean=0
SET upload=0
echo Starting build > build.log

if "%1" == "" goto args_build_all

:args_loop
if "%1" == "" goto args_done
if "%1" == "w32" set w32=1
if "%1" == "x64" set x64=1
if "%1" == "ia64" set ia64=1
if "%1" == "hdr" set hdr=1
if "%1" == "src" set hdr=1
if "%1" == "clean" set clean=1
if "%1" == "upload" set upload=1
shift
goto args_loop

:args_build_all
echo ### BUILDING ALL ### : %1 --
SET w32=1
SET x64=1
SET IA64=1
SET hdr=1
SET src=1

:args_done


if not "%clean%" == "1" goto no_build_clean
@del/S/Q stage
@del/S/Q bjam-tmp-build
@del/S/Q bin
:no_build_clean

if not "%hdr%" == "1" goto no_build_hdr
echo %jam% --toolset=msvc --with-cryptopp=%CRYPTOPP_SOURCE% --with-cryptopp-target=%NSCP_INCLUDE%\cryptopp cryptopp-headers >> build.log
%jam% --toolset=msvc --with-cryptopp=%CRYPTOPP_SOURCE% --with-cryptopp-target=%NSCP_INCLUDE%\cryptopp cryptopp-headers
if %ERRORLEVEL% == -1 goto :error
echo :: Result: %ERRORLEVEL% >> build.log
:no_build_hdr

if not "%src%" == "1" goto no_build_src
echo %jam% --toolset=msvc source-archive >> build.log
%jam% --toolset=msvc source-archive
if %ERRORLEVEL% == -1 goto :error
echo :: Result: %ERRORLEVEL% >> build.log
:no_build_src

if not "%w32%" == "1" goto no_build_x86
call :build_one address-model=32 variant=release debug-symbols=on debug-store=database --build-type=complete "--library-path=%TARGET_LIB_x86_DIR%" "--with-psdk-lib=%PLATTFORM_SDK_LIB_x86%"
IF DEFINED _ERROR goto :error
:no_build_x86

if not "%x64%" == "1" goto no_build_x64
call :build_one address-model=64 variant=release debug-symbols=on debug-store=database --build-type=complete "--library-path=%TARGET_LIB_x64_DIR%" "--with-psdk-lib=%PLATTFORM_SDK_LIB_x64%"
IF DEFINED _ERROR goto :error
:no_build_x64
rem call build.bat runtime-link=static variant=release architecture=ia64 --library-path=%TARGET_LIB_IA64_DIR%

if not "%ia64%" == "1" goto no_build_ia64
call :build_one address-model=64 architecture=ia64 variant=release debug-symbols=on debug-store=database --build-type=complete "--library-path=%TARGET_LIB_IA64_DIR%" "--with-psdk-lib=%PLATTFORM_SDK_LIB_IA64%"
IF DEFINED _ERROR goto :error
:no_build_ia64


if not "%upload%" == "1" goto no_build_upload
pscp.exe "stage\archive\*.zip" "stage\installer\*.msi" nscp@nsclient.org:/var/nsclient/www/files/nightly/
:no_build_upload

echo *************
echo *           *
echo *    O K    *
echo *           *
echo *************
goto :eof

:build_one
echo build.bat runtime-link=static %* >> build.log
call build.bat runtime-link=static %*
if %ERRORLEVEL% == -1 goto :error_one
echo Result: %ERRORLEVEL% >> build.log
goto :eof

:error_one
echo *********************
echo *                   *
echo *    E R R O R      *
echo *                   *
echo *********************
set _ERROR=1
exit /b -1
goto :eof

:error
echo :: Error: %ERRORLEVEL% >> build.log
echo *********************
echo *                   *
echo *    E R R O R      *
echo *                   *
echo *********************
type build.log

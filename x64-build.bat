@echo off
@call env.bat

SET _ERROR=
echo Starting build > build.log

echo %jam% --toolset=msvc --with-cryptopp=%CRYPTOPP_SOURCE% --with-cryptopp-target=%NSCP_INCLUDE%\cryptopp cryptopp-headers >> build.log
     %jam% --toolset=msvc --with-cryptopp=%CRYPTOPP_SOURCE% --with-cryptopp-target=%NSCP_INCLUDE%\cryptopp cryptopp-headers
if %ERRORLEVEL% == -1 goto :error
echo :: Result: %ERRORLEVEL% >> build.log

echo %jam% --toolset=msvc source-archive >> build.log
     %jam% --toolset=msvc source-archive
if %ERRORLEVEL% == -1 goto :error
echo :: Result: %ERRORLEVEL% >> build.log

call :build_one address-model=64 variant=release debug-symbols=on debug-store=database --build-type=complete "--library-path=%TARGET_LIB_x64_DIR%" "--with-psdk-lib=%PLATTFORM_SDK_LIB_x64%"
IF DEFINED _ERROR goto :error

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

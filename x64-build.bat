@echo off
@call env.bat

SET _ERROR=
echo Starting build > build.log


rem call :build_hdrs
IF DEFINED _ERROR goto :error
rem call :build_src
IF DEFINED _ERROR goto :error    

call :build_one address-model=64 variant=release debug-symbols=on debug-store=database --build-type=complete "--library-path=%TARGET_LIB_x86_DIR%" "--with-psdk-lib=%PLATTFORM_SDK_LIB_x86%"
IF DEFINED _ERROR goto :error

echo *************
echo *           *
echo *    O K    *
echo *           *
echo *************
goto :eof

:build_hdrs
echo %jam% --toolset=msvc-8.0 --with-cryptopp=%CRYPTOPP_SOURCE% --with-cryptopp-target=%NSCP_INCLUDE%\cryptopp cryptopp-headers >> build.log
     %jam% --toolset=msvc-8.0 --with-cryptopp=%CRYPTOPP_SOURCE% --with-cryptopp-target=%NSCP_INCLUDE%\cryptopp cryptopp-headers
if %ERRORLEVEL% == -1 goto :error_one
echo :: Result: %ERRORLEVEL% >> build.log
goto :eof

:build_src
echo %jam% --toolset=msvc-8.0 source-archive >> build.log
     %jam% --toolset=msvc-8.0 source-archive
if %ERRORLEVEL% == -1 goto :error_one
echo :: Result: %ERRORLEVEL% >> build.log
goto :eof

:build_one
echo build.bat %* >> build.log
call build.bat %*
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

@echo off

@call env.bat

REM build x64 (Amd64, Emt64) binary
SET version=x64
SET cmdline=build.bat runtime-link=static  variant=release address-model=64 --library-path=%TARGET_LIB_x64_DIR% --psdk-lib=%PLATTFORM_SDK_LIB_x64%
call build.bat runtime-link=static  variant=release address-model=64 --library-path=%TARGET_LIB_x64_DIR% --psdk-lib=%PLATTFORM_SDK_LIB_x64%
if %ERRORLEVEL% == -1 goto :error

REM build x86 binary
SET version=x86
SET cmdline=build.bat runtime-link=static variant=release address-model=32 --library-path=%TARGET_LIB_x86_DIR% --psdk-lib=%PLATTFORM_SDK_LIB_x86%
call build.bat runtime-link=static variant=release address-model=32 --library-path=%TARGET_LIB_x86_DIR% --psdk-lib=%PLATTFORM_SDK_LIB_x86%
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

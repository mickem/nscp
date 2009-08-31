@echo off

SET BOOST_BUILD_PATH=D:\tools\boost-build
rem SET BOOST_BUILD_PATH=D:\source\libs-c\boost_1_37_0\tools\build\v2
rem SET TOOLS_DIR=d:\source\tools\;C:\Program Files\7-Zip\
SET TOOLS_DIR=d:\source\tools\;%ProgramFiles%\7-Zip\


SET openssl=D:\source\libs-c\openssl-0.9.8g\include
SET boost=D:\source\libs-c\boost_1_34_1
rem SET boost=D:\source\libs-c\boost_1_35_0

set TARGET_LIB_DIR=D:\source\lib
set TARGET_LIB_x86_DIR=%TARGET_LIB_DIR%\x86
set TARGET_LIB_x64_DIR=%TARGET_LIB_DIR%\x64
set TARGET_LIB_IA64_DIR=%TARGET_LIB_DIR%\IA64

set PATH=%PATH%;%TOOLS_DIR%

REM build x64 (Amd64, Emt64) binary
SET version=x64
call build.bat runtime-link=static  variant=release address-model=64 --library-path=%TARGET_LIB_x64_DIR%
if %ERRORLEVEL% == -1 goto :error

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


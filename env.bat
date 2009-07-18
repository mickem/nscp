@echo off
rem ########################
rem #
rem # Set the path to the boost build jam binary
rem #
SET jam=C:\src\tools\boost-jam-3.1.17\bin.ntx86\bjam.exe
rem #
rem ########################
rem #
rem # Set the path to the boost build path (usualy inside the boost library location)
rem #
rem SET BOOST_BUILD_PATH=D:\tools\boost-build
SET BOOST_BUILD_PATH=C:\src\lib-src\boost_1_39_0\tools\build\v2
rem #
rem ########################
rem #
rem # Set the path to your extra include directpy (openssl/boost/*)
rem #
SET include=C:\src\include
rem #
rem ########################
rem #
rem # Set the path to your extra library directory (openssl/boost/*)
rem #
SET TARGET_LIB_DIR=c:\src\lib
rem # (might need to tweak this as well)
SET TARGET_LIB_x86_DIR=%TARGET_LIB_DIR%\x86
SET TARGET_LIB_x64_DIR=%TARGET_LIB_DIR%\x64
SET TARGET_LIB_IA64_DIR=%TARGET_LIB_DIR%\IA64

SET TOOLS_DIR=d:\src\tools\;%ProgramFiles%\7-Zip\

SET PLATTFORM_SDK_LIB="D:/Program/Microsoft Platform SDK for Windows Server 2003 R2/Lib"

SET PLATTFORM_SDK_LIB_x86=%PLATTFORM_SDK_LIB%
SET PLATTFORM_SDK_LIB_x64=%PLATTFORM_SDK_LIB%/AMD64/msi
SET PLATTFORM_SDK_LIB_IA64=%PLATTFORM_SDK_LIB%/IA64

set PATH=%PATH%;%TOOLS_DIR%

rem # Used for building "other" stuff as "I do it".
set SOURCE_DIR=c:\src\lib-src\
set TARGET_INC_DIR=c:\src\include
set BOOST_DIR=%SOURCE_DIR%\boost_1_39_0

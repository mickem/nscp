@echo off
rem ########################
rem #
rem # Set the path to the boost build jam binary
rem #
rem SET jam=C:\src\tools\boost-jam-3.1.17\bin.ntx86\bjam.exe
rem SET jam=D:\source\tools\bjam.exe
set jam=D:\source\libraries\boost_1_45_0\bjam.exe

rem #
rem ########################
rem #
rem # Set the path to the boost build path (usualy inside the boost library location)
rem #
rem SET BOOST_BUILD_PATH=D:\tools\boost-build
rem SET BOOST_BUILD_PATH=C:\src\lib-src\boost_1_39_0\tools\build\v2
SET BOOST_BUILD_PATH=D:\source\libraries\boost_1_45_0\tools\build\v2
rem set BOOST_BUILD_PATH=D:\source\boost-build

rem #
rem ########################
rem #
rem # Set the path to your extra include directpy (openssl/boost/*)
rem #
SET TARGET_INC_DIR=D:\source\include
SET BOOST_INCLUDE_DIR=%TARGET_INC_DIR%\boost-1_45\
SET NSCP_INCLUDE=%TARGET_INC_DIR%
rem #
rem ########################
rem #
rem # Set the path to your extra library directory (openssl/boost/*)
rem #
SET TARGET_LIB_DIR=D:\source\lib
rem #
rem # Setup various relative paths (might need to tweak)
rem #
SET TARGET_LIB_x86_DIR=%TARGET_LIB_DIR%\x86
SET TARGET_LIB_x64_DIR=%TARGET_LIB_DIR%\x64
SET TARGET_LIB_IA64_DIR=%TARGET_LIB_DIR%\IA64
rem #
rem ########################
rem #
rem # Set the path to your Lua sources
rem #
rem set LUA_SOURCE=D:\source\NSCP-stable\lib-source\LUA\src\lua-5.1.2
set LUA_SOURCE=d:\source\libraries\lua-5.1.4
rem #
rem ########################
rem #
rem # Set the path to your Platform SDK
rem #
rem SET PLATTFORM_SDK=D:\Program\Microsoft Platform SDK for Windows Server 2003 R2
rem SET PLATTFORM_SDK=c:\Program Files\Microsoft Platform SDK for Windows Server 2003 R2
SET PLATTFORM_SDK=c:\Program Files\Microsoft Platform SDK
rem #
rem # Setup various relative paths (might need to tweak)
rem #
SET PLATTFORM_SDK_LIB=%PLATTFORM_SDK%\Lib
SET PLATTFORM_SDK_INCLUDE=%PLATTFORM_SDK%\Include
SET PLATTFORM_SDK_LIB_x86=%PLATTFORM_SDK_LIB%
SET PLATTFORM_SDK_LIB_x64=%PLATTFORM_SDK_LIB%/AMD64/msi
SET PLATTFORM_SDK_LIB_IA64=%PLATTFORM_SDK_LIB%/IA64
rem #
rem ########################
rem #
rem # Set the path to your WiX installation
rem #
SET WIX_PATH=d:\source\tools\wix-2.0
rem #
rem ########################
rem #
rem # Set the path to your Crypt++ sources
rem #
SET CRYPTOPP_SOURCE=D:\source\libraries\crypto++-5.6.1
rem ########################
rem #
rem #
rem #

SET GOOGLE_BREAKPAD_INCLUDE=d:\source\libraries\google-breakpad-svn\src

SET ZIP7_PATH=C:\Program Files\7-Zip
SET PERL_PATH=C:\Strawberry\perl\bin
SET TOOLS_DIR=D:\source\tools
SET PUTTY_DIR="c:\Program Files (x86)\PuTTY"

set PATH=%PATH%;%ZIP7_PATH%;%PERL_PATH%;%TOOLS_DIR%;%PUTTY_DIR%

rem # Used for building "other" stuff as "I do it".
rem # set SOURCE_DIR=c:\src\lib-src\
rem set BOOST_DIR=%SOURCE_DIR%\boost_1_39_0

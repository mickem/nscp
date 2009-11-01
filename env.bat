@echo off
rem ########################
rem #
rem # Set the path to the boost build jam binary
rem #
rem SET jam=C:\src\tools\boost-jam-3.1.17\bin.ntx86\bjam.exe
SET jam=D:\source\tools\bjam.exe
rem set jam=C:\src\tools\nightly\boost-build\jam_src\bin.ntx86\bjam.exe

rem #
rem ########################
rem #
rem # Set the path to the boost build path (usualy inside the boost library location)
rem #
rem SET BOOST_BUILD_PATH=D:\tools\boost-build
rem SET BOOST_BUILD_PATH=C:\src\lib-src\boost_1_39_0\tools\build\v2
rem SET BOOST_BUILD_PATH=D:\source\boost-build
SET BOOST_BUILD_PATH=D:\source\boost-build-39
rem SET BOOST_BUILD_PATH=D:\source\boost-build-nightly
rem #
rem ########################
rem #
rem # Set the path to your extra include directpy (openssl/boost/*)
rem #
rem SET TARGET_INC_DIR=C:\src\include
SET TARGET_INC_DIR=d:\source\include
SET NSCP_INCLUDE=%TARGET_INC_DIR%
rem #
rem ########################
rem #
rem # Set the path to your extra library directory (openssl/boost/*)
rem #
rem SET TARGET_LIB_DIR=c:\src\lib
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
rem SET LUA_SOURCE=D:\source\NSCP-stable\lib-source\LUA\src\lua-5.1.2
rem SET LUA_SOURCE=C:\src\lib-src\lua-5.1.4
SET LUA_SOURCE=D:\source\libs-c\lua-5.1.2
rem #
rem ########################
rem #
rem # Set the path to your Platform SDK
rem #
SET PLATTFORM_SDK=D:\Program\Microsoft Platform SDK for Windows Server 2003 R2
rem SET PLATTFORM_SDK=c:\Program Files\Microsoft Platform SDK for Windows Server 2003 R2
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
rem SET WIX_PATH=C:\Program Files\Windows Installer XML
SET WIX_PATH=d:\Program\Windows Installer XML
rem #
rem ########################
rem #
rem # Set the path to your Crypt++ sources
rem #
SET CRYPTOPP_SOURCE=D:\source\libs-c\crypto++-5.6.0
rem SET CRYPTOPP_SOURCE="C:\src\lib-src\Crypto++5.6.0"
rem ########################
rem #
rem #
rem #

SET XAUTOBUILD=D:\source\tools\XAutobuild.exe

SET TOOLS_DIR=c:\src\tools\;%ProgramFiles%\7-Zip\;d:\source\tools\


set PATH=%PATH%;%TOOLS_DIR%

rem # Used for building "other" stuff as "I do it".
rem # set SOURCE_DIR=c:\src\lib-src\
rem set BOOST_DIR=%SOURCE_DIR%\boost_1_39_0


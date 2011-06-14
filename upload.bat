rem SET BOOST_BUILD_PATH=D:\tools\boost-build
SET BOOST_BUILD_PATH=D:\source\libs-c\boost_1_37_0\tools\build\v2
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

set TargetDir=stage\

copy %TargetDir%\op5-installer\*.* X:\dist\op5
copy %TargetDir%\opsera-installer\*.* X:\dist\opsera

pscp.exe "%TargetDir%\archive\*.zip" "%TargetDir%\installer\*.msi" nscp@nsclient.org:/var/nsclient/www/files/nightly/
pscp.exe "%TargetDir%\installer\*.msi" nscp@nsclient.org:/var/nsclient/www/files/nightly/

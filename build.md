# Building NSClient++ on Windows

This is a sample instruction for building NSClient++ on Windows.
I always recommend to use the Pipelines in GitHub Actions to build the project.
But if you want to run locally this is a good starting place.
This will setup all libraries and tools required to build x64 version and only release artifacts.
If you want to produce debug builds and/or w32 some adjustments will be required.

## TOC

* [Prerequisites](#prerequisites)
* [x64 version (dynamic runtime)](#x64-version-dynamic-runtime)
* [Win32 version (static link)](#win32-version-static-link)

## Prerequisites

You need the following tools installed on your machine and in your path:
* Visual Studio (Community Edition is fine)
  * In addition, you need to enable the following modules (to get support for XP) in the installer
    * Microsoft.VisualStudio.Component.VC.v141.x86.x64
    * Microsoft.VisualStudio.Component.VC.v141.ATL
    * Microsoft.VisualStudio.Component.WinXP
* CMake
* 7zip
* Perl (I use strawberry perl)


## x64 version (dynamic runtime)

### Environment

To make things smooth we will define a few environment variables we can use:

```commandline
set SOURCE_ROOT=FOLDER WHERE YOU CLONED THE GIT REPO
set BUILD_FOLDER=WHERE TO BUILD EVERYTHING, NORMALLY AN EMPTY FOLDER
set NSCP_VERSION=NORMALLY READ FROM GIT
# Ensure we configure Visual Studio to use 14.16 (v141_xp) which is the toolset which support building XP binaries
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.16
mkdir %BUILD_FOLDER%
mkdir %BUILD_FOLDER%\nscp
```


### Libraries

#### Open SSL

Run the following commands in a Visual Studio Command Prompt (2015 x64 Native):

```commandline
SET OPENSSL_VERSION=1.1.1w
cd %BUILD_FOLDER%
curl -L https://www.openssl.org/source/openssl-%OPENSSL_VERSION%.tar.gz --output openssl.tar.gz
7z x openssl.tar.gz
7z x openssl.tar

cd %BUILD_FOLDER%\openssl-%OPENSSL_VERSION%
perl Configure VC-WIN64A no-asm no-shared
nmake
```

#### Boost

Run the following commands in a Visual Studio Command Prompt (2015 x64 Native):

```commandline
SET BOOST_VERSION=1.83.0
SET BOOST_VERSION_=%BOOST_VERSION:.=_%
cd %BUILD_FOLDER%
curl -L https://archives.boost.io/release/%BOOST_VERSION%/source/boost_%BOOST_VERSION_%.tar.gz --output boost.tar.gz
7z x -y boost.tar.gz
7z x -y boost.tar
xcopy boost_%BOOST_VERSION_% boost_%BOOST_VERSION_%_static /E /I

cd %BUILD_FOLDER%\boost_%BOOST_VERSION_%
call bootstrap.bat
b2.exe --layout=system address-model=64 toolset=msvc-14.1 variant=release link=shared runtime-link=shared warnings=off -d0 --with-system --with-filesystem --with-thread --with-regex --with-date_time --with-program_options --with-python --with-chrono

cd %BUILD_FOLDER%\boost_%BOOST_VERSION_%_static
call bootstrap.bat
b2.exe --layout=system address-model=64 toolset=msvc-14.1 variant=release link=static runtime-link=static warnings=off define=BOOST_NO_CXX17_HDR_SHARED_MUTEX -d0 --with-system --with-filesystem
```

#### Build Proto-buf

```commandline
SET PROTOBUF_VERSION=21.12
cd %BUILD_FOLDER%
curl -L https://github.com/protocolbuffers/protobuf/releases/download/v%PROTOBUF_VERSION%/protobuf-all-%PROTOBUF_VERSION%.zip --output protobuf.zip
7z x protobuf.zip
cd %BUILD_FOLDER%\protobuf-%PROTOBUF_VERSION%
mkdir build
cd %BUILD_FOLDER%\protobuf-%PROTOBUF_VERSION%\build
cmake -DBUILD_SHARED_LIBS=TRUE -G "Visual Studio 17" -T v141_xp -A x64 ..

msbuild libprotobuf.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild libprotobuf.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild libprotoc.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild protoc.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild libprotobuf-lite.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild libprotobuf-lite.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild libprotobuf.vcxproj /p:Configuration=Debug /p:Platform=x64
```

#### Build Crypto++

```commandline
set CRYPTOPP_VERSION=890
set CRYPTOPP_VERSION_=8_9_0
cd %BUILD_FOLDER%
set SOURCE_ROOT=<where you cloned the repository>
curl -L https://github.com/weidai11/cryptopp/releases/download/CRYPTOPP_%CRYPTOPP_VERSION_%/cryptopp%CRYPTOPP_VERSION%.zip --output cryptopp.zip
mkdir CRYPTOPP_%CRYPTOPP_VERSION_%
cd %BUILD_FOLDER%\CRYPTOPP_%CRYPTOPP_VERSION_%
7z x ..\cryptopp.zip

python %SOURCE_ROOT%/build/python/msdev-to-dynamic.py cryptlib.vcxproj
msbuild cryptlib.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v141_xp
msbuild cryptdll.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v141_xp
```

#### Download Lua

Lua does not require building instead we need to download and configure where the build system can find it.

```commandline
SET LUA_VERSION=5.4.7
cd %BUILD_FOLDER%
curl -L https://www.lua.org/ftp/lua-%LUA_VERSION%.tar.gz --output lua.tar.gz
7z x lua.tar.gz
7z x lua.tar
del lua.tar
del lua.tar.gz
```


#### Download TinyXML-2

TinyXML2 does not require building instead we need to download and configure where the build system can find it.

```commandline
SET TINY_XML2_VERSION=10.1.0
cd %BUILD_FOLDER%
curl -L https://github.com/leethomason/tinyxml2/archive/refs/tags/%TINY_XML2_VERSION%.zip --output tinyxml.zip
7z x tinyxml.zip
del tinyxml.zip
```

### Build installer library

```commandline
cd %BUILD_FOLDER%
mkdir installer_lib
cd installer_lib 
cmake %SOURCE_ROOT%/installer_lib -T v141_xp -G "Visual Studio 17" -A x64 -DBOOST_ROOT=%BUILD_FOLDER%\boost_%BOOST_VERSION_%_static -DBOOST_LIBRARYDIR=%BUILD_FOLDER%\boost_%BOOST_VERSION_%_static/stage/lib -DOPENSSL_ROOT_DIR=%BUILD_FOLDER%\openssl-%OPENSSL_VERSION% -DBUILD_VERSION=%NSCP_VERSION% 
msbuild installer_lib.sln /p:Configuration=Release /p:Platform=x64
```

### Build google test

```commandline
cd %BUILD_FOLDER%
git clone --depth 1 --branch release-1.12.1 https://github.com/google/googletest.git
cd googletest
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="C:/src/build/googletest/install" -DBUILD_SHARED_LIBS=ON -T v141
cmake --build build --config Release --target install
copy install\bin\*.dll %BUILD_FOLDER%\nscp
```

### Configure CMake

Create a file called `build.cmake` adding the paths to the above tools and libraries.

```cmake
SET(USE_STATIC_RUNTIME FALSE)
set(Boost_USE_STATIC_RUNTIME ON)
SET(BOOST_ROOT "BUILD_FOLDER/boost_VERSION")
SET(BOOST_LIBRARYDIR "BUILD_FOLDER/boost_VERSION/stage/lib")
SET(PROTOBUF_LIBRARYDIR "BUILD_FOLDER/protobuf-VERSION/build/Release")
SET(OPENSSL_USE_STATIC_LIBS TRUE)
SET(OPENSSL_ROOT_DIR "BUILD_FOLDER/openssl-VERSION")
SET(LUA_SOURCE_DIR "BUILD_FOLDER/lua-VERSION/src")
SET(CRYPTOPP_ROOT "BUILD_FOLDER/CRYPTOPP_VERSION")
SET(TINY_XML2_SOURCE_DIR "BUILD_FOLDER/tinyxml2-VERSION")
```

### Build NSClient++

```commandline
cd %BUILD_FOLDER%
mkdir nscp
cd nscp
cmake %SOURCE_ROOT% -T v141_xp -G "Visual Studio 17" -A x64 -DBUILD_VERSION=%NSCP_VERSION%
msbuild nscp.sln /p:Configuration=Release /p:Platform=x64
```

## Win32 version (static link)

### Environment

To make things smooth we will define a few environment variables we can use:

```commandline
set SOURCE_ROOT=FOLDER WHERE YOU CLONED THE GIT REPO
set BUILD_FOLDER=WHERE TO BUILD EVERYTHING, NORMALLY AN EMPTY FOLDER
set NSCP_VERSION=NORMALLY READ FROM GIT
# Ensure we configure Visual Studio to use 14.16 (v141_xp) which is the toolset which support building XP binaries
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 -vcvars_ver=14.16
```

### Libraries

#### Open SSL

Run the following commands in a Visual Studio Command Prompt (2015 x64 Native):

```commandline
SET OPENSSL_VERSION=1.1.1w
cd %BUILD_FOLDER%
curl -L https://www.openssl.org/source/openssl-%OPENSSL_VERSION%.tar.gz --output openssl.tar.gz
7z x openssl.tar.gz
7z x openssl.tar

cd %BUILD_FOLDER%\openssl-%OPENSSL_VERSION%
perl Configure VC-WIN32 no-asm no-shared -D_WIN32_WINNT=0x0501
nmake
cd ..
```

#### Boost

Run the following commands in a Visual Studio Command Prompt (2015 x64 Native):

```commandline
SET BOOST_VERSION=1.83.0
SET BOOST_VERSION_=%BOOST_VERSION:.=_%
cd %BUILD_FOLDER%
curl -L https://archives.boost.io/release/%BOOST_VERSION%/source/boost_%BOOST_VERSION_%.tar.gz --output boost.tar.gz
7z x -y boost.tar.gz
7z x -y boost.tar
xcopy boost_%BOOST_VERSION_% boost_%BOOST_VERSION_%_static /E /I

cd %BUILD_FOLDER%\boost_%BOOST_VERSION_%_static
call bootstrap.bat
b2.exe --layout=system address-model=32 toolset=msvc-14.1 variant=release link=static runtime-link=static warnings=off define=BOOST_USE_WINAPI_VERSION=0x501 define=BOOST_NO_CXX17_HDR_SHARED_MUTEX -d0 --with-system --with-filesystem --with-thread --with-regex --with-date_time --with-program_options --with-python --with-chrono
cd ..
```

#### Build Proto-buf

```commandline
SET PROTOBUF_VERSION=21.12
cd %BUILD_FOLDER%
curl -L https://github.com/protocolbuffers/protobuf/releases/download/v%PROTOBUF_VERSION%/protobuf-all-%PROTOBUF_VERSION%.zip --output protobuf.zip
7z x protobuf.zip
cd protobuf-%PROTOBUF_VERSION%
mkdir build
cd %BUILD_FOLDER%\protobuf-%PROTOBUF_VERSION%\build
cmake -DBUILD_SHARED_LIBS=FALSE -G "Visual Studio 17" -T v141_xp -A Win32 ..

msbuild libprotobuf.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v141_xp /p:AdditionalOptions="/DGOOGLE_PROTOBUF_SUPPORT_WINDOWS_XP %(AdditionalOptions)"
msbuild libprotoc.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v141_xp
msbuild protoc.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v141_xp
msbuild libprotobuf-lite.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v141_xp
msbuild libprotobuf-lite.vcxproj /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v141_xp
msbuild libprotobuf.vcxproj /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v141_xp
```

#### Build Crypto++

```commandline
set CRYPTOPP_VERSION=890
set CRYPTOPP_VERSION_=8_9_0
cd %BUILD_FOLDER%
curl -L https://github.com/weidai11/cryptopp/releases/download/CRYPTOPP_%CRYPTOPP_VERSION_%/cryptopp%CRYPTOPP_VERSION%.zip --output cryptopp.zip
mkdir CRYPTOPP_%CRYPTOPP_VERSION_%
cd CRYPTOPP_%CRYPTOPP_VERSION_%
7z x ..\cryptopp.zip

msbuild cryptlib.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v141_xp
msbuild cryptdll.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v141_xp
```

#### Download Lua

Lua does not require building instead we need to download and configure where the build system can find it.

```commandline
SET LUA_VERSION=5.4.7
cd %BUILD_FOLDER%
curl -L https://www.lua.org/ftp/lua-%LUA_VERSION%.tar.gz --output lua.tar.gz
7z x lua.tar.gz
7z x lua.tar
del lua.tar
del lua.tar.gz
```


#### Download TinyXML-2

TinyXML2 does not require building instead we need to download and configure where the build system can find it.

```commandline
SET TINY_XML2_VERSION=10.1.0
cd %BUILD_FOLDER%
curl -L https://github.com/leethomason/tinyxml2/archive/refs/tags/%TINY_XML2_VERSION%.zip --output tinyxml.zip
7z x tinyxml.zip
del tinyxml.zip
```

### Build installer library

```commandline
cd %BUILD_FOLDER%
mkdir installer_lib
cd installer_lib 
cmake %SOURCE_ROOT%/installer_lib -T v141_xp -G "Visual Studio 17" -A Win32 -DBOOST_ROOT=%BUILD_FOLDER%\boost_%BOOST_VERSION_%_static -DBOOST_LIBRARYDIR=%BUILD_FOLDER%\boost_%BOOST_VERSION_%_static/stage/lib -DOPENSSL_ROOT_DIR=%BUILD_FOLDER%\openssl-%OPENSSL_VERSION% -DBUILD_VERSION=%NSCP_VERSION% 
msbuild installer_lib.sln /p:Configuration=Release /p:Platform=x64
```

### Build google test

```commandline
cd %BUILD_FOLDER%
git clone --depth 1 --branch release-1.12.1 https://github.com/google/googletest.git
cd googletest
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="C:/src/build/googletest/install" -T v141_xp -A Win32
cmake --build build --config Release --target install
copy install\bin\*.dll %BUILD_FOLDER%\nscp
```

### Configure CMake

Create a file called `build.cmake` adding the paths to the above tools and libraries.

```cmake
SET(USE_STATIC_RUNTIME FALSE)
set(Boost_USE_STATIC_RUNTIME ON)
SET(BOOST_ROOT "BUILD_FOLDER/boost_VERSION_static")
SET(BOOST_LIBRARYDIR "BUILD_FOLDER/boost_VERSION_static/stage/lib")
SET(PROTOBUF_LIBRARYDIR "BUILD_FOLDER/protobuf-VERSION_static/build/Release")
SET(OPENSSL_USE_STATIC_LIBS TRUE)
SET(OPENSSL_ROOT_DIR "BUILD_FOLDER/openssl-VERSION")
SET(LUA_SOURCE_DIR "BUILD_FOLDER/lua-VERSION/src")
SET(CRYPTOPP_ROOT "BUILD_FOLDER/CRYPTOPP_VERSION")
SET(TINY_XML2_SOURCE_DIR "BUILD_FOLDER/tinyxml2-VERSION")
```

### Build NSClient++

```commandline
cd %BUILD_FOLDER%
mkdir nscp
cd nscp
cmake %SOURCE_ROOT% -T v141_xp -G "Visual Studio 17" -A Win32 -DBUILD_VERSION=%NSCP_VERSION%
msbuild nscp.sln /p:Configuration=Release /p:Platform=Win32
```
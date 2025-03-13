# Building NSClient++ on Windows

This is the embryo of instruction for building NSClient++ on Windows.
I always recommend to use the Pipelines in GitHub Actions to build the project.

But if you want to build it locally, here is how you can do it.


## Prerequisites

You need the following tools installed on your machine and likely in your path:
* Visual Studio (Community Edition is fine)
* CMake
* 7zip
* Perl (I use strawberry perl)

## Libraries

### Open SSL

Run the following commands in a Visual Studio Command Prompt (2015 x64 Native):

```commandline
SET OPENSSL_VERSION=1.1.1w
curl -L https://www.openssl.org/source/openssl-%OPENSSL_VERSION%.tar.gz --output openssl.tar.gz
7z x openssl.tar.gz
7z x openssl.tar

cd openssl-%OPENSSL_VERSION%
perl Configure VC-WIN64A no-asm no-shared
nmake
cd ..
```

### Boost

Run the following commands in a Visual Studio Command Prompt (2015 x64 Native):

```commandline
SET BOOST_VERSION=1.82.0
SET BOOST_VERSION_=%BOOST_VERSION:.=_%
curl -L https://archives.boost.io/release/%BOOST_VERSION%/source/boost_%BOOST_VERSION_%.tar.gz --output boost.tar.gz
7z x -y boost.tar.gz
7z x -y boost.tar
xcopy boost_%BOOST_VERSION_% boost_%BOOST_VERSION_%_static /E /I

cd boost_%BOOST_VERSION_%
call bootstrap.bat
b2.exe --layout=system address-model=64 toolset=msvc-14.16 variant=release link=shared runtime-link=shared warnings=off -d0 --with-system --with-filesystem --with-thread --with-regex --with-date_time --with-program_options --with-python --with-chrono
cd ..

cd boost_%BOOST_VERSION_%_static
call bootstrap.bat
b2.exe --layout=system address-model=64 toolset=msvc-14.16 variant=release link=static runtime-link=shared warnings=off -d0 --with-system --with-filesystem
cd ..
```

### Build Crypto++

```commandline
set CRYPTOPP_VERSION=890
set CRYPTOPP_VERSION_=8_9_0
set SOURCE_ROOT=<where you cloned the repository>
curl -L https://github.com/weidai11/cryptopp/releases/download/CRYPTOPP_%CRYPTOPP_VERSION_%/cryptopp%CRYPTOPP_VERSION%.zip --output cryptopp.zip
mkdir CRYPTOPP_%CRYPTOPP_VERSION_%
cd CRYPTOPP_%CRYPTOPP_VERSION_%
7z x ..\cryptopp.zip

python %SOURCE_ROOT%/build/python/msdev-to-dynamic.py cryptlib.vcxproj
msbuild cryptlib.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v141_xp
msbuild cryptlib.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v141_xp
msbuild cryptdll.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v141_xp
msbuild cryptdll.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v141_xp
```

### Download Lua

Lua does not require building instead we need to download and configure where the build system can find it.

```commandline
SET LUA_VERSION=5.4.7
curl -L https://www.lua.org/ftp/lua-%LUA_VERSION%.tar.gz --output lua.tar.gz
7z x lua.tar.gz
7z x lua.tar
del lua.tar
del lua.tar.gz
```


## Download TinyXML-2

TinyXML2 does not require building instead we need to download and configure where the build system can find it.

```commandline
SET TINY_XML2_VERSION=10.1.0
curl -L https://github.com/leethomason/tinyxml2/archive/refs/tags/%TINY_XML2_VERSION%.zip --output tinyxml.zip
7z x tinyxml.zip
del tinyxml.zip
```

## Configure CMake

Create a file called `build.cmake` adding the paths to the above tools and libraries.

```cmake
set(Boost_USE_STATIC_RUNTIME ON)

SET(USE_STATIC_RUNTIME FALSE)
SET(BOOST_ROOT "TODO")
SET(BOOST_LIBRARYDIR "TODO")
SET(PROTOBUF_ROOT "TODO")
SET(PROTOBUF_LIBRARYDIR "TODO")
SET(OPENSSL_ROOT_DIR "TODO")
SET(OPENSSL_USE_STATIC_LIBS TRUE)
SET(LUA_SOURCE_ROOT "TODO")
SET(Python3_ROOT_DIR "TODO")
SET(Python3_FIND_STRATEGY LOCATION)
SET(CRYPTOPP_ROOT "TODO")
SET(TINY_XML2_SOURCE_DIR "TODO")
```

## Build installer library

```commandline
set SOURCE_ROOT=<where you cloned the repository>
set BUILD_FOLDER=<where you build everything>
set VERSION=0.6.0
mkdir installer_lib
cd installer_lib 
cmake %SOURCE_ROOT%/installer_lib -T v141 -G "Visual Studio 17" -A x64 -DBOOST_ROOT=%BUILD_FOLDER%\boost_%BOOST_VERSION_%_static -DBOOST_LIBRARYDIR=%BUILD_FOLDER%\boost_%BOOST_VERSION_%_static/stage/lib -DOPENSSL_ROOT_DIR=%BUILD_FOLDER%\openssl-%OPENSSL_VERSION% -DBUILD_VERSION=%VERSION% 

msbuild installer_lib.sln /p:Configuration=Release /p:Platform=x64
```

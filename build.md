# Building NSClient++ on Windows

This is a sample instruction for building NSClient++ on Windows.
I always recommend to use the Pipelines in GitHub Actions to build the project.
But if you want to run locally this is a good starting place.
This will setup all libraries and tools required to build x64 version and only release artifacts.
If you want to produce debug builds and/or w32 some adjustments will be required.

## TOC

* [Prerequisites](#prerequisites)
* [Dependencies](#dependencies)
* [Build options](#build-options)
* [x64 version (dynamic runtime)](#x64-version-dynamic-runtime)
* [Win32 version (static link)](#win32-version-static-link)
* [Linux version](#linux-version)
* [Running tests](#running-tests)

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

## Dependencies

NSClient++ links against a handful of third-party libraries. A few are
**required** — the build aborts at configure time without them — and the rest
are **optional**: when they are not found the build still succeeds and the
feature or module that needs them is silently disabled (see
[Build options](#build-options) for the toggles, and
[`build/docker/`](build/docker/README.md) for the images that validate each
"missing library" case).

On Linux almost everything comes from the distribution packages (see
[Linux version](#linux-version)); on Windows each library is downloaded and
built individually (see the per-library subsections under the Windows builds)
and its location is pointed to from `build.cmake`.

### Required

| Dependency                                                                                     | Used for                                     | Debian/Ubuntu package                           | Windows                                                      |
|------------------------------------------------------------------------------------------------|----------------------------------------------|-------------------------------------------------|--------------------------------------------------------------|
| C++17 toolchain                                                                                | compiling                                    | `build-essential`                               | Visual Studio (v141_xp toolset)                              |
| CMake ≥ 3.10                                                                                   | build system                                 | `cmake`                                         | CMake                                                        |
| Boost (system, filesystem, thread, regex, date_time, program_options, chrono, json, container) | core runtime, filtering, JSON, threading     | `libboost-all-dev`                              | built from source                                            |
| Protocol Buffers (library + `protoc`)                                                          | every cross-module message                   | `libprotobuf-dev`, `protobuf-compiler`          | built from source                                            |
| Python 3 **interpreter** + Jinja2                                                              | build-time protobuf / module code generation | `python3`, `python3-protobuf`, `python3-jinja2` | on `PATH` (+ `pip install -r build/python/requirements.txt`) |

The Python *interpreter* is a build tool (it generates each module's glue code
during the build, via a Jinja2 template), so it is always required even though
the Python *runtime libraries* are optional (see below). The full Python tool
set is pinned in `build/python/requirements.txt`.

### Optional

| Dependency                             | Enables                                                   | Debian/Ubuntu package                            | Disabled when absent                                                                         |
|----------------------------------------|-----------------------------------------------------------|--------------------------------------------------|----------------------------------------------------------------------------------------------|
| OpenSSL                                | TLS for NRPE/NSCA/check_mk/native protocol + HTTPS client | `libssl-dev`                                     | TLS features; `SMTPClient` + `NSCANgClient` modules. **Required for the Beast web backend.** |
| Crypto++                               | NSCA payload encryption                                   | `libcrypto++-dev`                                | NSCA encryption                                                                              |
| libzip (Linux/macOS) / miniz (Windows) | reading `.zip` plugin archives                            | `libzip-dev`                                     | ZIP support (`NSCP_ZIP_BACKEND=none`); `.zip` plugins won't load                             |
| Lua (system or bundled source)         | scripting                                                 | `liblua5.4-dev`                                  | `LUAScript`, `CheckMKClient`, `CheckMKServer` modules                                        |
| TinyXML2                               | NRDP XML payloads                                         | `libtinyxml2-dev`                                | `NRDPClient` module                                                                          |
| Python 3 dev libs + Boost.Python       | embedding CPython                                         | `python3.12-dev` (+ Boost built `--with-python`) | `PythonScript` module                                                                        |
| Google Test                            | unit tests                                                | bundled via FetchContent (or `libgmock-dev`)     | unit tests (also toggled by `NSCP_BUILD_TESTS`)                                              |
| Mongoose                               | web / REST server (mongoose backend)                      | vendored source (`MONGOOSE_SOURCE_DIR`)          | only needed when `NSCP_WEB_BACKEND=mongoose` (the default); not needed with `beast`          |
| Rust toolchain                         | builds the bundled `check_nsclient` plugin                | [`rustup`](https://rust-lang.org/tools/install/) | bundled `check_nsclient` (skip with `-DCHECK_NSCLIENT_MISSING=TRUE`)                         |

A few additional Linux packages are pulled in for packaging and supporting
libraries: `pkg-config`, `libffi-dev`, `libdbus-1-dev` and `rpm` (the last one
only for building `.rpm` packages).

## Build options

These are passed on the `cmake` command line (`-DNAME=VALUE`) or set in a
`build.cmake` file. On Linux they go on the command line; on Windows they go in
`build.cmake` (auto-included when found at the repository root, or point at it
with `-DNSCP_CMAKE_CONFIG=<file>`).

### General

| Option                      | Default          | Description                                                                                                                                                              |
|-----------------------------|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `BUILD_VERSION`             | —                | Version string baked into the binaries and installer (normally derived from git).                                                                                        |
| `CMAKE_BUILD_TYPE`          | `RelWithDebInfo` | `Release` / `Debug` / `RelWithDebInfo` / `MinSizeRel`.                                                                                                                   |
| `NSCP_BUILD_TESTS`          | `ON`             | Build the C++ unit tests. `OFF` skips the tests, the `tests/` subdirectory and the bundled googletest download — use it to build the daemon without a test toolchain.    |
| `NSCP_WEB_BACKEND`          | `mongoose`       | HTTP/REST backend for `WEBServer`: `mongoose` (needs the vendored Mongoose source) or `beast` (header-only, **requires OpenSSL**). The Linux package builds use `beast`. |
| `NSCP_BOOST_PYTHON_VERSION` | —                | Boost.Python component matching your Python, e.g. `python311`, `python312`, `python313`. Only relevant when building `PythonScript`.                                     |
| `NSCP_SANITIZE`             | `off`            | Comma-separated sanitizer list for gcc/clang on Linux: `address`, `undefined`, `address,undefined`, `thread`. See `tools/sanitizers/run.sh`.                             |
| `USE_STATIC_RUNTIME`        | `OFF`            | Link the C/C++ runtime statically (used by the Win32 static build).                                                                                                      |
| `USE_SYSTEMD`               | `ON` (Linux)     | Install systemd service files in the package.                                                                                                                            |
| `USE_INITD`                 | `OFF` (Linux)    | Install legacy init.d scripts in the package.                                                                                                                            |
| `CHECK_NSCLIENT_LOCATION`   | —                | Directory holding the prebuilt Rust `check_nsclient` binary to bundle.                                                                                                   |
| `CHECK_NSCLIENT_MISSING`    | unset            | Set `TRUE` to skip bundling `check_nsclient` (no Rust build required).                                                                                                   |
| `NSCP_CMAKE_CONFIG`         | —                | Path (relative to the source or binary dir) to a custom `build.cmake`-style config to include.                                                                           |

### Dependency locations

Mainly needed on Windows (and any time a library is not in a default search
path); on Linux the system packages are found automatically.

| Variable                                       | Points at                                         |
|------------------------------------------------|---------------------------------------------------|
| `BOOST_ROOT` / `BOOST_LIBRARYDIR`              | Boost headers / compiled libraries                |
| `Boost_USE_STATIC_RUNTIME`                     | link Boost against the static runtime             |
| `OPENSSL_ROOT_DIR` / `OPENSSL_USE_STATIC_LIBS` | OpenSSL install / static linking                  |
| `PROTOBUF_LIBRARYDIR`                          | compiled Protocol Buffers libraries               |
| `CRYPTOPP_ROOT`                                | Crypto++ build directory                          |
| `LUA_SOURCE_DIR`                               | unpacked Lua source (built from source)           |
| `TINY_XML2_SOURCE_DIR`                         | unpacked TinyXML2 source                          |
| `MONGOOSE_SOURCE_DIR`                          | unpacked Mongoose source (mongoose backend)       |
| `MINIZ_INCLUDE_DIR`                            | unpacked miniz source (Windows ZIP backend)       |
| `DEPENDENCIES_FOLDER`                          | base folder several of the above are derived from |

### Selecting individual modules

Each module under `modules/`, `clients/` and (on Windows) `tools/` is exposed
as a `BUILD_MODULE_<Name>` CMake option that defaults to `ON`. Pass
`-DBUILD_MODULE_<Name>=OFF` to exclude a module from the build, for example:

```bash
cmake $SOURCE_ROOT \
    -DBUILD_MODULE_WEBServer=OFF \
    -DBUILD_MODULE_NSClientServer=OFF
```

The module is still discovered (the build uses globbing) but its
`add_subdirectory()` is skipped, so it produces no binary and links nothing in.
The configure log marks it with the reason, alongside modules that are skipped
for platform reasons:

```
-- Adding all: Modules
--  - CheckDisk: Disabled (-DBUILD_MODULE_CheckDisk=OFF)
--  - NSClientServer: Skipped
--  - WEBServer: Disabled (-DBUILD_MODULE_WEBServer=OFF)
```

The flag can only opt out: setting `BUILD_MODULE_<Name>=ON` does **not**
override a module that is unsupported on the current platform (e.g.
`NSClientServer` on Linux). The exact module names are the directory names
under `modules/` / `clients/` / `tools/`. Options are cached, so they persist
across reconfigures until explicitly flipped back to `ON`.

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
SET OPENSSL_VERSION=3.5.4
cd %BUILD_FOLDER%
curl -L https://github.com/openssl/openssl/releases/download/openssl-%OPENSSL_VERSION%/openssl-%OPENSSL_VERSION%.tar.gz --output openssl-%OPENSSL_VERSION%.tar.gz
7z x openssl-%OPENSSL_VERSION%.tar.gz
7z x openssl-%OPENSSL_VERSION%.tar

cd %BUILD_FOLDER%\openssl-%OPENSSL_VERSION%
perl Configure VC-WIN64A no-asm no-shared
nmake
```

#### Boost

Run the following commands in a Visual Studio Command Prompt (2015 x64 Native):

```commandline
SET BOOST_VERSION=1.86.0
SET BOOST_VERSION_=%BOOST_VERSION:.=_%
cd %BUILD_FOLDER%
curl -L https://archives.boost.io/release/%BOOST_VERSION%/source/boost_%BOOST_VERSION_%.tar.gz --output boost.tar.gz
7z x -y boost.tar.gz
7z x -y boost.tar
xcopy boost_%BOOST_VERSION_% boost_%BOOST_VERSION_%_static /E /I

cd %BUILD_FOLDER%\boost_%BOOST_VERSION_%
call bootstrap.bat vc141
b2.exe --layout=system address-model=64 toolset=msvc-14.1 variant=release link=shared runtime-link=shared warnings=off -d0 --with-system --with-filesystem --with-thread --with-regex --with-date_time --with-program_options --with-python --with-chrono --with-json --with-container

cd %BUILD_FOLDER%\boost_%BOOST_VERSION_%_static
call bootstrap.bat vc141
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
msbuild cryptlib.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v141
msbuild cryptdll.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v141
msbuild cryptlib.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v141
msbuild cryptdll.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v141
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

#### Download Mongoose

Mongoose does not require building instead we need to download and configure where the build system can find it.

```commandline
SET MONGOOSE_VERSION=7.19
cd %BUILD_FOLDER%
curl -L https://github.com/cesanta/mongoose/archive/refs/tags/%MONGOOSE_VERSION%.zip --output mongoose-%MONGOOSE_VERSION%.zip
7z x mongoose-%MONGOOSE_VERSION%.zip
del mongoose-%MONGOOSE_VERSION%.zip
```

#### Download Miniz

Miniz does not require building instead we need to download and configure where the build system can find it.

```commandline
SET MINIZ_VERSION=3.1.1
cd %BUILD_FOLDER%
curl -L https://github.com/richgel999/miniz/releases/download/%MINIZ_VERSION%/miniz-%MINIZ_VERSION%.zip --output miniz.zip
mkdir miniz-%MINIZ_VERSION%
7z x miniz.zip -ominiz-%MINIZ_VERSION%
del miniz.zip
```

### Build installer library

```commandline
cd %BUILD_FOLDER%
mkdir installer_lib
cd installer_lib 
cmake %SOURCE_ROOT%/installer_lib -T v141 -G "Visual Studio 18" -A x64 -DBOOST_ROOT=%BUILD_FOLDER%\boost_%BOOST_VERSION_%_static -DBOOST_LIBRARYDIR=%BUILD_FOLDER%\boost_%BOOST_VERSION_%_static/stage/lib -DOPENSSL_ROOT_DIR=%BUILD_FOLDER%\openssl-%OPENSSL_VERSION% -DBUILD_VERSION=%NSCP_VERSION% 
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
SET(NSCP_BOOST_PYTHON_VERSION "python311")
SET(BOOST_LIBRARYDIR "BUILD_FOLDER/boost_VERSION/stage/lib")
SET(PROTOBUF_LIBRARYDIR "BUILD_FOLDER/protobuf-VERSION/build/Release")
SET(OPENSSL_USE_STATIC_LIBS TRUE)
SET(OPENSSL_ROOT_DIR "BUILD_FOLDER/openssl-VERSION")
SET(LUA_SOURCE_DIR "BUILD_FOLDER/lua-VERSION/src")
SET(CRYPTOPP_ROOT "BUILD_FOLDER/CRYPTOPP_VERSION")
SET(TINY_XML2_SOURCE_DIR "BUILD_FOLDER/tinyxml2-VERSION")
SET(MONGOOSE_SOURCE_DIR "BUILD_FOLDER/mongoose-VERSION")
SET(MINIZ_INCLUDE_DIR "BUILD_FOLDER/miniz-VERSION")
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
SET OPENSSL_VERSION=3.5.2
cd %BUILD_FOLDER%
curl -L https://github.com/openssl/openssl/releases/download/openssl-%OPENSSL_VERSION%/openssl-%OPENSSL_VERSION%.tar.gz --output openssl-%OPENSSL_VERSION%.tar.gz
7z x openssl-%OPENSSL_VERSION%.tar.gz
7z x openssl-%OPENSSL_VERSION%.tar

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

#### Download Miniz

Miniz does not require building instead we need to download and configure where the build system can find it.

```commandline
SET MINIZ_VERSION=3.1.1
cd %BUILD_FOLDER%
curl -L https://github.com/richgel999/miniz/releases/download/%MINIZ_VERSION%/miniz-%MINIZ_VERSION%.zip --output miniz.zip
mkdir miniz-%MINIZ_VERSION%
7z x miniz.zip -ominiz-%MINIZ_VERSION%
del miniz.zip
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
SET(NSCP_BOOST_PYTHON_VERSION "python311")
SET(BOOST_ROOT "BUILD_FOLDER/boost_VERSION_static")
SET(BOOST_LIBRARYDIR "BUILD_FOLDER/boost_VERSION_static/stage/lib")
SET(PROTOBUF_LIBRARYDIR "BUILD_FOLDER/protobuf-VERSION_static/build/Release")
SET(OPENSSL_USE_STATIC_LIBS TRUE)
SET(OPENSSL_ROOT_DIR "BUILD_FOLDER/openssl-VERSION")
SET(LUA_SOURCE_DIR "BUILD_FOLDER/lua-VERSION/src")
SET(CRYPTOPP_ROOT "BUILD_FOLDER/CRYPTOPP_VERSION")
SET(TINY_XML2_SOURCE_DIR "BUILD_FOLDER/tinyxml2-VERSION")
SET(MINIZ_INCLUDE_DIR "BUILD_FOLDER/miniz-VERSION")
```

### Build NSClient++

```commandline
cd %BUILD_FOLDER%
mkdir nscp
cd nscp
cmake %SOURCE_ROOT% -T v141_xp -G "Visual Studio 17" -A Win32 -DBUILD_VERSION=%NSCP_VERSION%
msbuild nscp.sln /p:Configuration=Release /p:Platform=Win32
```

## Linux version

### Install dependencies

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev libboost-all-dev libprotobuf-dev protobuf-compiler liblua5.4-dev libtinyxml2-dev libzip-dev libffi-dev python3.12-dev python3-protobuf python3-jinja2 libdbus-1-dev pkg-config rpm libgmock-dev
```

The build generates each module's glue code at build time with a Python script
that uses **Jinja2** (`python3-jinja2` above). The full set of Python build
tools — Jinja2 plus the optional docs (MkDocs) and Lua/Markdown protobuf
generators — is pinned in `build/python/requirements.txt`; install it with pip
if you also want those optional pieces (this is what CI does):

```bash
pip3 install -r build/python/requirements.txt
```

In addition to this you also need to install rust: https://rust-lang.org/tools/install/

### Download dependencies

To build NSClient++ we need to download a few dependencies which are linked to NSClient.
These can be placed anywhere but for this example we will place them in `~/dependencies`.

```bash
export DEPENDENCIES_FOLDER="${HOME}/dependencies"
mkdir -p $DEPENDENCIES_FOLDER
```

TinyXML2 and the zip backend (libzip) are installed via the apt step above
(`libtinyxml2-dev`, `libzip-dev`). No download required on Linux.

Mongoose is not needed on Linux **when building with the Beast backend**
(`NSCP_WEB_BACKEND=beast`, which `build-debian.yml` / `build-redhat.yml` set
in their generated `build.cmake`). Beast is header-only and already covered by
`libboost-all-dev`. Note the default backend is still `mongoose`, so a plain
configure that doesn't set `NSCP_WEB_BACKEND=beast` would require the vendored
Mongoose source — the Linux package builds opt into Beast for exactly this
reason.

#### Build Rust NSClient check_nsclient client

The Rust based `check_nsclient` tool needs to be built before building NSClient++.

```bash
cd $SOURCE_ROOT/rust/check_nsclient
cargo build --release
```

#### Build the web UI bundle (optional)

The React/Vite web frontend is **not** built as part of the CMake build:
`web/CMakeLists.txt` only copies files out of `web/dist/` and is a no-op when
`dist/` is empty. The CI `build-debian.yml` / `build-redhat.yml` workflows
deliberately leave `dist/` empty so the resulting `.deb` / `.rpm` do not
contain the UI — operators install the matching `NSCP-Web-<version>.zip` at
runtime via `sudo nscp web install-ui` (see
[Installing on Linux](docs/docs/setup/installing.md#installing-the-web-ui-bundle)).

For a local source build you have two options:

1. **Bundle the UI in-tree** — run the npm build once before configuring
   CMake. The resulting `web/dist/` is picked up automatically; the
   built daemon serves the UI without any further step.

    ```bash
    cd $SOURCE_ROOT/web
    npm install
    npm run build
    cd -
    ```

2. **Install the UI at runtime** — skip the npm step entirely. The daemon
   boots fine without the bundle and serves a built-in placeholder on `/`
   until `nscp web install-ui` (or `--from /path/to/NSCP-Web-X.Y.Z.zip` for
   air-gapped hosts) drops the real UI into `${web-path}`.

Either way, `WEBServer` and its REST API don't depend on the bundle.

### Build NSClient++

Linux passes its configuration on the `cmake` command line — no `build.cmake`
file is needed (Windows still uses one, see the sections above). The cache
remembers everything so subsequent `cmake $SOURCE_ROOT` calls in the same
build directory don't need to repeat the flags.

```bash
cd $BUILD_FOLDER/nscp
cmake $SOURCE_ROOT \
    -DBUILD_VERSION=$NSCP_VERSION \
    -DCMAKE_BUILD_TYPE=Release \
    -DNSCP_WEB_BACKEND=beast \
    -DNSCP_BOOST_PYTHON_VERSION=python312 \
    -DCHECK_NSCLIENT_LOCATION="$SOURCE_ROOT/rust/check_nsclient/target/release"
make -j$(nproc)
```

### Trimming the build

The [Dependencies](#dependencies) and [Build options](#build-options) sections
list which libraries are optional and the flags that control them. To build on
a host that is missing some of them, drop the matching `-dev` packages from the
`apt-get install` line above and add the relevant options, for example:

```bash
# A lean build: no unit tests and no embedded Python (drop libgmock-dev,
# python3.12-dev and Boost.Python from the apt line). OpenSSL is kept so the
# Beast web backend still works.
cmake $SOURCE_ROOT \
    -DBUILD_VERSION=$NSCP_VERSION \
    -DCMAKE_BUILD_TYPE=Release \
    -DNSCP_WEB_BACKEND=beast \
    -DNSCP_BUILD_TESTS=OFF \
    -DCHECK_NSCLIENT_MISSING=TRUE
```

Dropping **OpenSSL** is a special case: the Beast web backend needs it, so an
OpenSSL-less build must also switch to `-DNSCP_WEB_BACKEND=mongoose`, which in
turn needs the vendored Mongoose source (`-DMONGOOSE_SOURCE_DIR=...`) since
there is no Linux package for it.

Each missing-dependency combination is validated by the Docker images under
[`build/docker/`](build/docker/README.md).

Individual modules can also be dropped without touching dependencies — see
[Selecting individual modules](#selecting-individual-modules) for the
`-DBUILD_MODULE_<Name>=OFF` flag.

## Running tests

There are three kinds of tests in this repo:

1. **Unit tests** — C++ Google Test binaries, registered with CTest via
   `NSCP_CREATE_TEST()` in `tests/CMakeLists.txt`. They run against
   library code and don't need a built daemon.
2. **Acceptance tests** — Python scripts driven by `nscp unit`, executed
   by `tests/acceptance-tests.sh` (Linux) and
   `tests/acceptance-tests.bat` (Windows). They need an installed `nscp`
   on PATH.
3. **Scenario / integration tests** — cross-platform Jest + TypeScript
   suites that spin up per-protocol Docker containers (NRDP, NSCA,
   NSCA-NG, SMTP, Icinga, HTTP proxy, etc.) and drive `nscp` against
   them. These replace the old `tests/<proto>/run-test.bat` scripts and
   run on both Linux and Windows.

### Unit tests (CTest)

```bash
# Linux / WSL
ctest --test-dir cmake-build-debug-wsl --output-on-failure

# Run a single test target by name (regex)
ctest --test-dir cmake-build-debug-wsl -R str_test --output-on-failure

# Run a gtest binary directly with a filter
./cmake-build-debug-wsl/bin/str_test --gtest_filter='FormatTest.*'
```

### Acceptance tests

```bash
# Linux
./tests/acceptance-tests.sh

# Windows (from the build/target folder containing nscp.exe)
tests\acceptance-tests.bat
```

### Scenario / integration tests

These live under `tests/integration/` and a per-protocol `*.test.ts` file in each `tests/<proto>/` directory. They
use [testcontainers-node](https://node.testcontainers.org/) to build and manage the per-test Docker
images, [execa](https://github.com/sindresorhus/execa) to drive the `nscp` CLI, and Jest as the runner.

#### Requirements

* Node.js 20+ and npm (matches the existing `tests/rest/` Jest suite)
* Docker Desktop (Windows) or a working Docker daemon (Linux)
* A built `nscp` (or `nscp.exe`) binary

#### Install and run

```bash
cd tests/integration
npm install                                # one-time

# Point at your built binary. The harness also auto-detects
# cmake-build-debug-wsl/nscp and cmake-build-debug/nscp at the repo root.
export NSCP_BIN=/abs/path/to/nscp           # Linux/macOS
# set NSCP_BIN=C:\path\to\nscp.exe          # Windows (cmd)

npm test                                                            # all
npx jest --runInBand --testPathPattern nrdp                         # one suite
RUN_CMK_SITE_TEST=1 npx jest --testPathPattern cmk-site             # heavy opt-in suite
```

The Checkmk site end-to-end test pulls a ~500MB image and is skipped unless `RUN_CMK_SITE_TEST=1` is set. The MSI
installer tests (`tests/msi/`) stay Windows-only and are not part of this harness.

#### Running under sanitizers

The integration harness understands a sanitizer-instrumented `nscp`: on every
`NscpInstance.stop()` it scans the daemon's captured stderr for AddressSanitizer
/ LeakSanitizer / UndefinedBehaviorSanitizer reports and fails the test (printing
the full report) if it finds one. This surfaces a leak or UB hit even when the
test's own assertions all passed — without it, a report on a passing test would
be swallowed by the capture buffer and never shown.

Build a sanitizer-instrumented tree with `tools/sanitizers/run.sh`. It
configures `build-<sanitizers>/` with `-DNSCP_SANITIZE` and
`-DCMAKE_BUILD_TYPE=RelWithDebInfo`, builds the whole daemon **and all its
modules** (not just the `*_test` binaries), then runs the C++ unit tests under
the sanitizer:

```bash
# From the repo root. ASan + UBSan by default -> build-address+undefined/
tools/sanitizers/run.sh

# Or pick a different sanitizer (each gets its own build dir):
SANITIZE=thread tools/sanitizers/run.sh
```

That leaves a fully instrumented daemon at `build-address+undefined/nscp` (the
directory is `build-${SANITIZE//,/+}`). Point the harness at it and run the
suites as usual:

```bash
cd tests
export NSCP_BIN=$PWD/../build-address+undefined/nscp
npm test
```

Unlike the ctest unit tests — where `run.sh` / CMake bake the sanitizer runtime
options into each test's `ENVIRONMENT` — the harness only forwards your shell
environment to the spawned daemon. Set the options yourself if you need to tune
behaviour or apply the repo's suppression lists:

```bash
export LSAN_OPTIONS=suppressions=$PWD/../tools/sanitizers/lsan-suppressions.txt
export UBSAN_OPTIONS=suppressions=$PWD/../tools/sanitizers/ubsan-suppressions.txt:print_stacktrace=1
```

See the header of `tools/sanitizers/run.sh` and
`.github/workflows/tests-sanitizers.yml` for more variations.

See `tests/integration/README.md` for the full layout, fixture documentation

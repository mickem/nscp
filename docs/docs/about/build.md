# Building NSClient++

NSClient++ is fairly simple to build and setup. It requires python and cmake for the build process as well as a number of external libraries.
This document is split between "Linux" and "Windows" since there is a substantial difference in how to build them.
For people wanting a Apple osx version it can be built similarly to the Linux machines on that platform as well.

## Linux

As dependencies vary between different Linux distributions the packages will be different.
The best way to find and up-to-date list of dependencies is to review the various Dockerfiles found under the `build/docker` folder.

Some general requirements are:

* cmake
* python
* openssl
* libboost

Some modules will require additional dependencies such as NSCA require libcrypto, WEBServer require json-spirit. etc...

### Getting the code from github

Next up we download the source code from github and it is important we specify `--recursive` here as git wont clone the submodule otherwise:

```
git clone --recursive https://github.com/mickem/nscp.git
```

### Building NSClient++

Create a folder in which we will build the code (you can use the same on as the source but it is not recommended):

```
mkdir build
cd build

Run cmake to create the build files:

```
cmake ../nscp
```

Build the actual code:

```
make
```

Run the built-in test to make sure everything is working:

```
make test
```

## Windows

Dependencies is a bit of a bother to manage on Windows since there is no general package mechanism for libraries.
To help with the there is a script called fetch-deps.py which will help download and build all dependencies required.

### Tools ###

All tools have to be installed manually by hand (fetchdeps only manages libraries):

- CMake 2.6
- Python 2.7 (Make sure the right python platform is in your path)
- Visual Studio (I use 2012 and 2015)
- WiX 3.9 (or later)
- Perl 5.12 (required by openssl)
- A git client (if you want to fetch the git sources)

Validate all tools are in your path:

```
cmake --version
python -V
perl -v
cl /?
```

Get the source code using git:

```
git clone --recursive https://github.com/mickem/nscp.git
```

Create a build folder:

```
mkdir build
cd build

Build all dependencies and required libraries (pick the right platform Win32 or x64):

```
nscp\build\python\fetchdeps.py --target <platform> --dyn --source <source path> --msver 2012
```

Validate that we have all dependencies:

```
cmake -D TARGET=dist -D SOURCE=nscp -P nscp\check_deps.cmake
```

Build NSClient++ (If you don't know you your visual studio version name you can run cmake --help to list all available profiles):

```
cd dist
cmake -G "VISUAL STUDIO GNERATOR STRING" ../nscp
msbuild /p:Configuration=RelWithDebInfo NSCP.sln
```

## Docker

We provide a number of Dockerfiles which will build packages for various distributions.

```
git clone --recursive https://github.com/mickem/nscp.git
```

```
docker build -f build/docker/<your dist> --tag builder .
docker run --rm -it --volume /packages:/tmp builder
```

The resulting packages will be found under /tmp given the above example.

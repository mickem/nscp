# Building NSClient++

NSClient++ is fairly simple to build and setup. It requires python and cmake for the build process as well as a number of external libraries.
This document is split between "Linux" and "Windows" since there is a substantial difference in how to build them.
For people wanting a Apple osx version it can be built similarly to the Linux machines on that platform as well.

## Linux

The dependencies are different on different Linux systems so we will start with a section on installing dependencies on various platforms.

### Dependencies on Ubuntu

First we need to install a set of packages:

```
sudo apt-get install -y git
sudo apt-get install -y build-essential
sudo apt-get install -y cmake
sudo apt-get install -y python python-dev
sudo apt-get install -y libssl-dev
sudo apt-get install -y libboost-all-dev
sudo apt-get install -y protobuf-compiler python-protobuf libprotobuf-dev
sudo apt-get install -y python-sphinx
sudo apt-get install -y libcrypto++-dev libcrypto++
sudo apt-get install -y liblua5.1-0-dev
sudo apt-get install -y libgtest-dev
```

### Getting the code from github

Next up we download the source code from github: `git clone --recursive https://github.com/mickem/nscp.git`

### Building NSClient++

Create a folder in which we will build the code:
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

## Windows ##

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

## Vagrant

We provide a number of vagrant profiles which will built NSClient++ as well:

```
git clone --recursive https://github.com/mickem/nscp.git
```

```
cd vagrant
cd precise32 # Replace this with precise64 or oracle-linux-6.4_64
vagrant up -- provision
```

The resulting packages will be found under packages

#####################
 Building NSClient++
#####################

NSclient++ is fairly simple to build and setup. It requires python and cmake for the build process as well as a number of external libraries.
This document is plsit between "Linux" and "Windows" since there is a substansial difference in how to build them.
For people wanting a APple osx version my guess is that it could be built htre as well but I do not have access to any apple hardware and can thus no try it.

Linux
======
The dependencies are different on different linux systems so we will start with a section on installing dependencies on vairous plattformas.

Dependencies on Ubuntu
***********************

First we need to install a set of packages::

	sudo apt-get install -y git 
	sudo apt-get install -y build-essential
	sudo apt-get install -y cmake
	sudo apt-get install -y python python-dev
	sudo apt-get install -y libssl-dev 
	sudo apt-get install -y libboost-all-dev
	sudo apt-get install -y protobuf-compiler python-protobuf libprotobuf-dev
	sudo apt-get install -y rst2pdf  
	sudo apt-get install -y python-sphinx
	sudo apt-get install -y libcrypto++-dev libcrypto++
	sudo apt-get install -y liblua5.1-0-dev
	sudo apt-get install -y libgtest-dev

Getting the code fom github
****************************

Nerxt up we download the source code from guit hub::

	git clone --recursive https://github.com/mickem/nscp.git

Building NSClient++
********************

First we create a folder in which we will build the code::

	mkdir build
	cd build
	
Then we run cmake to create the build files::

	cmake ../nscp
	
Then we build the actual code::

	make

Lastly we run the built-in test to make sure eveything is working::

	make test

Windows
========

Dependencies is a bit of a bother to manage on Windows sicne there is no package mechanism.

Tools
*************

All tools have to be installed manuall by hand:

* CMake 2.6
* Python 2.7
* Visual Studio
* WiX 3.5
* Nasm 2.10 (optinal)
* Perl 5.12 (required by openssl)
* msysGit (latest version)

Validate all tools are in your path::

	cmake --version
	python -V
	perl -v
	cl /?

Get the sourcecode using git::
	mkdir win32-build-folder
	cd win32-build-folder
	git clone --recursive https://github.com/mickem/nscp.git

Build all dependencies and required libraries (pick the right plattform Win32 or x64)::

	nscp\build\python\fetchdeps.py --target win32 --cmake-config dist
	nscp\build\python\fetchdeps.py --target x64 --cmake-config dist

	
Validate that we have all dependencies::

	cmake -D TARGET=dist -D SOURCE=nscp -P nscp\check_deps.cmake

Build NSClient++ (If you dont know you your visual studio versio name you can run cmake --help to list all avalible profiles)::

	cd dist
	cmake -G "VISUAL STUDIO GNERATOR STRING" ../nscp
	msbuild /p:Configuration=RelWithDebInfo NSCP.sln
	
Vagrant
========

I provide a number of vagrant profiles which will built NSClient++ as well::

	git clone --recursive https://github.com/mickem/nscp.git
	cd vagrant
	cd precise32 # Replace this with precise64 or oracle-linux-6.4_64
	vagrant up
	vagrant provision
	vagrant ssh
	# Once your done playing...
	vagrant destroy

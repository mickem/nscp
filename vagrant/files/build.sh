#/bin/sh
mkdir build
pushd build
cmake /source/nscp || exit 1
make package || exit 1
ctest --output-on-failure || exit 1
#sh copy_package.sh

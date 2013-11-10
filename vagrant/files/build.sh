mkdir build
pushd build
cmake /source/nscp || exit 1
make || exit 1
ctest --output-on-failure || exit 1

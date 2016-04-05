#/bin/bash
mkdir build
pushd build
cmake $* /source/nscp || exit 1
make || exit 1
ctest --output-on-failure || exit 1
make package || exit 1
[[ -n $(shopt -s nullglob; echo *.rpm) ]] && cp *.rpm /source/nscp/packages
[[ -n $(shopt -s nullglob; echo *.deb) ]] && cp *.deb /source/nscp/packages
#sh copy_package.sh

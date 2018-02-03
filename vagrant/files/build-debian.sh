#/bin/bash
mkdir build
mkdir deb
pushd build
cmake $* /source/nscp || exit 1
make package_source || exit 1
VERSION=`/source/nscp/build/python/version.py -f /source/nscp/version.txt -d`
mv nscp-$VERSION-Source.tar.gz ../nsclient++_$VERSION.orig.tar.gz
popd
pushd deb
tar zxvf ../nsclient++_$VERSION.orig.tar.gz || exit 1
mv nscp-$VERSION-Source/* .
rm -rf nscp-$VERSION-Source
dpkg-buildpackage || exit 1
popd
mv nsclient++*_$VERSION_*.deb /source/nscp/packages

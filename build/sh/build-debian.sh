#/bin/bash
mkdir /build/src
mkdir /build/deb
cd /build/src
dos2unix /nscp/debian/source/format /nscp/debian/*
cmake $* /nscp || exit 1
make package_source || exit 1
VERSION=`/nscp/build/python/version.py -f /nscp/version.txt -d`
mv nscp-$VERSION-Source.tar.gz /build/nsclient++_$VERSION.orig.tar.gz
cd /build/deb
tar zxvf /build/nsclient++_$VERSION.orig.tar.gz || exit 1
mv nscp-$VERSION-Source/* .
rm -rf nscp-$VERSION-Source
dch -v $VERSION-1 "Upstream package" || exit 1
dpkg-buildpackage || exit 1
mv ../nsclient++*_$VERSION_*.deb /packages
. /etc/os-release
ARCH=`uname -m`
OSVERSION=debian-$VERSION_ID
VERSION=`/nscp/build/python/version.py -f /nscp/version.txt -d`
zip -j /packages/NSCP-${VERSION}-${OSVERSION}-${ARCH}.zip /packages/nsclient++*_$VERSION_*.deb
python /nscp/build/python/upload.py --token ${GH_TOKEN} --release ${VERSION} --file /packages/NSCP-${VERSION}-${OSVERSION}-${ARCH}.zip

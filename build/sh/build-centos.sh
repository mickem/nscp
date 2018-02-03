#/bin/bash

cd /src/build
rpmdev-setuptree || exit 1
cmake $* /src/nscp || exit 1
make package_source || exit 1
VERSION=`/src/nscp/build/python/version.py -f /src/nscp/version.txt -d`
cp nscp-$VERSION-Source.tar.gz /root/rpmbuild/SOURCES/
rpmbuild -ba SPECS/nscp.spec || exit 1

if cat /etc/redhat-release | grep -q 'release 6' ; then
  OSVERSION=el6
elif cat /etc/redhat-release | grep -q 'release 6' ; then
  OSVERSION=el6
elif lsb_release -r -s | grep -q ^7 ; then
  OSVERSION=el7
elif cat /etc/redhat-release | grep -q 'release 7' ; then
  OSVERSION=el7
else
  echo "Unrecognized OS version"
  exit 1
fi;

if [ -d /root/rpmbuild/RPMS/x86_64 ] ; then
  zip -j /src/packages/NSCP-${VERSION}-${OSVERSION}-x86_64.zip /root/rpmbuild/RPMS/x86_64/*${VERSION}* -x *debuginfo*
  python /src/nscp/build/python/upload.py --token ${GH_TOKEN} --release ${VERSION} --file /src/packages/NSCP-${VERSION}-${OSVERSION}-x86_64.zip
fi;
if [ -d /root/rpmbuild/RPMS/i386 ] ; then
  zip -j /src/packages/NSCP-${VERSION}-${OSVERSION}-i386.zip /root/rpmbuild/RPMS/i386/* -x *debuginfo*
  python /src/nscp/build/python/upload.py --token ${GH_TOKEN} --release ${VERSION} --file /src/packages/NSCP-${VERSION}-${OSVERSION}-x86_64.zip
fi;

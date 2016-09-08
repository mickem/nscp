#/bin/bash
rpmdev-setuptree || exit 1
mkdir build
pushd build
cmake $* /source/nscp || exit 1
make package_source || exit 1
cp nscp-*-Source.tar.gz /home/vagrant/rpmbuild/SOURCES/
rpmbuild -ba SPECS/nscp.spec || exit 1
# cp -r /home/vagrant/rpmbuild/RPMS/ /source/nscp/packages
popd
VERSION=`/source/nscp/build/python/version.py -f /source/nscp/version.txt -d`

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
fi;
#DIST=`lsb_release -si`
#if [ "$DIST" == "CentOS" ] ; then DIST=Fedora ; fi

if [ -d /home/vagrant/rpmbuild/RPMS/x86_64 ] ; then
  zip -j /source/nscp/packages/NSCP-${VERSION}-${OSVERSION}-x86_64.zip /home/vagrant/rpmbuild/RPMS/x86_64/*${VERSION}* -x *debuginfo*
  cp /home/vagrant/rpmbuild/RPMS/x86_64/*debuginfo*${VERSION}* /source/nscp/packages
fi;
if [ -d /home/vagrant/rpmbuild/RPMS/i386 ] ; then
  zip -j /source/nscp/packages/NSCP-${VERSION}-${OSVERSION}-i386.zip /home/vagrant/rpmbuild/RPMS/i386/* -x *debuginfo*
  cp /home/vagrant/rpmbuild/RPMS/i386/*debuginfo*${VERSION}* /source/nscp/packages
fi;
# ctest --output-on-failure || exit 1
# make package || exit 1
# [[ -n $(shopt -s nullglob; echo *.rpm) ]] && cp *.rpm /source/nscp/packages
# [[ -n $(shopt -s nullglob; echo *.deb) ]] && cp *.deb /source/nscp/packages
#sh copy_package.sh

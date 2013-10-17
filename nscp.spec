%define debug_package %{nil}

Summary:	Simple yet powerful and secure monitoring daemon originally built for Nagios/Icinga
Name:		nscp
Version:	0.4.1
Release:	3%{?dist}
License:	GPLv2+
Group:		Applications/System
URL:		http://nsclient.org
Source0:	https://github.com/mickem/nscp/tree/master/releases/%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Packager:	Pall Sigurdsson <palli@opensource.is>
Requires:	boost-filesystem boost-program-options boost-thread protobuf
BuildRequires:  cmake
BuildRequires:  python 
BuildRequires:  openssl-devel
BuildRequires:  boost-devel
BuildRequires:  python-devel
BuildRequires:  protobuf-devel
BuildRequires:  protobuf-compiler
BuildRequires:  rst2pdf 



%description
NSClient++ (nscp) aims to be a simple yet powerful and secure monitoring daemon. It was built for Nagios/Icinga, but nothing in the daemon is Nagios/Icinga specific and it can be used in many other scenarios where you want to receive/distribute check metrics.

%prep
%setup -q

%build
mkdir tmp
pushd tmp
cmake ..
make
touch nsclient.ini
./nscp settings --generate  --load-all --add-defaults


%install
rm -rf %{buildroot}
ls
mkdir -p %{buildroot}%{_exec_prefix}
mkdir -p %{buildroot}%{_sysconfdir} 
mkdir -p %{buildroot}%{_sysconfdir}/init.d/
 
install -D -p -m 0755 tmp/nscp %{buildroot}%{_exec_prefix}/sbin/nscp
install -m 0755 -p tmp/nsclient.ini %{buildroot}%{_sysconfdir}/nsclient.ini
install -m 0755 -p files/nscp.init %{buildroot}%{_sysconfdir}/init.d/nscp


%clean
rm -rf %{buildroot}

%post

%files
%defattr(-,root,root,-)
%doc README.md
%{_exec_prefix}/sbin/nscp
%config(noreplace) %{_sysconfdir}/nsclient.ini
%{_sysconfdir}/init.d/nscp

%changelog
* Wed Oct 16 2013 Pall Sigurdsson <palli@opensource.is> 0.4.1-3
- Initial Packaging

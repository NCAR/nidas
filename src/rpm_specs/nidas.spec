Summary: Basic system setup for NIDAS (NCAR In-Situ Data Acquistion Software)
Name: nidas
Version: 1.0
Release: 1
License: GPL
Group: Applications/Engineering
Url: http://www.eol.ucar.edu/
Packager: Gordon Maclean <maclean@ucar.edu>
# becomes RPM_BUILD_ROOT
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Vendor: UCAR
BuildArch: noarch
Source: %{name}-%{version}.tar.gz

Requires: xmlrpc++ xerces-c libpcap

# Source: %{name}-%{version}.tar.gz

%description
ld.so.conf setup for NIDAS

%package x86-build
Summary: Package for building nidas on x86 systems with scons.
Requires: nidas scons xerces-c-devel
%description x86-build
Package for building nidas on x86 systems with scons.

%package daq
Summary: Package for doing data acquisition with NIDAS.
Requires: nidas
%description daq
Package for doing data acquisition with NIDAS.
Contains some udev rules to expand permissions on /dev/tty[A-Z]* and /dev/usbtwod*

%prep
%setup -n nidas

%build

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d
echo "/opt/local/nidas/x86/lib" > $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nidas.conf

install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/udev/rules.d
cp etc/udev/rules.d/* $RPM_BUILD_ROOT%{_sysconfdir}/udev/rules.d

%post
/sbin/ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%config %{_sysconfdir}/ld.so.conf.d/nidas.conf

%files x86-build

%files daq
%config %attr(0644,root,root) %{_sysconfdir}/udev/rules.d/99-nidas.rules


%changelog
* Tue May 12 2009 Gordon Maclean <maclean@ucar.edu>
- initial version

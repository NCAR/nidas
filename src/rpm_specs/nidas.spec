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

Requires: xmlrpc++ xerces-c

# Source: %{name}-%{version}.tar.gz

%description
ld.so.conf setup for NIDAS

%prep

%build

%install
install -d $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d
echo "/opt/local/nidas/x86/lib" > $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nidas.conf

%post
/sbin/ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%config %{_sysconfdir}/ld.so.conf.d/nidas.conf

%changelog
* Tue May 12 2009 Gordon Maclean <maclean@ucar.edu>
- initial version

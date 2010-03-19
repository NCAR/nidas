%define nidas_prefix /opt/local/nidas

Summary: NCAR In-Situ Data Acquistion Software binaries and configuration XML schema.
Name: nidas-bin
Version: 1.0
Release: 1%{?dist}
License: GPL
Group: Applications/Engineering
Url: http://www.eol.ucar.edu/
Packager: Gordon Maclean <maclean@ucar.edu>
# becomes RPM_BUILD_ROOT
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Vendor: UCAR
BuildArch: i386
Source: %{name}-%{version}.tar.gz
BuildRequires: nidas-x86-build
Requires: nidas
%description
NCAR In-Situ Data Acquistion Software libraries, executables and configuration XML schema.

%package devel
Summary: NIDAS C/C++ headers.
Requires: nidas-bin
Group: Applications/Engineering
%description devel
NIDAS C/C++ header files.

# Source: %{name}-%{version}.tar.gz

%prep
%setup -n nidas-bin

%build

pwd
cd src
scons BUILDS=x86 PREFIX=${RPM_BUILD_ROOT}%{nidas_prefix}
 
%install
pwd
rm -rf $RPM_BUILD_ROOT
cd src
scons BUILDS=x86 PREFIX=${RPM_BUILD_ROOT}%{nidas_prefix} install

%post

%clean
rm -rf $RPM_BUILD_ROOT

%files
%{nidas_prefix}/x86/bin
%{nidas_prefix}/x86/lib
%{nidas_prefix}/x86/linux
%{nidas_prefix}/share/xml

%files devel
%{nidas_prefix}/x86/include

%changelog
* Wed Mar  3 2010 Gordon Maclean <maclean@ucar.edu> 1.0-1
- original

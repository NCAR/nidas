%define nidas_prefix /opt/local/nidas

Summary: NCAR In-Situ Data Acquistion Software binaries and configuration XML schema.
Name: nidas-bin
Version: 1.0
Release: 1%{?dist}
License: GPL
Group: Applications/Engineering
Url: http://www.eol.ucar.edu/
Packager: Gordon Maclean <maclean@ucar.edu>
# becomes RPM_BUILD_ROOT, except on newer versions of rpmbuild
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
# Allow this package to be relocatable to other places than /opt/local/nidas/x86
# rpm --relocate /opt/local/nidas/x86=/usr --relocate /opt/local/nidas/share=/usr/share
Prefix: /opt/local/nidas/x86
Prefix: /opt/local/nidas/share
Prefix: /opt/local/nidas/x86/linux
%description devel
NIDAS C/C++ header files.

# Source: %{name}-%{version}.tar.gz

%prep
%setup -n nidas

%build
pwd
cd src
scons BUILDS=x86 PREFIX=${RPM_BUILD_ROOT}%{nidas_prefix}
 
%install
rm -rf $RPM_BUILD_ROOT
cd src
scons BUILDS=x86 PREFIX=${RPM_BUILD_ROOT}%{nidas_prefix} install

%post
ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
# We don't list directories here, so that the package can be relocated
# to /usr/bin, for example.
%{nidas_prefix}/x86/bin/auto_cal
%{nidas_prefix}/x86/bin/ck_aout
%{nidas_prefix}/x86/bin/ck_calfile
%{nidas_prefix}/x86/bin/ck_goes
%{nidas_prefix}/x86/bin/ck_lams
%{nidas_prefix}/x86/bin/ck_xml
%{nidas_prefix}/x86/bin/data_dump
%{nidas_prefix}/x86/bin/data_stats
%{nidas_prefix}/x86/bin/dsm
%{nidas_prefix}/x86/bin/dsm_server
%{nidas_prefix}/x86/bin/extract2d
%{nidas_prefix}/x86/bin/lidar_vel
%{nidas_prefix}/x86/bin/merge_verify
%{nidas_prefix}/x86/bin/n_hdr_util
%{nidas_prefix}/x86/bin/nidsmerge
%{nidas_prefix}/x86/bin/proj_configs
%{nidas_prefix}/x86/bin/pdecode
%{nidas_prefix}/x86/bin/prep
%{nidas_prefix}/x86/bin/rserial
%{nidas_prefix}/x86/bin/sensor_extract
%{nidas_prefix}/x86/bin/sensor_sim
%{nidas_prefix}/x86/bin/sing
%{nidas_prefix}/x86/bin/statsproc
%{nidas_prefix}/x86/bin/status_listener
%{nidas_prefix}/x86/bin/sync_dump
%{nidas_prefix}/x86/bin/sync_server
%{nidas_prefix}/x86/bin/tee_tty
%{nidas_prefix}/x86/bin/utime
%{nidas_prefix}/x86/bin/xml_dump

%{nidas_prefix}/x86/lib/libnidas_util.so
%{nidas_prefix}/x86/lib/libnidas_util.so.*
%{nidas_prefix}/x86/lib/libnidas_util.a
%{nidas_prefix}/x86/lib/libnidas.so
%{nidas_prefix}/x86/lib/libnidas.so.*
%{nidas_prefix}/x86/lib/libnidas_dynld.so
%{nidas_prefix}/x86/lib/libnidas_dynld.so.*
%{nidas_prefix}/x86/lib/nidas_dynld_iss_TiltSensor.so
%{nidas_prefix}/x86/lib/nidas_dynld_iss_WxtSensor.so

%{nidas_prefix}/x86/linux
%{nidas_prefix}/share/xml

%files devel
%{nidas_prefix}/x86/include/nidas

%changelog
* Wed Mar  3 2010 Gordon Maclean <maclean@ucar.edu> 1.0-1
- original

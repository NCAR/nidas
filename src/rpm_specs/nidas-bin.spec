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
Source: %{name}-%{version}.tar.gz
BuildRequires: nidas-x86-build nc_server-devel
Requires: nidas nc_server-devel
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
%setup -n nidas -D

%build
pwd
cd src
scons -j 4 BUILDS=x86 PREFIX=${RPM_BUILD_ROOT}%{nidas_prefix}
 
%install
rm -rf $RPM_BUILD_ROOT

cd src
scons -j 4 BUILDS=x86 PREFIX=${RPM_BUILD_ROOT}%{nidas_prefix} install

cp scripts/nidas_rpm_update.sh ${RPM_BUILD_ROOT}%{nidas_prefix}/x86/bin

%pre

# Add an nidas user and eol group to system, so that installed files on
# /opt/local/nidas are owned and writable by eol

adduser=false
addgroup=false
grep -q ^nidas /etc/passwd || adduser=true
grep -q ^eol /etc/group || addgroup=true

# check if NIS is running. If so, check if nidas.eol is known to NIS
if which ypwhich > /dev/null 2>&1 && ypwhich > /dev/null 2>&1; then
    ypmatch nidas passwd > /dev/null 2>&1 && adduser=false
    ypmatch eol group > /dev/null 2>&1 && addgroup=false
fi

$addgroup && /usr/sbin/groupadd -g 1342 -o eol
$adduser && /usr/sbin/useradd  -u 10035 -o -N -M -g eol -s /sbin/nologin -d /tmp -c NIDAS -K PASS_MAX_DAYS=-1 nidas || :

%post

# remove any old 32-bit libraries on a 64 bit system.
# Should remove this at some point - perhaps someone may
# want both 32 and 64 bit libraries (but we don't have separate
# bin directories).
if [ %{_lib} != lib ]; then
    rm -rf %{nidas_prefix}/x86/lib
fi

ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0775,nidas,eol,2775)
# specify directories so that they will be owned by nidas.eol
{nidas_prefix}/x86
{nidas_prefix}/x86/bin
%{nidas_prefix}/x86/bin/ck_aout
%{nidas_prefix}/x86/bin/ck_calfile
%{nidas_prefix}/x86/bin/ck_goes
%{nidas_prefix}/x86/bin/ck_lams
%{nidas_prefix}/x86/bin/ck_xml
%{nidas_prefix}/x86/bin/data_dump
%{nidas_prefix}/x86/bin/data_stats
%{nidas_prefix}/x86/bin/dmd_mmat_test
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
%{nidas_prefix}/x86/bin/nidas_udp_relay
%{nidas_prefix}/x86/bin/utime
%{nidas_prefix}/x86/bin/xml_dump
%{nidas_prefix}/x86/bin/nidas_rpm_update.sh

{nidas_prefix}/x86/%{_lib}
%{nidas_prefix}/x86/%{_lib}/libnidas_util.so
%{nidas_prefix}/x86/%{_lib}/libnidas_util.so.*
%{nidas_prefix}/x86/%{_lib}/libnidas_util.a
%{nidas_prefix}/x86/%{_lib}/libnidas.so
%{nidas_prefix}/x86/%{_lib}/libnidas.so.*
%{nidas_prefix}/x86/%{_lib}/libnidas_dynld.so
%{nidas_prefix}/x86/%{_lib}/libnidas_dynld.so.*
%{nidas_prefix}/x86/%{_lib}/nidas_dynld_iss_TiltSensor.so
%{nidas_prefix}/x86/%{_lib}/nidas_dynld_iss_TiltSensor.so.*

%{nidas_prefix}/x86/linux
%{nidas_prefix}/share/xml

%files devel
%defattr(0664,nidas,eol,2775)
%{nidas_prefix}/x86/include
%{nidas_prefix}/x86/include/nidas

%changelog
* Wed Mar  3 2010 Gordon Maclean <maclean@ucar.edu> 1.0-1
- original

%define nidas_prefix /opt/nidas
%define scons scons-3

# Command line switches: --with arinc --with modules
%bcond_with arinc
%bcond_with modules

%if %{with arinc}
%define buildarinc BUILD_ARINC=yes
%else
%define buildarinc BUILD_ARINC=no
%endif

%if %{with modules}
%define buildmodules LINUX_MODULES=yes
%else
%define buildmodules LINUX_MODULES=no
%endif

%define has_systemd 0
%{?systemd_requires: %define has_systemd 1}

Summary: NIDAS: NCAR In-Situ Data Acquistion Software
Name: nidas
Version: %{gitversion}
Release: %{releasenum}%{?dist}
License: GPL
Group: Applications/Engineering
Url: https://github.com/ncareol/nidas
Vendor: UCAR
Source: https://github.com/ncareol/%{name}/archive/master.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires: gcc-c++ xerces-c-devel xmlrpc++ bluez-libs-devel bzip2-devel
BuildRequires: flex gsl-devel kernel-devel libcap-devel
BuildRequires: eol_scons >= 4.2
Requires: jsoncpp
BuildRequires: jsoncpp-devel

Requires: nidas-libs
BuildRoot: %{_topdir}/%{name}-%{version}-root
Prefix: %{nidas_prefix}
%description
NCAR In-Situ Data Acquistion Software programs

%package libs
Summary: NIDAS shareable libraries
Group: Applications/Engineering
Obsoletes: nidas-min <= 1.2, nidas-buildeol <= 1.2, nidas-build <= 1.2
Requires: xerces-c xmlrpc++
Requires (post): /sbin/ldconfig
Requires (postun): /sbin/ldconfig
%if 0%{?fedora} > 28
Requires (post): libselinux-utils policycoreutils-python-utils policycoreutils
Requires (postun): libselinux-utils policycoreutils-python-utils policycoreutils
%else
%if 0%{?rhel} < 8
Requires (post): policycoreutils-python
Requires (postun): policycoreutils-python
%else
Requires (post): python3-policycoreutils
Requires (postun): python3-policycoreutils
%endif
%endif

Prefix: %{nidas_prefix}
%description libs
NIDAS shareable libraries

%if %{with modules}
%package modules
Summary: NIDAS kernel modules
Group: Applications/Engineering
Requires: nidas
Prefix: %{nidas_prefix}
%description modules
NIDAS kernel modules.
%endif

%package daq
Summary: Package for doing data acquisition with NIDAS.
# remove dist from release on noarch RPM
Release: %{releasenum}
Requires: nidas
Group: Applications/Engineering
BuildArch: noarch
%description daq
Package for doing data acquisition with NIDAS.  Contains some udev rules to
expand permissions on /dev/tty[A-Z]* and /dev/usbtwod*.
Edit /etc/default/nidas-daq to specify the desired user
to run NIDAS real-time data acquisition processes.

%package devel
Summary: Headers, symbolic links and pkg-config for building software which uses NIDAS.
Requires: nidas-libs libcap-devel
Group: Applications/Engineering
# Prefix: %%{nidas_prefix}
%description devel
NIDAS C/C++ headers, shareable library links, pkg-config.

%prep
%setup -q -c


%build

cd src
%{scons} -j 4 --config=force gitinfo=off BUILD=host \
 REPO_TAG=v%{version} %{buildarinc} %{buildmodules} \
 PREFIX=%{nidas_prefix} PKGCONFIGDIR=%{_libdir}/pkgconfig \
 SYSCONFIGDIR=%{_sysconfdir}

%install
rm -rf $RPM_BUILD_ROOT

cd src
%{scons} -j 4 --config=force gitinfo=off BUILD=host \
 REPO_TAG=v%{version} %{buildarinc} %{buildmodules} \
 PREFIX=%{nidas_prefix} PKGCONFIGDIR=%{_libdir}/pkgconfig \
 SYSCONFIGDIR=%{_sysconfdir} \
 INSTALL_ROOT=$RPM_BUILD_ROOT install install.root
cd -

install -m 0755 -d $RPM_BUILD_ROOT/usr/lib/udev/rules.d
install -m 0664 pkg_files/udev/rules.d/* $RPM_BUILD_ROOT/usr/lib/udev/rules.d

cp -r pkg_files/systemd ${RPM_BUILD_ROOT}%{nidas_prefix}

install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/default
install -m 0664 pkg_files/root/etc/default/nidas-* $RPM_BUILD_ROOT%{_sysconfdir}/default

%post libs

# If selinux is Enforcing, ldconfig can fail with permission denied if the
# policy and file contexts are not right on the libaries. Set the file context of
# library directory and contents to lib_t. I'm not sure at this point
# that this solves the whole issue, or whether a policy change is also required.
# There is some mystery in that ldconfig from root's interactive session never
# seems to fail with permission denied, but does fail from other contexts.
# During SCP, several times (probably after an rpm update) the nidas libs were
# not in the ld cache. I added ldconfig to rc.local and a crontab, and sometimes
# those failed with permission problems related to SELinux and /opt/nidas/{lib,lib64}.
# 
# The following is found in /etc/selinux/targeted/contexts/files/file_contexts
# /opt/(.*/)?lib(/.*)?	system_u:object_r:lib_t:s0
# in selinux-policy-targeted-3.14.2-57.fc29
# Looks like it doesn't match lib64 in /opt/nidas/lib64
# 
# To view:
# semanage fcontext --list -C | fgrep /opt/nidas
# /opt/(.*/)?var/lib(/.*)?  all files system_u:object_r:var_lib_t:s0

if /sbin/selinuxenabled; then
    /sbin/semanage fcontext -a -t lib_t %{nidas_prefix}/%{_lib}"(/.*)?" 2>/dev/null || :
    /sbin/restorecon -R %{nidas_prefix}/%{_lib} || :
fi
/sbin/ldconfig

%postun libs
if [ $1 -eq 0 ]; then # final removal
    /sbin/semanage fcontext -d -t lib_t %{nidas_prefix}/%{_lib}"(/.*)?" 2>/dev/null || :
fi

# If selinux is Enforcing, ldconfig can fail with permission denied if the
# policy and file contexts are not right. Set the file context of
# library directory and contents to lib_t. I'm not sure at this point
# that this solves the whole issue, or whether a policy change is also required.
# There is some mystery in that ldconfig from root's interactive session never
# seems to fail with permission denied, but does fail from other contexts.
# During SCP, several times (probably after an rpm update) the nidas libs were
# not in the ld cache. I added ldconfig to rc.local and a crontab, and sometimes
# those failed with permission problems related to SELinux and /opt/nidas/{lib,lib64}.
# 
# The following is found in /etc/selinux/targeted/contexts/files/file_contexts
# /opt/(.*/)?lib(/.*)?	system_u:object_r:lib_t:s0
# in selinux-policy-targeted-3.14.2-57.fc29
# Looks like it doesn't match lib64 in /opt/nidas/lib64
# 
# To view:
# semanage fcontext --list -C | fgrep /opt/nidas
# /opt/(.*/)?var/lib(/.*)?  all files system_u:object_r:var_lib_t:s0
#
# (gjg) I'm not sure about this approach, since the context needs to be
# installed even if selinux happens to be disabled at the moment.  The
# suggestion at the link below is to put the selinux contexts into a
# separate -selinux package, so they do not need to be installed on systems
# without selinux.  (I don't know if this is still current, but there are
# still examples of -selinux packages.)
#
# https://fedoraproject.org/wiki/PackagingDrafts/SELinux#File_contexts

if /sbin/selinuxenabled; then
    /sbin/semanage fcontext -a -t lib_t %{nidas_prefix}/%{_lib}"(/.*)?" 2>/dev/null || :
    /sbin/restorecon -R %{nidas_prefix}/%{_lib} || :
fi
/sbin/ldconfig

%pre daq
if [ "$1" -eq 1 ]; then
    echo "Edit %{_sysconfdir}/default/nidas-daq to set the DAQ_USER and DAQ_GROUP"
fi

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0775,root,root,2775)
%dir %{nidas_prefix}
%{nidas_prefix}/bin/arinc_out
%{nidas_prefix}/bin/ck_aout
%{nidas_prefix}/bin/ck_calfile
%{nidas_prefix}/bin/ck_xml
%{nidas_prefix}/bin/data_dump
%{nidas_prefix}/bin/data_stats
%{nidas_prefix}/bin/datasets
%{nidas_prefix}/bin/dmd_mmat_vin_limit_test
%{nidas_prefix}/bin/dsc_a2d_ck
%caps(cap_sys_nice,cap_net_admin+p) %{nidas_prefix}/bin/dsm_server
%caps(cap_sys_nice,cap_net_admin+p) %{nidas_prefix}/bin/dsm
%caps(cap_sys_nice,cap_net_admin+p) %{nidas_prefix}/bin/nidas_udp_relay
%caps(cap_sys_nice+p) %{nidas_prefix}/bin/tee_tty
%caps(cap_sys_nice+p) %{nidas_prefix}/bin/tee_i2c
%{nidas_prefix}/bin/extract2d
%{nidas_prefix}/bin/extractDMT
%{nidas_prefix}/bin/ir104
%{nidas_prefix}/bin/lidar_vel
%{nidas_prefix}/bin/merge_verify
%{nidas_prefix}/bin/n_hdr_util
%{nidas_prefix}/bin/nidsmerge
%{nidas_prefix}/bin/arl-ingest
%{nidas_prefix}/bin/proj_configs
%{nidas_prefix}/bin/prep
%{nidas_prefix}/bin/rserial
%{nidas_prefix}/bin/sensor_extract
%{nidas_prefix}/bin/sensor_sim
%{nidas_prefix}/bin/sing
%{nidas_prefix}/bin/statsproc
%{nidas_prefix}/bin/status_listener
%{nidas_prefix}/bin/sync_dump
%{nidas_prefix}/bin/sync_server
%{nidas_prefix}/bin/test_irig
%{nidas_prefix}/bin/utime
%{nidas_prefix}/bin/xml_dump
%{nidas_prefix}/bin/data_influxdb

%attr(0664,-,-) %{nidas_prefix}/share/xml/nidas.xsd

%{nidas_prefix}/systemd

%files libs
%defattr(0775,root,root,2775)
%{nidas_prefix}/%{_lib}/libnidas_util.so.*
%{nidas_prefix}/%{_lib}/libnidas.so.*
%{nidas_prefix}/%{_lib}/libnidas_dynld.so.*
# %%{nidas_prefix}/%%{_lib}/nidas_dynld_iss_TiltSensor.so.*
# %%{nidas_prefix}/%%{_lib}/nidas_dynld_iss_WICORSensor.so.*

%defattr(-,root,root,-)
%{_sysconfdir}/ld.so.conf.d/nidas.conf

%if %{with modules}
%files modules
%defattr(0775,root,root,2775)
%if %{with arinc}
%{nidas_prefix}/modules/arinc.ko
%endif
%{nidas_prefix}/modules/dmd_mmat.ko
%{nidas_prefix}/modules/emerald.ko
%{nidas_prefix}/modules/gpio_mm.ko
%{nidas_prefix}/modules/ir104.ko
%{nidas_prefix}/modules/lamsx.ko
%{nidas_prefix}/modules/mesa.ko
%{nidas_prefix}/modules/ncar_a2d.ko
%{nidas_prefix}/modules/nidas_util.ko
%{nidas_prefix}/modules/pc104sg.ko
%{nidas_prefix}/modules/pcmcom8.ko
%{nidas_prefix}/modules/short_filters.ko
%{nidas_prefix}/modules/usbtwod.ko
%{nidas_prefix}/firmware
%endif

%files daq
%defattr(0775,root,root,0775)
%config /usr/lib/udev/rules.d/99-nidas.rules
%config(noreplace) %{_sysconfdir}/default/nidas-daq

%files devel
%defattr(0664,root,root,2775)
%{nidas_prefix}/include/nidas/Config.h
%{nidas_prefix}/include/nidas/Revision.h
%{nidas_prefix}/include/nidas/util
%{nidas_prefix}/include/nidas/core
%{nidas_prefix}/include/nidas/dynld
%{nidas_prefix}/include/nidas/linux
%{nidas_prefix}/%{_lib}/libnidas_util.so
%{nidas_prefix}/%{_lib}/libnidas_util.a
%{nidas_prefix}/%{_lib}/libnidas.so
%{nidas_prefix}/%{_lib}/libnidas_dynld.so
%config %{nidas_prefix}/%{_lib}/pkgconfig/nidas.pc
%config %{_libdir}/pkgconfig/nidas.pc
%attr(0775,-,-) %{nidas_prefix}/bin/start_podman

%changelog

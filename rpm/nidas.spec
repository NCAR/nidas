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
%if %{defined gitversion}
Version: %{gitversion}
%else
Version: 1.2.3
%endif
Release: %{releasenum}%{?dist}
License: GPL
Group: Applications/Engineering
Obsoletes: nidas-daq <= 1.2
Url: https://github.com/ncareol/nidas
Vendor: UCAR
Source: https://github.com/ncareol/%{name}/archive/master.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires: gcc-c++ xerces-c-devel xmlrpc++ bluez-libs-devel bzip2-devel
BuildRequires: flex gsl-devel kernel-devel libcap-devel
# Allow eol_scons requirement to be met with a local checkout and not only as
# an installed package.  Building NIDAS does require one or the other.
# BuildRequires: eol_scons >= 4.2
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

%package devel
Summary: Headers, symbolic links and pkg-config for building software which uses NIDAS.
Requires: nidas-libs libcap-devel
Group: Applications/Engineering
# Prefix: %%{nidas_prefix}
%description devel
NIDAS C/C++ headers, shareable library links, pkg-config.

%prep
%if %{defined gitversion}
%setup -q -c
%else
%setup
%endif

%build

%{scons} -C src -j 4 --config=force gitinfo=off BUILD=host \
 REPO_TAG=v%{version} %{buildarinc} %{buildmodules} \
 PREFIX=%{nidas_prefix} PKGCONFIGDIR=%{_libdir}/pkgconfig \
 SYSCONFIGDIR=%{_sysconfdir}

%install
rm -rf $RPM_BUILD_ROOT

%{scons} -C src -j 4 --config=force gitinfo=off BUILD=host \
 REPO_TAG=v%{version} %{buildarinc} %{buildmodules} \
 PREFIX=%{nidas_prefix} PKGCONFIGDIR=%{_libdir}/pkgconfig \
 SYSCONFIGDIR=%{_sysconfdir} \
 INSTALL_ROOT=$RPM_BUILD_ROOT install install.root

%post libs
# Separate lib64 context is no longer needed, so make sure it gets removed.
/sbin/semanage fcontext -d -t lib_t %{nidas_prefix}/lib64"(/.*)?" 2>/dev/null || :
/sbin/ldconfig

%postun libs
/sbin/ldconfig

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
%{nidas_prefix}/bin/dsm.init
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
%{nidas_prefix}/bin/cktty
%{nidas_prefix}/bin/irout
%{nidas_prefix}/bin/irqs
%{nidas_prefix}/bin/vout
%{nidas_prefix}/bin/setup_nidas.sh
%{nidas_prefix}/systemd

%attr(0664,-,-) %{nidas_prefix}/share/xml/nidas.xsd

%files libs
%defattr(0775,root,root,2775)
%{nidas_prefix}/lib/libnidas_util.so.*
%{nidas_prefix}/lib/libnidas.so.*
%{nidas_prefix}/lib/libnidas_dynld.so.*

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
%{_sysconfdir}/init.d/emerald
%{_sysconfdir}/init.d/pcmcom8
%{_sysconfdir}/default/emerald
%{_sysconfdir}/default/pcmcom8
%{_sysconfdir}/modprobe.d/diamond.conf
%{_sysconfdir}/modprobe.d/nidas.conf
%{_sysconfdir}/modprobe.d/pcmcom8.conf
/lib/udev/rules.d/99-nidas.rules
/lib/modules/*/nidas/*.ko
%endif

%files devel
%defattr(0664,root,root,2775)
%{nidas_prefix}/include/nidas/Config.h
%{nidas_prefix}/include/nidas/Revision.h
%{nidas_prefix}/include/nidas/util
%{nidas_prefix}/include/nidas/core
%{nidas_prefix}/include/nidas/dynld
%{nidas_prefix}/include/nidas/linux
%{nidas_prefix}/lib/libnidas_util.so
%{nidas_prefix}/lib/libnidas_util.a
%{nidas_prefix}/lib/libnidas.so
%{nidas_prefix}/lib/libnidas_dynld.so
%config %{nidas_prefix}/lib/pkgconfig/nidas.pc
%config %{_libdir}/pkgconfig/nidas.pc
%attr(0775,-,-) %{nidas_prefix}/bin/start_podman

%changelog
* Sat Mar 02 2024 Gary Granger <granger@ucar.edu> - 1.2.3-1
- package 1.2.3
  https://github.com/ncareol/nidas/releases/tag/v1.2.3

* Wed Dec 13 2023 Gary Granger <granger@ucar.edu> - 1.2.2-1
- update package to v1.2.2:
  https://github.com/ncareol/nidas/releases/tag/v1.2.2

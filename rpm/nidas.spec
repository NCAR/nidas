%define nidas_prefix /opt/nidas

# Command line switches:  --with configedit --with autocal
# If not specified, configedit or autocal package will not be built
%bcond_with configedit
%bcond_with autocal
%bcond_with raf

%if %{with raf}
%define buildraf BUILD_RAF=yes
%else
%define buildraf BUILD_RAF=no
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
BuildRequires: gcc-c++ scons xerces-c-devel xmlrpc++ bluez-libs-devel bzip2-devel flex gsl-devel kernel-devel libcap-devel qt-devel eol_scons
Requires: yum-utils nidas-min
Obsoletes: nidas-bin <= 1.0
BuildRoot: %{_topdir}/%{name}-%{version}-root
# Allow this package to be relocatable to other places than /opt/nidas
# rpm --relocate /opt/nidas=/usr
Prefix: %{nidas_prefix}
%description
NCAR In-Situ Data Acquistion Software programs

%package min
Summary: Minimal NIDAS run-time configuration, and pkg-config file.
Group: Applications/Engineering
Obsoletes: nidas <= 1.0, nidas-run <= 1.0
Requires: xerces-c xmlrpc++
%description min
Minimal run-time setup for NIDAS: /etc/ld.so.conf.d/nidas.conf. Useful on systems
that NFS mount %{nidas_prefix}, or do their own builds.  Also creates /usr/lib[64]/pkgconfig/nidas.pc.

%package libs
Summary: NIDAS shareable libraries
Group: Applications/Engineering
Requires: nidas-min
Prefix: %{nidas_prefix}
%description libs
NIDAS shareable libraries

%package modules
Summary: NIDAS kernel modules
Group: Applications/Engineering
Requires: nidas
Prefix: %{nidas_prefix}
%description modules
NIDAS kernel modules.

%if %{with autocal}

%package autocal
Summary: Auto-calibration program, with Qt GUI, for NCAR RAF A2D board
Requires: nidas
Group: Applications/Engineering
Prefix: %{nidas_prefix}
%description autocal
Auto-calibration program, with Qt GUI, for NCAR A2D board.

%endif

%if %{with configedit}

%package configedit
Summary: GUI editor for NIDAS configurations
Requires: nidas
Group: Applications/Engineering
Prefix: %{nidas_prefix}
%description configedit
GUI editor for NIDAS configurations

%endif

%package daq
Summary: Package for doing data acquisition with NIDAS.
# remove %{dist} from %{release} on noarch RPM
Release: %{releasenum}
Requires: nidas-min
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
Obsoletes: nidas-bin-devel <= 1.0
Group: Applications/Engineering
# Prefix: %{nidas_prefix}
%description devel
NIDAS C/C++ headers, shareable library links, pkg-config.

%package build
Summary: Package for building NIDAS by hand
# remove %{dist} from %{release} on noarch RPM
Release: %{releasenum}
Group: Applications/Engineering
Requires: gcc-c++ scons xerces-c-devel xmlrpc++ bluez-libs-devel bzip2-devel flex gsl-devel kernel-devel libcap-devel qt-devel eol_scons
Obsoletes: nidas-builduser <= 1.2-189
BuildArch: noarch
%description build
Contains software dependencies needed to build NIDAS by hand,
and /etc/default/nidas-build containing the desired user and group owner
of %{nidas_prefix}.

%package buildeol
Summary: Set build user and group to nidas.eol.
# remove %{dist} from %{release} on noarch RPM
Release: %{releasenum}
Group: Applications/Engineering
Requires: nidas-build
BuildArch: noarch
%description buildeol
Sets BUILD_GROUP=eol in /etc/default/nidas-build so that %{nidas_prefix} will be group writable by eol.

%prep
%setup -q -c

%build
cd src
scons -j 4 --config=force BUILDS=host REPO_TAG=v%{version} %{buildraf} PREFIX=%{nidas_prefix}
 
%install
rm -rf $RPM_BUILD_ROOT

cd src
scons -j 4 BUILDS=host PREFIX=${RPM_BUILD_ROOT}%{nidas_prefix} %{buildraf} REPO_TAG=v%{version} install
cd -

install -d ${RPM_BUILD_ROOT}%{_sysconfdir}/ld.so.conf.d
echo "%{nidas_prefix}/%{_lib}" > $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nidas.conf

install -m 0755 -d $RPM_BUILD_ROOT%{_libdir}/pkgconfig
# scons puts entire $RPM_BUILD_ROOT string in nidas.pc, remove it for package
sed -r -i "s,$RPM_BUILD_ROOT,," \
        $RPM_BUILD_ROOT%{nidas_prefix}/%{_lib}/pkgconfig/nidas.pc

cp $RPM_BUILD_ROOT%{nidas_prefix}/%{_lib}/pkgconfig/nidas.pc \
        $RPM_BUILD_ROOT%{_libdir}/pkgconfig

install -m 0755 -d $RPM_BUILD_ROOT%{nidas_prefix}/scripts
install -m 0775 pkg_files%{nidas_prefix}/scripts/* $RPM_BUILD_ROOT%{nidas_prefix}/scripts

# install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/init.d
# install -m 0775 pkg_files/root/etc/init.d/* $RPM_BUILD_ROOT%{_sysconfdir}/init.d

install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/profile.d
install -m 0664 pkg_files/root/etc/profile.d/* $RPM_BUILD_ROOT%{_sysconfdir}/profile.d

install -m 0755 -d $RPM_BUILD_ROOT/usr/lib/udev/rules.d
install -m 0664 pkg_files/udev/rules.d/* $RPM_BUILD_ROOT/usr/lib/udev/rules.d

cp -r pkg_files/systemd ${RPM_BUILD_ROOT}%{nidas_prefix}

install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/default
install -m 0664 pkg_files/root/etc/default/nidas-* $RPM_BUILD_ROOT%{_sysconfdir}/default

%post min

/sbin/ldconfig

%post libs

# If selinux is Enforcing, ldconfig can fail with permission denied if the
# policy and file contexts are not right. Set the file context of
# library directory and contents to lib_t. I'm not sure at this point
# that this solves the whole issue, or whether a policy change is also required.
# There is some mystery in that ldconfig from root's interactive session never
# seems to fail with permission denied, but does fail from other contexts.
# During SCP, several times (probably after an rpm update) the nidas libs were
# not in the ld cache. I added ldconfig to rc.local and a crontab, and sometimes
# those failed with permission problems related to SELinux and /opt/nidas/{lib,lib64}.
if selinuxenabled; then
    /usr/sbin/semanage fcontext -a -t lib_t %{nidas_prefix}/%{_lib}"(/.*)?"
    /sbin/restorecon -R %{nidas_prefix}/%{_lib}
fi
/sbin/ldconfig

%pre daq
if [ "$1" -eq 1 ]; then
    echo "Edit %{_sysconfdir}/default/nidas-daq to set the DAQ_USER and DAQ_GROUP"
fi

%pre build
if [ $1 -eq 1 ]; then
    echo "Set BUILD_USER and BUILD_GROUP in %{_sysconfdir}/default/nidas-build.
Files installed in %{nidas_prefix} will then be owned by that user and group"
fi

%triggerin -n nidas-build -- nidas nidas-libs nidas-devel nidas-modules nidas-buildeol nidas-doxygen nidas-configedit nidas-autocal

[ -d %{nidas_prefix} ] || mkdir -p -m u=rwx,g=rwxs,o=rx %{nidas_prefix}

cf=%{_sysconfdir}/default/nidas-build

if [ -f $cf ]; then

    .  $cf 

    echo "nidas-build trigger: BUILD_USER=$BUILD_USER, BUILD_GROUP=$BUILD_GROUP read from $cf"

    if [ "$BUILD_USER" != root -o "$BUILD_GROUP" != root ]; then

        n=$(find %{nidas_prefix} \( \! -user $BUILD_USER -o \! -group $BUILD_GROUP \) -execdir chown -h $BUILD_USER:$BUILD_GROUP {} + -print | wc -l)

        find %{nidas_prefix} \! -type l \! -perm /g+w -execdir chmod g+w {} +

        [ $n -gt 0 ] && echo "nidas-build trigger: ownership of $n files under %{nidas_prefix} set to $BUILD_USER.$BUILD_GROUP, with group write"

        # chown on a file removes any associated capabilities
        if [ -x /usr/sbin/setcap ]; then
            arg="cap_sys_nice,cap_net_admin+p" 
            ckarg=$(echo $arg | cut -d, -f 1 | cut -d+ -f 1)

            for prog in %{nidas_prefix}/bin/{dsm_server,dsm,nidas_udp_relay}; do
                if [ -f $prog ] && ! getcap $prog | grep -F -q $ckarg; then
                    echo "nidas-build trigger: setcap $arg $prog"
                    setcap $arg $prog
                fi
            done
        fi
    fi
fi

%post buildeol
cf=%{_sysconfdir}/default/nidas-build 
. $cf
if [ "$BUILD_GROUP" != eol ]; then
    sed -i -r -e 's/^ *BUILD_GROUP=.*/BUILD_GROUP=eol/g' $cf
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
%{nidas_prefix}/bin/dmd_mmat_test
%caps(cap_sys_nice,cap_net_admin+p) %{nidas_prefix}/bin/dsm_server
%caps(cap_sys_nice,cap_net_admin+p) %{nidas_prefix}/bin/dsm
%caps(cap_sys_nice,cap_net_admin+p) %{nidas_prefix}/bin/nidas_udp_relay
%{nidas_prefix}/bin/extract2d
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
%{nidas_prefix}/bin/tee_tty
%{nidas_prefix}/bin/utime
%{nidas_prefix}/bin/xml_dump
%{nidas_prefix}/scripts/*

%config(noreplace) %{_sysconfdir}/profile.d/nidas.sh
%config(noreplace) %{_sysconfdir}/profile.d/nidas.csh

%attr(0664,-,-) %{nidas_prefix}/share/xml/nidas.xsd

%{nidas_prefix}/systemd

%files libs
%defattr(0775,root,root,2775)
%{nidas_prefix}/%{_lib}/libnidas_util.so.*
%{nidas_prefix}/%{_lib}/libnidas.so.*
%{nidas_prefix}/%{_lib}/libnidas_dynld.so.*
# %{nidas_prefix}/%{_lib}/nidas_dynld_iss_TiltSensor.so.*
# %{nidas_prefix}/%{_lib}/nidas_dynld_iss_WICORSensor.so.*

%files modules
%defattr(0775,root,root,2775)
%{nidas_prefix}/modules/arinc.ko
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

%if %{with autocal}
%files autocal
%defattr(0775,root,root,2775)
%{nidas_prefix}/bin/auto_cal
%endif

%if %{with configedit}
%files configedit
%defattr(0775,root,root,2775)
%{nidas_prefix}/bin/configedit
%endif

%files min
%defattr(-,root,root,-)
%{_sysconfdir}/ld.so.conf.d/nidas.conf

%files daq
%defattr(0775,root,root,0775)
%config /usr/lib/udev/rules.d/99-nidas.rules
%config(noreplace) %{_sysconfdir}/default/nidas-daq
# %config(noreplace) %{_sysconfdir}/init.d/dsm_server
# %config(noreplace) %{_sysconfdir}/init.d/dsm

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
# %{nidas_prefix}/%{_lib}/nidas_dynld_iss_TiltSensor.so
# %{nidas_prefix}/%{_lib}/nidas_dynld_iss_WICORSensor.so
%config %{nidas_prefix}/%{_lib}/pkgconfig/nidas.pc
%config %{_libdir}/pkgconfig/nidas.pc

%files build
%defattr(-,root,root,-)
%config(noreplace) %attr(0664,-,-) %{_sysconfdir}/default/nidas-build

%files buildeol

%changelog

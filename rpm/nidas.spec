%define nidas_prefix /opt/nidas

# Command line switches:  --with configedit --with autocal
# If not specified, configedit or autocal package will not be built
%bcond_with configedit
%bcond_with autocal

%define has_systemd 0
%{?systemd_requires: %define has_systemd 1}

Summary: NIDAS: NCAR In-Situ Data Acquistion Software
Name: nidas
Version: %{version}
Release: %{releasenum}%{?dist}
License: GPL
Group: Applications/Engineering
Url: https://github.com/ncareol/nidas
Vendor: UCAR
# Source: %{name}-%{version}.tar.gz
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
Obsoletes: nidas <= 1.0, nidas-run
Requires: xerces-c xmlrpc++
%description min
Minimal run-time setup for NIDAS: /etc/ld.so.conf.d/nidas.conf. Useful on systems
that NFS mount /opt/nidas, or do their own builds.  Also creates /usr/lib[64]/pkgconfig/nidas.pc.

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
Requires: nidas-min
Group: Applications/Engineering
%description daq
Package for doing data acquisition with NIDAS.  Contains some udev rules to
expand permissions on /dev/tty[A-Z]* and /dev/usbtwod*.
Edit /etc/default/nidas-daq to specify the desired user
to run NIDAS real-time data acquisition processes.

%package devel
Summary: Headers, symbolic links and pkg-config for building software which uses NIDAS.
Requires: nidas-libs
Obsoletes: nidas-bin-devel <= 1.0
Group: Applications/Engineering
Prefix: %{nidas_prefix}
%description devel
NIDAS C/C++ headers, shareable library links, pkg-config.

%package build
Summary: Package for building NIDAS for the native architecture systems with scons
Requires: gcc-c++ scons xerces-c-devel xmlrpc++ bluez-libs-devel bzip2-devel flex gsl-devel kernel-devel libcap-devel qt-devel eol_scons nidas-builduser
Group: Applications/Engineering
Prefix: %{nidas_prefix}
Obsoletes: nidas-x86-build <= 1.0
%description build
Requirements for building NIDAS on x86 systems with scons. Changes ownership of
/opt/nidas to the user and group specifid in /var/lib/nidas/BuildUserGroup,
which can first be installed from builduser package, then modified
to match a user and group who will be building NIDAS.

%package builduser
Summary: User and group owner of %{nidas_prefix}
Group: Applications/Engineering
Requires: nidas-build
%description builduser
Contains /var/lib/nidas/BuildUserGroup, which can be modified to specify the
desired user and group owner of /opt/nidas.

%package buildeol
Summary: Set build user and group to nidas.eol.
Group: Applications/Engineering
Requires: nidas-builduser
%description buildeol
Overwrites /var/lib/nidas/BuildUserGroup with "root(0):eol(1342)" so that build tree will group writable by eol.

%prep
%setup -q -c

%build
cd src
scons -j 4 --config=force BUILDS=x86 REPO_TAG=v%{version}
 
%install
rm -rf $RPM_BUILD_ROOT

cd src
scons -j 4 BUILDS=x86 PREFIX=${RPM_BUILD_ROOT}%{nidas_prefix} REPO_TAG=v%{version} install
cd -

install -d ${RPM_BUILD_ROOT}%{_sysconfdir}/ld.so.conf.d

echo "/opt/nidas/%{_lib}" > $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nidas.conf

install -m 0755 -d $RPM_BUILD_ROOT%{_libdir}/pkgconfig

# Create the pkgconfig file that is part of nidas-devel.
# Note that one is also created below by the post script section of
# nidas-min. That should probably be changed so that it is
# owned by nidas-min, since nidas-min is required by nidas-devel.

# the value of %{nidas_prefix} and  %{_lib} will be set to lib or lib64 by rpmbuild
cat << \EOD > $RPM_BUILD_ROOT%{_libdir}/pkgconfig/nidas.pc
prefix=%{nidas_prefix}
libdir=${prefix}/%{_lib}
includedir=${prefix}/include

Name: nidas
Description: NCAR In-Situ Data Acquisition Software
Version: %{version}-%{releasenum}
Libs: -L${libdir} -lnidas_util -lnidas -lnidas_dynld
Cflags: -I${includedir}
Requires: xerces-c,xmlrpcpp
EOD

install -m 0755 -d $RPM_BUILD_ROOT%{nidas_prefix}/scripts
install -m 0775 pkg_files/opt/nidas/scripts/* $RPM_BUILD_ROOT%{nidas_prefix}/scripts

# install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/init.d
# install -m 0775 pkg_files/root/etc/init.d/* $RPM_BUILD_ROOT%{_sysconfdir}/init.d

install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/profile.d
install -m 0664 pkg_files/root/etc/profile.d/* $RPM_BUILD_ROOT%{_sysconfdir}/profile.d

install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/udev/rules.d
install -m 0664 pkg_files/root/etc/udev/rules.d/* $RPM_BUILD_ROOT%{_sysconfdir}/udev/rules.d

cp -r pkg_files/systemd ${RPM_BUILD_ROOT}%{nidas_prefix}

install -m 0755 -d $RPM_BUILD_ROOT%{_sharedstatedir}/nidas
install -m 0664 pkg_files/root%{_sharedstatedir}/nidas/* $RPM_BUILD_ROOT%{_sharedstatedir}/nidas

install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/default
install -m 0664 pkg_files/root/etc/default/nidas-daq $RPM_BUILD_ROOT%{_sysconfdir}/default
%post min

# Create nidas.pc file in the post script of the nidas-min package. That file
# is owned by the nidas-devel package, but we'll provide it here
# for people who build their own nidas, and want the pkg-config file for
# building other software.
# the value of %{nidas_prefix} and  %{_lib} will be replaced by lib or lib64 by rpmbuild
cf=%{_libdir}/pkgconfig/nidas.pc
if [ ! -f $cf ]; then
    cat << \EOD > $cf
prefix=%{nidas_prefix}
libdir=${prefix}/%{_lib}
includedir=${prefix}/include

Name: nidas
Description: NCAR In-Situ Data Acquisition Software
Version: %{version}-%{releasenum}
Libs: -L${libdir} -lnidas_util -lnidas -lnidas_dynld
Cflags: -I${includedir}
Requires: xerces-c,xmlrpcpp
EOD
fi

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

%pre builduser
if [ $1 -eq 1 ]; then
    echo "Edit user(uid):group(gid) in %{_sharedstatedir}/nidas/BuildUserGroup.
Installation of nidas packages will then create the user and group and set ownership of %{nidas_prefix}."
fi

%triggerin -n nidas-builduser -- nidas nidas-libs nidas-devel nidas-modules nidas-build nidas-buildeol 

[ -d %{nidas_prefix} ] || mkdir -p -m u=rwx,g=rwxs,o=rx %{nidas_prefix}

if [ -f %{_sharedstatedir}/nidas/BuildUserGroup ]; then

    # read BuildUserGroup, containing one line with the following format:
    #   user(uid):group(gid)
    # where user and group are alphanumeric names, uid and gid are numeric ids.
    # Also accept a dot betwee user and group.
    delim=:
    user=`cut -d "$delim" -f 1 %{_sharedstatedir}/nidas/BuildUserGroup`
    group=`cut -d "$delim" -f 2 %{_sharedstatedir}/nidas/BuildUserGroup`

    if echo $user | grep -F -q "("; then
        uid=`echo $user | cut -d "(" -f 2 | cut -d ")" -f 1`
        user=`echo $user | cut -d "(" -f 1`
    fi
    if echo $group | grep -F -q "("; then
        gid=`echo $group | cut -d "(" -f 2 | cut -d ")" -f 1`
        group=`echo $group | cut -d "(" -f 1`
    fi

    echo "user=$user, group=$group read from %{_sharedstatedir}/nidas/BuildUserGroup"

    if [ "$user" != root -o "$group" != root ]; then

        # Add a user and group to system, so that installed files on
        # /opt/nidas are owned and writable by the group, rather than root.
        adduser=false
        addgroup=false
        grep -q "^$user" /etc/passwd || adduser=true
        grep -q "^$group" /etc/group || addgroup=true

        # check if NIS is running. If so, check if user.group is known to NIS
        if which ypwhich > /dev/null 2>&1 && ypwhich > /dev/null 2>&1; then
            ypmatch $user passwd > /dev/null 2>&1 && adduser=false
            ypmatch $group group > /dev/null 2>&1 && addgroup=false
        fi

        $addgroup && /usr/sbin/groupadd -g $gid -o eol
        export USERGROUPS_ENAB=no
        $adduser && /usr/sbin/useradd  -u $uid -o -M -g $group -s /sbin/nologin -d /tmp -c "NIDAS build user" -K PASS_MAX_DAYS=-1 $user || :

        n=`find %{nidas_prefix} \( \! -user $user -o \! -group $group \) -execdir chown $user:$group {} + -print | wc -l`
        [ $n -gt 0 ] && echo "Set owner of files under %{nidas_prefix} to $user.$group"

        find %{nidas_prefix} \! -perm /g+w -execdir chmod g+w {} +

        # chown on a file removes any associated capabilities
        if [ -x /usr/sbin/setcap ]; then
            echo "trigger, doing setcap on %{nidas_prefix}/bin/{dsm_server,dsm}"
            setcap cap_sys_nice,cap_net_admin+p %{nidas_prefix}/bin/dsm_server
            setcap cap_sys_nice,cap_net_admin+p %{nidas_prefix}/bin/dsm
        fi
    fi
fi

%post buildeol
if [ "$1" -eq 1 ]; then
    echo "root(0):eol(1342)" > $RPM_BUILD_ROOT%{_sharedstatedir}/nidas/BuildUserGroup
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
%{nidas_prefix}/bin/extract2d
%{nidas_prefix}/bin/ir104
%{nidas_prefix}/bin/lidar_vel
%{nidas_prefix}/bin/merge_verify
%{nidas_prefix}/bin/n_hdr_util
%{nidas_prefix}/bin/nidsmerge
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
%{nidas_prefix}/bin/nidas_udp_relay
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
%{nidas_prefix}/%{_lib}/nidas_dynld_iss_TiltSensor.so.*
%{nidas_prefix}/%{_lib}/nidas_dynld_iss_WICORSensor.so.*

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
%config %{_sysconfdir}/udev/rules.d/99-nidas.rules
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
%{nidas_prefix}/%{_lib}/nidas_dynld_iss_TiltSensor.so
%{nidas_prefix}/%{_lib}/nidas_dynld_iss_WICORSensor.so
%config %{_libdir}/pkgconfig/nidas.pc

%files build

%files builduser
%defattr(-,root,root,-)
%config(noreplace) %attr(0664,-,-) %{_sharedstatedir}/nidas/BuildUserGroup

%files buildeol

%changelog

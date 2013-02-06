%define nidas_prefix /opt/nidas

# Command line switch:  --with configedit.
# If not specified, configedit package will not be built
%bcond_with configedit

Summary: NIDAS: NCAR In-Situ Data Acquistion Software
Name: nidas
Version: 1.1
Release: 0%{?dist}
License: GPL
Group: Applications/Engineering
Url: http://www.eol.ucar.edu/
Vendor: UCAR
Source: %{name}-%{version}.tar.gz
BuildRequires: gcc-c++ scons xerces-c-devel xmlrpc++ bluez-libs-devel bzip2-devel flex gsl-devel kernel-devel libcap-devel nc_server-devel qt-devel qwt-devel
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

%package autocal
Summary: Auto-calibration program, with Qt GUI, for NCAR RAF A2D board
Requires: nidas
Group: Applications/Engineering
Prefix: %{nidas_prefix}
%description autocal
Auto-calibration program, with Qt GUI, for NCAR A2D board.

%if %{with configedit}

%package configedit
Summary: GUI editor for NIDAS configurations
Requires: nidas
Group: Applications/Engineering
Prefix: %{nidas_prefix}
%description configedit
GUI editor for NIDAS configurations

%endif

%package editcal
Summary: GUI editor for calibrations
Requires: nidas
Group: Applications/Engineering
Prefix: %{nidas_prefix}
%description editcal
GUI editor for calibrations

%package daq
Summary: Package for doing data acquisition with NIDAS.
Requires: nidas-min
Group: Applications/Engineering
%description daq
Package for doing data acquisition with NIDAS.  Contains some udev rules to
expand permissions on /dev/tty[A-Z]* and /dev/usbtwod*.
Contains /etc/init.d/nidas-{dsm,dsm_server} boot scripts and /var/lib/nidas/DaqUser
which can be modified to specify the desired user to run NIDAS real-time data
acquisition processes.

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
Requires: gcc-c++ scons xerces-c-devel xmlrpc++ bluez-libs-devel bzip2-devel flex gsl-devel kernel-devel libcap-devel qt-devel
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
Overwrites /var/lib/nidas/BuildUserGroup with "nidas(10035):eol(1342)" so that build tree will be owned by nidas and group writable by eol.

%prep
%setup -q -n nidas -D
# -D means don't clear BUILD directory before untar-ing

# we could do a scons clear:
# cd src
# scons -c BUILDS=x86 

%build
cd src
scons -j 4 BUILDS=x86
 
%install
rm -rf $RPM_BUILD_ROOT

cd src
scons -j 4 BUILDS=x86 PREFIX=${RPM_BUILD_ROOT}%{nidas_prefix} install
cd -

install -d ${RPM_BUILD_ROOT}%{_sysconfdir}/ld.so.conf.d

echo "/opt/nidas/%{_lib}" > $RPM_BUILD_ROOT%{_sysconfdir}/ld.so.conf.d/nidas.conf

install -m 0755 -d $RPM_BUILD_ROOT%{_sharedstatedir}/nidas
echo "root(0):root(0)" > $RPM_BUILD_ROOT%{_sharedstatedir}/nidas/BuildUserGroup
echo "root" > $RPM_BUILD_ROOT%{_sharedstatedir}/nidas/DaqUser

install -m 0755 -d $RPM_BUILD_ROOT%{_libdir}/pkgconfig
# the value of %{nidas_prefix} and  %{_lib} will be set to lib or lib64 by rpmbuild
cat << \EOD > $RPM_BUILD_ROOT%{_libdir}/pkgconfig/nidas.pc
prefix=%{nidas_prefix}
libdir=${prefix}/%{_lib}
includedir=${prefix}/include

Name: nidas
Description: NCAR In-Situ Data Acquisition Software
Version: 1.1-0
Libs: -L${libdir} -lnidas_util -lnidas -lnidas_dynld
Cflags: -I${includedir}
Requires: xerces-c,xmlrpcpp
EOD

install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/init.d
cp etc/init.d/* $RPM_BUILD_ROOT%{_sysconfdir}/init.d

install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/profile.d
cp etc/profile.d/* $RPM_BUILD_ROOT%{_sysconfdir}/profile.d

install -m 0755 -d $RPM_BUILD_ROOT%{_sysconfdir}/udev/rules.d
cp etc/udev/rules.d/* $RPM_BUILD_ROOT%{_sysconfdir}/udev/rules.d

install -m 0775 -d $RPM_BUILD_ROOT%{_localstatedir}/run/nidas

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
Version: 1.1-0
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
    echo "Edit %{_sharedstatedir}/nidas/DaqUser to specify the user to run NIDAS processes and own %{_localstatedir}/run/nidas"
fi

%post daq

user=`cut -d "$delim" -f 1 %{_sharedstatedir}/nidas/DaqUser`
if [ -n "$user" -a "$user" != root ]; then
    echo "user=$user read from %{_sharedstatedir}/nidas/DaqUser"
    chown -R $user %{_localstatedir}/run/nidas
    group=`id -gn $user`
    if [ -n "$group" ]; then
        chgrp -R $group %{_localstatedir}/run/nidas
    fi
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
    if ! grep -F -q "$delim" %{_sharedstatedir}/nidas/BuildUserGroup; then
        grep -F -q "." %{_sharedstatedir}/nidas/BuildUserGroup && delim=.
    fi

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

    if [ "$user" != root ]; then

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
    fi
fi

%post buildeol
if [ "$1" -eq 1 ]; then
    echo "nidas(10035):eol(1342)" > $RPM_BUILD_ROOT%{_sharedstatedir}/nidas/BuildUserGroup
fi

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0775,root,root,2775)
%dir %{nidas_prefix}
%{nidas_prefix}/bin/arinc_out
%{nidas_prefix}/bin/ck_aout
%{nidas_prefix}/bin/ck_calfile
%{nidas_prefix}/bin/ck_goes
%{nidas_prefix}/bin/ck_xml
%{nidas_prefix}/bin/data_dump
%{nidas_prefix}/bin/data_stats
%{nidas_prefix}/bin/dmd_mmat_test
%{nidas_prefix}/bin/dsm
%{nidas_prefix}/bin/dsm_server
%{nidas_prefix}/bin/extract2d
%{nidas_prefix}/bin/ir104
%{nidas_prefix}/bin/lidar_vel
%{nidas_prefix}/bin/merge_verify
%{nidas_prefix}/bin/n_hdr_util
%{nidas_prefix}/bin/nidsmerge
%{nidas_prefix}/bin/proj_configs
%{nidas_prefix}/bin/pdecode
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
%{nidas_prefix}/bin/nidas_rpm_update.sh

%config(noreplace) %{_sysconfdir}/profile.d/nidas.sh
%config(noreplace) %{_sysconfdir}/profile.d/nidas.csh

%attr(0664,-,-) %{nidas_prefix}/share/xml/nidas.xsd

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

%files autocal
%defattr(0775,root,root,2775)
%{nidas_prefix}/bin/auto_cal

%if %{with configedit}
%files configedit
%defattr(0775,root,root,2775)
%{nidas_prefix}/bin/configedit
%endif

%files editcal
%defattr(0775,root,root,2775)
%{nidas_prefix}/bin/edit_cal

%files min
%defattr(-,root,root,-)
%{_sysconfdir}/ld.so.conf.d/nidas.conf

%files daq
%defattr(0775,root,root,0775)
%config %{_sysconfdir}/udev/rules.d/99-nidas.rules
%config(noreplace) %{_sharedstatedir}/nidas/DaqUser
%config(noreplace) %{_sysconfdir}/init.d/nidas-dsm_server
%config(noreplace) %{_sysconfdir}/init.d/nidas-dsm
# directory for /var/run/nidas pid files
%dir %{_localstatedir}/run/nidas

%files devel
%defattr(0664,root,root,2775)
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
* Mon Dec  5 2011 Gordon Maclean <maclean@ucar.edu> 1.1-0
- Rework of package structure and installation directory:
- /opt/nidas/{bin,lib[64],modules,share,arm,armbe}.
- arm and armbe directories have bin,lib,modules subdirectories.
* Wed Mar  3 2010 Gordon Maclean <maclean@ucar.edu> 1.0-1
- original

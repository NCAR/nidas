Summary: Metapackage for installing Arcom Embedded Linux (AEL) for NIDAS development.
Name: nidas-ael
Version: 1.0
Release: 5
License: GPL
Group: Development
BuildArch: noarch

Requires: ael-base

Requires: xmlrpc++-cross-arm-linux
Requires: xmlrpc++-cross-armbe-linux

Requires: xerces-c-cross-arm-linux
Requires: xerces-c-cross-devel-arm-linux

Requires: xerces-c-cross-armbe-linux
Requires: xerces-c-cross-devel-armbe-linux

Requires: bzip2-cross-arm-linux
Requires: bzip2-cross-armbe-linux

# Kernel trees
Requires: lsb-arcom-viper-linux-source-2.6.16.28-arcom1
Requires: lsb-arcom-vulcan-linux-source-2.6.21.7-ael1

%description
Package with dependencies needed for NIDAS cross development for
Arcom Embedded Linux (AEL) targets (Viper and Vulcan).

%files

%changelog
* Sat Nov 28 2009 Gordon Maclean <maclean@ucar.edu> 1.0-5
- Added bzip2-cross-{arm,armbe}
* Tue May 12 2009 Gordon Maclean <maclean@ucar.edu> 1.0-4
- Removed xmlrpc++ and xerces-c-devel dependencies and udev rules.
* Thu Feb 21 2008 Gordon Maclean <maclean@ucar.edu> 1.0-3
- Added etc/udev/rules.d/99-nidas.rules
- This eventually should go in a separate nidas-base package
* Thu Jan 31 2008 Gordon Maclean <maclean@ucar.edu> 1.0-2
- Split base AEL stuff off to ael-base
- Added kernel source trees
* Tue Jan 15 2008 Gordon Maclean <maclean@ucar.edu> 1.0-1
- Initial hack


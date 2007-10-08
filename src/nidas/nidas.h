/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ********************************************************************

*/

/*!
  \mainpage NIDAS
  \section intro_sec Introduction
  NCAR In-Situ Data Acquisition Software
  \section install_both How to Build and Install NIDAS for ARM and Intel X86

  NIDAS uses the xerces-c and xmlrpc++ software packages. Building NIDAS
  requires access to the xerces-c and xmlrpc++ header files and libraries.

  The path the installation of the X86 version of these packages can
  be specified with the OTHER_X86 option to scons. Set OTHER_X86
  to the name of the parent directory of the 'include' and 'lib' directories
  containing the xerces-c and xmlrpc++ header files and libraries.

  The default value of OTHER_X86 is $PREFIX/x86, where PREFIX can
  be set as the destination the installation.  The default value
  of PREFIX is /opt/local/nidas.

  The path the installation of the ARM version of these packages can
  be specified with the OTHER_ARM option to scons. 
  The default value of OTHER_ARM is $PREFIX/arm.

  When building for ARM, make sure arm-linux-g++, and the other arm tools are available and in your path. For example:
  - sh/bash:  PATH=/opt/arcom/bin:$PATH
  - csh/tcsh: set path = (/opt/arcom/bin $path)

  The RTLinux modules in nidas/rtlinux are also built when building for arm.  The the RTLinux ARM cross-development tools must be installed in /opt/rtldk-2.2.

  - cd src
  - scons PREFIX=/install_path OTHER_X86=/usr/local OTHER_ARM=/usr/local_arm
  - scons install

  If the xerces-c and xmlrpc++ software is installed somewhere other than $PREFIX/x86 and $PREFIX/arm, use the OTHER_X86 and OTHER_ARM options:

  - cd src
  - scons PREFIX=/install_path OTHER_X86=/usr/local OTHER_ARM=/usr/local_arm
  - scons install

  /install_path is where you want to install NIDAS. The default value for PREFIX is /opt/local/nidas.
  Executables, libraries and header files will be installed as follows:

  <dl>
  <dt>$PREFIX/arm/bin</dt>	<dd>Executables for ARM</dd>
  <dt>$PREFIX/arm/lib</dt>	<dd>Libraries for ARM (libnidas.so, libnidas_dynld.so)</dd>
  <dt>$PREFIX/arm/include</dt>	<dd>Headers</dd>

  <dt>$PREFIX/x86/bin</dt>	<dd>Executables for X86</dd>
  <dt>$PREFIX/x86/lib</dt>	<dd>Libraries for X86 (libnidas.so, libnidas_dynld.so)</dd>
  <dt>$PREFIX/x86/include</dt>	<dd>Headers</dd>
  </dl>

  \section install_x86 How to Build NIDAS for Intel X86
  - cd src
  - scons PREFIX=/install_path
  - scons x86_install

  \section install_arm How to Build NIDAS for ARM
  - sh/bash:  PATH=/opt/arcom/bin:$PATH
  - csh/tcsh: set path = (/opt/arcom/bin $path)

  - cd src
  - scons PREFIX=/install_path
  - scons arm_install

  \namespace nidas
    \brief Root namespace for the NCAR In-Situ Data Acquisition Software.
 */

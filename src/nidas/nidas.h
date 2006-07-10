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

  NIDAS requires xercesc and xmlrpc++.  They should be installed in PREFIX/arm and PREFIX/x86, where PREFIX is chosen when running scons, below.

  When building for ARM, make sure arm-linux-g++, and the other arm tools are available and in your path. For example:
  - sh/bash:  PATH=/opt/arcom/bin:$PATH
  - csh/tcsh: set path = (/opt/arcom/bin $path)

  The RTLinux modules in nidas/rtlinux are also built when building for arm.  The the RTLinux ARM cross-development tools must be installed in /opt/rtldk-2.2.

  - cd src
  - scons PREFIX=/install_path
  - scons install

  /install_path is where you want to install NIDAS, for example: /opt/nidas (which is the default).
  Executables, libraries and header files will be installed as follows:

  <dl>
  <dt>/install_path/arm/bin</dt>	<dd>Executables for ARM</dd>
  <dt> /install_path/arm/lib</dt>	<dd>Libraries for ARM (libnidas.so, libnidas_dynld.so)</dd>
  <dt> /install_path/arm/include</dt>	<dd>Headers</dd>

  <dt> /install_path/x86/bin</dt>	<dd>Executables for X86</dd>
  <dt> /install_path/x86/lib</dt>	<dd>Libraries for X86 (libnidas.so, libnidas_dynld.so)</dd>
  <dt> /install_path/x86/include</dt>	<dd>Headers</dd>
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

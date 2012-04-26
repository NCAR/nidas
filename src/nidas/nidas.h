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

  \section install_both How to Build and Install NIDAS for ARM and Intel Systems

  Building NIDAS requires the the gcc and g++ compilers, the flex parser, and the scons build tool.

  NIDAS uses various other software, such as xerces-c, xmlrpc++, bluez for bluetooth, and the Linux kernel headers for building kernel modules

  On RedHat systems, yum install the <b>nidas-build</b> package from the EOL repository before building nidas. Installing that package will ensure that your development system has the required software to do a build.

  \section install_all How to Build and Install NIDAS for All Supported Hosts
  - cd src
  - scons install

  \section prefix Setting install destination with PREFIX
  The value of PREFIX defaults to /opt/nidas. It can be changed with a scons runstring variable:
  - cd src
  - scons PREFIX=/usr install

  \section install_x86 How to Build NIDAS for Build Host's Architecture
  - cd src
  - scons BUILDS=host
  - scons BUILDS=host install

  \section install_arm How to Build NIDAS for ARM

  On RedHat systems, yum install the <b>nidas-ael</b> package from the EOL repository. Installing that package will ensure that your development system has the required software to do a build for ARM systems.

  Then make sure arm-linux-g++, and the other arm tools are available and in your path. For example:

  - sh/bash:  PATH=/opt/arcom/bin:$PATH
  - csh/tcsh: set path = (/opt/arcom/bin $path)

  - cd src
  - scons BUILDS=arm
  - scons BUILDS=arm install

  - cd src
  - scons BUILDS=armbe
  - scons BUILDS=armbe install

  \section directory Directory of NIDAS Installation
  $PREFIX defaults to /opt/nidas. It can be changed with a scons runstring variable.
  <dl>
  <dt>$PREFIX/bin</dt>	<dd>Executables for the host system</dd>
  <dt>$PREFIX/lib</dt>	<dd>Libraries for the host (libnidas.so, libnidas_dynld.so)</dd>
  <dt>$PREFIX/modules</dt>	<dd>Kernel modules for the host</dd>
  <dt>$PREFIX/include</dt>	<dd>C/C++ header files</dd>
  <dt>$PREFIX/arm/bin</dt>	<dd>Executables for ARM</dd>
  <dt>$PREFIX/arm/lib</dt>	<dd>Libraries for ARM (libnidas.so, libnidas_dynld.so)</dd>
  <dt>$PREFIX/arm/modules/viper</dt>	<dd>Kernel modules for Eurotech Viper CPU</dd>
  <dt>$PREFIX/arm/modules/titan</dt>	<dd>Kernel modules for Eurotech Titan CPU</dd>
  <dt>$PREFIX/armbe/bin</dt>	<dd>Executables for big-endian ARM</dd>
  <dt>$PREFIX/armbe/lib</dt>	<dd>Libraries for big-endian ARM (libnidas.so, libnidas_dynld.so)</dd>
  <dt>$PREFIX/armbe/modules/vulcan</dt>	<dd>Kernel modules for Eurotech Vulcan CPU</dd>
  </dl>
  \namespace nidas
    \brief Root namespace for the NCAR In-Situ Data Acquisition Software.
 */

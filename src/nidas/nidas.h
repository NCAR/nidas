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

  NIDAS uses the xerces-c and xmlrpc++ software packages. Building NIDAS
  requires access to the xerces-c and xmlrpc++ header files and libraries.

  \section directory Directory of NIDAS Installation
  $PREFIX defaults to /opt/nidas. It can be changed with a scons runstring variable
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

  \section install_all How to Build and Install NIDAS for All Supported Hosts
  - cd src
  - scons install

  \section prefix Setting install destination with PREFIX
  - cd src
  - scons PREFIX=/usr install

  \section install_x86 How to Build NIDAS for Intel hosts
  - cd src
  - scons BUILDS=host
  - scons BUILDS=host install

  \section install_arm How to Build NIDAS for ARM
  When building for ARM, make sure arm-linux-g++, and the other arm tools are available and in your path. For example:

  - sh/bash:  PATH=/opt/arcom/bin:$PATH
  - csh/tcsh: set path = (/opt/arcom/bin $path)

  - cd src
  - scons BUILDS=arm
  - scons BUILDS=arm install

  - cd src
  - scons BUILDS=armbe
  - scons BUILDS=armbe install

  \namespace nidas
    \brief Root namespace for the NCAR In-Situ Data Acquisition Software.
 */

/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ********************************************************************

*/

/*! \namespace nidas::dynld
    \brief The dynamically loadable classes of nidas.
    The nidas::dynld namespace and any sub namespaces,
    like nidas::dynld::raf, are for dynamically loadable classes,
    and any support classes that are only referenced by classes in
    nidas::dynld. Objects in nidas::dynld are created by
    nidas::core::DOMObjectFactory, using nidas::core::DynamicLoader,
    typically from a  class="XXXXX" attribute in an XML file.
 */

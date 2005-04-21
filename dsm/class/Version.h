/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_VERSION_H
#define DSM_VERSION_H

namespace dsm {

/**
 * Class of static member functions providing version strings.
 */
class Version
{
public:
    static const char* getSoftwareVersion() { return "$LastChangedRevision$"; }
    static const char* getArchiveVersion() { return "0"; }
};

}

#endif

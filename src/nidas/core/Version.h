/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-04-21 16:12:37 -0600 (Thu, 21 Apr 2005) $

    $LastChangedRevision: 1789 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.atd.ucar.edu/svn/hiaper/ads3/dsm/class/Version.h $
 ********************************************************************

*/

#ifndef NIDAS_CORE_VERSION_H
#define NIDAS_CORE_VERSION_H

namespace nidas { namespace core {

/**
 * Class of static member functions providing version strings.
 */
class Version
{
public:
    static const char* getSoftwareVersion() { return version; }
    static const char* getArchiveVersion() { return "1"; }

    static const char* version;
};

}}	// namespace nidas namespace core

#endif

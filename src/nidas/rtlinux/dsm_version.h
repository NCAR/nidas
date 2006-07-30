/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-04-21 16:12:37 -0600 (Thu, 21 Apr 2005) $

    $LastChangedRevision: 1789 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.atd.ucar.edu/svn/hiaper/ads3/dsm/class/Version.h $

    The macro DSM_VERSION should be defined with a compiler option:
    	-DDSM_VERSION=XXXX.
 ********************************************************************

*/

#ifndef DSM_VERSION_H
#define DSM_VERSION_H

#define DSM_STRINGIFY(x) #x

#define DSM_VERSION_STRING DSM_STRINGIFY(DSM_VERSION)

#endif

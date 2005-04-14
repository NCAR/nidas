/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision: 1703 $

    $LastChangedBy: wasinger $

    $HeadURL: http://orion/svn/hiaper/ads3/disc/class/portable.h $
 ********************************************************************
*/

#ifndef PORTABLE_H
#define PORTABLE_H

#include <sys/types.h>
#include <netinet/in.h>

#if !defined(VXWORKS) && (defined(__LITTLE_ENDIAN) || defined(_LITTLE_ENDIAN) || defined(LITTLE_ENDIAN))

// TODO - determine if the Viper board is big or little endian...

float   ntohf(float);

#else

#define ntohf(x)        (x)

#endif

#define htonf        ntohf

#endif

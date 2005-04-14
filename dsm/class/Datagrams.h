/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_DATAGRAMS_H
#define DSM_DATAGRAMS_H

#define DSM_MULTICAST_PORT 50000
#define DSM_MULTICAST_ADDR "239.0.0.10"

namespace dsm {

typedef enum datagramTypes {
    XML_CONFIG,
    RAW_SAMPLE,
    SYNC_RECORD,
} datagramType_t;

}

#endif

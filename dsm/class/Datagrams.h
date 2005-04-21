/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_DATAGRAMS_H
#define DSM_DATAGRAMS_H

#define DSM_MULTICAST_PORT 50000
#define DSM_MULTICAST_STATUS_PORT 50001
#define DSM_MULTICAST_ADDR "239.0.0.10"

namespace dsm {

typedef enum datagramTypes {
    XML_CONFIG,
    RAW_SAMPLE,
    SYNC_RECORD,
} datagramType_t;

}

#endif

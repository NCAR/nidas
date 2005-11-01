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

#define DSM_MULTICAST_PORT        30000
#define DSM_MULTICAST_STATUS_PORT 30001
#define ADS_XMLRPC_PORT           30002 // DSMServerIntf.cc
#define DSM_XMLRPC_PORT           30003 // DSMEngineIntf.cc
#define DIR_XMLRPC_PORT           30004 // ADSDirectorIntf.cc
#define ADS_XMLRPC_STATUS_PORT    30005 // for status_listener.cc

#define DSM_MULTICAST_ADDR "239.0.0.10"

namespace dsm {

typedef enum datagramTypes {
    XML_CONFIG,
    RAW_SAMPLE,
    SYNC_RECORD,
} datagramType_t;

}

#endif

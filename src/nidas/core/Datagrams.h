/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_DATAGRAMS_H
#define NIDAS_CORE_DATAGRAMS_H

#include <nidas/core/SocketAddrs.h>

// TODO: phase these macros out, replaced by those in SocketAddrs.h
#define DSM_SVC_REQUEST_PORT	  30000
#define DSM_MULTICAST_STATUS_PORT 30001
#define RSERIAL_PORT    	  30002 // DSMServerIntf.cc
#define ADS_XMLRPC_PORT           30003 // DSMServerIntf.cc
#define DSM_XMLRPC_PORT           30004 // DSMEngineIntf.cc
#define DIR_XMLRPC_PORT           30005 // ADSDirectorIntf.cc
#define ADS_XMLRPC_STATUS_PORT    30006 // for status_listener.cc

#define DSM_MULTICAST_ADDR "239.0.0.10"

namespace nidas { namespace core {

typedef enum datagramTypes {
    XML_CONFIG,
    RAW_SAMPLE,
    VARIABLE_LIST_XML,
} datagramType_t;

}}	// namespace nidas namespace core

#endif

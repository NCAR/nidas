/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-01-30 09:29:44 -0700 (Fri, 30 Jan 2009) $

    $LastChangedRevision: 4477 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/Datagrams.h $
 ********************************************************************

*/

#ifndef NIDAS_CORE_SOCKETADDRS_H
#define NIDAS_CORE_SOCKETADDRS_H

#define NIDAS_SVC_REQUEST_PORT_UDP      30000   // dsm_server listens for service requests
#define NIDAS_RAW_DATA_PORT_TCP         30000   // TCP port for raw data connections

#define DSM_STATUS_PORT_UDP             30001   // dsm processes multicast status on this port
#define NIDAS_SYNCREC_DATA_TCP          30001   // port for sending syncrecord data
                                                // TODO: use unix socket for this

#define NIDAS_DATA_PORT_UDP             30002   // UDP port for processed data
#define NIDAS_RSERIAL_PORT_TCP          30002   // rserial connections

#define DSM_SERVER_XMLRPC_PORT_TCP      30003   // dsm_server xmlrpc listen port, DSMServerIntf.cc

#define DSM_XMLRPC_PORT_TCP             30004   // dsm process xmlrpc listen port, DSMEngineIntf.cc

#define NIDAS_XMLRPC_STATUS_PORT_TCP    30006   // status_listener provides merged status on this port

#define NIDAS_MULTICAST_ADDR "239.0.0.10"

#endif

// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#ifndef NIDAS_CORE_SOCKETADDRS_H
#define NIDAS_CORE_SOCKETADDRS_H

#define NIDAS_SVC_REQUEST_PORT_UDP      30000   // dsm_server listens for service requests
#define NIDAS_RAW_DATA_PORT_TCP         30000   // TCP port for raw data connections

#define NIDAS_STATUS_PORT_UDP           30001   // nidas processes multicast status on this port
#define NIDAS_SYNCREC_DATA_TCP          30001   // port for sending syncrecord data
                                                // TODO: use unix socket for this

#define NIDAS_DATA_PORT_UDP             30002   // UDP port for processed data
#define NIDAS_RSERIAL_PORT_TCP          30002   // rserial connections

#define DSM_SERVER_XMLRPC_PORT_TCP      30003   // dsm_server xmlrpc listen port, DSMServerIntf.cc

#define DSM_XMLRPC_PORT_TCP             30004   // dsm process xmlrpc listen port, DSMEngineIntf.cc

#define NIDAS_XMLRPC_STATUS_PORT_TCP    30006   // status_listener provides merged status on this port

#define NIDAS_VARIABLE_LIST_PORT_TCP    30007   // server port for providing list of variables

#define NIDAS_MULTICAST_ADDR "239.0.0.10"

#endif

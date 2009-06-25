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

namespace nidas { namespace core {

enum McSocketRequest {

    /**
     * Request for a TCP feed of the configuration in XML.
     */
    XML_CONFIG = 0,

    /**
     * Request for a TCP feed of raw samples.
     */
    RAW_SAMPLE,

    /**
     * Request for feed of processed samples on a UDP socket,
     * and a listing of variables on a TCP socket.
     */
    UDP_PROCESSED_SAMPLE_FEED,
};

}}	// namespace nidas namespace core

#endif

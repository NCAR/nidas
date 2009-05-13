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
     * Request for a XML listing of processed variables via TCP.
     */
    XML_VARIABLE_LIST,
};

}}	// namespace nidas namespace core

#endif

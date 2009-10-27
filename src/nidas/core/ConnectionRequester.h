/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef NIDAS_CORE_CONNECTIONREQUESTER_H
#define NIDAS_CORE_CONNECTIONREQUESTER_H

#include <nidas/core/DSMTime.h>
#include <nidas/util/IOException.h>
#include <nidas/util/Inet4PacketInfo.h>

namespace nidas { namespace core {

class SampleInput;
class SampleOutput;

/**
 * Interface for an object that requests connections SampleOutputs.
 */
class SampleConnectionRequester
{
public:
    virtual ~SampleConnectionRequester() {}

    /**
     * How SampleOutputs notify their SampleConnectionRequester
     * that they are connected.
     */
    virtual void connect(SampleOutput* output) throw() = 0;

    /**
     * How SampleOutputs notify their SampleConnectionRequester
     * that they wish to be closed, likely do to an IOException.
     */
    virtual void disconnect(SampleOutput* output) throw() = 0;

#ifdef NEEDED
    /**
     * sendHeader will be called when a client of SampleConnectRequester
     * wants a header written, for example at the beginning of a file.
     */
    virtual void sendHeader(dsm_time_t,SampleOutput* output)
    	throw(nidas::util::IOException);
#endif
};

}}	// namespace nidas namespace core

#endif

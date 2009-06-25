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

class IOChannel;
class SampleInput;
class SampleOutput;

/**
 * Interface for an object that requests connections to SampleInputs
 * or SampleOutputs.
 */
class SampleConnectionRequester
{
public:
    virtual ~SampleConnectionRequester() {}
    virtual void connect(SampleInput*) throw() = 0;
    virtual void connect(SampleOutput* origout, SampleOutput* newout) throw() = 0;
    virtual void disconnect(SampleInput*) throw() = 0;
    virtual void disconnect(SampleOutput*) throw() = 0;

    /**
     * sendHeader will be called when a client of SampleConnectRequester
     * wants a header written, for example at the beginning of a file.
     */
    virtual void sendHeader(dsm_time_t,SampleOutput* output)
    	throw(nidas::util::IOException);
};

}}	// namespace nidas namespace core

#endif

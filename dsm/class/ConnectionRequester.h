/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef DSM_CONNECTIONREQUESTER_H
#define DSM_CONNECTIONREQUESTER_H

#include <DSMTime.h>
#include <atdUtil/IOException.h>

namespace dsm {

class IOChannel;
class IOStream;
class SampleInput;
class SampleOutput;

/**
 * Interface for an object that requests connections to Inputs
 * or Outputs.
 */
class ConnectionRequester
{
public:
    virtual void connected(IOChannel*) throw() {}
    virtual void disconnected(IOChannel*) throw() {}
};

/**
 * Interface for an object that requests connections to SampleInputs
 * or SampleOutputs.
 */
class SampleConnectionRequester
{
public:
    virtual void connected(SampleInput*) throw() {}
    virtual void connected(SampleOutput*) throw() {}
    virtual void disconnected(SampleInput*) throw() {}
    virtual void disconnected(SampleOutput*) throw() {}

    /**
     * sendHeader will be called when a client of SampleConnectRequester
     * wants a header written, for example at the beginning of a file.
     */
    virtual void sendHeader(dsm_time_t,IOStream* iostream)
    	throw(atdUtil::IOException);
};

}

#endif

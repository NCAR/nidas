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
     * SampleConnectionRequester will be called back when a new file is created.
     */
    virtual void newFileCallback(dsm_time_t,IOStream* iostream)
    	throw(atdUtil::IOException) {}
};

}

#endif

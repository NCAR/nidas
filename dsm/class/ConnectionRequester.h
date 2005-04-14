/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_CONNECTIONREQUESTER_H
#define DSM_CONNECTIONREQUESTER_H

#include <DSMTime.h>

namespace dsm {

class IOChannel;
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
    virtual void newFileCallback(dsm_sys_time_t) throw() {}
};

}

#endif

/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_CONNECTIONREQUESTER_H
#define DSM_CONNECTIONREQUESTER_H

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
    virtual void connected(IOChannel*) {}
    virtual void disconnected(IOChannel*) {}
};

/**
 * Interface for an object that requests connections to SampleInputs
 * or SampleOutputs.
 */
class SampleConnectionRequester
{
public:
    virtual void connected(SampleInput*) {}
    virtual void connected(SampleOutput*) {}
    virtual void disconnected(SampleInput*) {}
    virtual void disconnected(SampleOutput*) {}
};

}

#endif

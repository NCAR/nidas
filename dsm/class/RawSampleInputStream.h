/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_RAWSAMPLEINPUTSTREAM_H
#define DSM_RAWSAMPLEINPUTSTREAM_H

#include <SampleInput.h>
#include <Datagrams.h>

namespace dsm {

class RawSampleInputStream: public SampleInputStream
{
public:
    RawSampleInputStream();
    ~RawSampleInputStream();

    SampleInput* clone() { return new RawSampleInputStream(*this); }

    int getPseudoPort() const { return RAW_SAMPLE; }

    bool isSingleton() const { return false; }


protected:
};

}

#endif

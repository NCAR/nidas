/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_RAWSAMPLEOUTPUTSTREAM_H
#define DSM_RAWSAMPLEOUTPUTSTREAM_H

#include <SampleOutput.h>
#include <Datagrams.h>

namespace dsm {

class RawSampleOutputStream: public SampleOutputStream
{
public:
    RawSampleOutputStream();
    ~RawSampleOutputStream();

    SampleOutput* clone() { return new RawSampleOutputStream(*this); }

    int getPseudoPort() const { return RAW_SAMPLE; }

    bool isSingleton() const { return false; }


protected:
};

}

#endif

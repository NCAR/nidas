/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

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

    SampleOutput* clone() const { return new RawSampleOutputStream(*this); }

    bool isRaw() const { return true; }

    int getPseudoPort() const { return RAW_SAMPLE; }

protected:
};

}

#endif

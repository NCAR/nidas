/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_RAWSAMPLEOUTPUT_H
#define DSM_RAWSAMPLEOUTPUT_H

#include <SampleOutputStream.h>

namespace dsm {

class RawSampleOutput: public SampleOutputStream
{
public:
    RawSampleOutput();
    ~RawSampleOutput();

    void connect() throw(atdUtil::IOException);

protected:
};

}

#endif

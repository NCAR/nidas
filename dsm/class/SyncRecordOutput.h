/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifndef DSM_SYNCRECORDOUTPUT_H
#define DSM_SYNCRECORDOUTPUT_H

#include <SampleOutputStream.h>

namespace dsm {

class SyncRecordOutput: public SampleOutputStream
{
public:
    SyncRecordOutput();
    ~SyncRecordOutput();

    void connect() throw(atdUtil::IOException);

    bool receive(const Sample *s)
	throw(SampleParseException, atdUtil::IOException);
protected:
};

}

#endif

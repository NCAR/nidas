
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

#include <SampleOutput.h>
#include <SampleSorter.h>
#include <SyncRecordGenerator.h>

namespace dsm {

class SyncRecordOutput: public SampleOutputStream
{
public:
    
    SyncRecordOutput();

    virtual ~SyncRecordOutput();

    void setDSMConfig(const DSMConfig* val);

    void init() throw(atdUtil::IOException);

    void flush() throw(atdUtil::IOException);

    void close() throw(atdUtil::IOException);

    bool receive(const Sample* samp)
        throw(SampleParseException, atdUtil::IOException);

    bool isSingleton() const { return true; }

protected:
    SampleSorter sorter;

    SyncRecordGenerator generator;

};

}

#endif

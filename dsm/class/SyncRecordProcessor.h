
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SYNCRECORDPROCESSOR_H
#define DSM_SYNCRECORDPROCESSOR_H

#include <SampleIOProcessor.h>
#include <SyncRecordGenerator.h>
#include <SampleSorter.h>

#include <vector>
#include <map>
#include <string>

namespace dsm {

class DSMSensor;

class SyncRecordProcessor: public SampleIOProcessor
{
public:
    
    SyncRecordProcessor();

    SyncRecordProcessor(const SyncRecordProcessor&);

    virtual ~SyncRecordProcessor();

    SampleIOProcessor* clone() const;

    // void setDSMConfig(const DSMConfig* val);

    // void setDSMService(const DSMService* val);

    virtual bool singleDSM() const { return false; }

    void connect(SampleInput* input) throw(atdUtil::IOException);
    
    void disconnect(SampleInput* input) throw(atdUtil::IOException);

    void connected(SampleOutput* output) throw();

    void disconnected(SampleOutput* output) throw();

    void newFileCallback(dsm_time_t thead,IOStream* iostream) throw(atdUtil::IOException);

protected:
    void init() throw();

    void scanSensors(const std::list<DSMSensor*>& sensors);

    void allocateRecord(dsm_time_t timetag);

protected:

    SampleInput* input;

    SyncRecordGenerator generator;

};

}

#endif

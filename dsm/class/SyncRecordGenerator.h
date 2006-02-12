
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SYNCRECORDGENERATOR_H
#define DSM_SYNCRECORDGENERATOR_H

#include <SampleIOProcessor.h>
#include <SyncRecordSource.h>
// #include <SampleSorter.h>

#include <vector>
#include <map>
#include <string>

namespace dsm {

class DSMSensor;

class SyncRecordGenerator: public SampleIOProcessor
{
public:
    
    /**
     * Constructor.
     */
    SyncRecordGenerator();

    SyncRecordGenerator(const SyncRecordGenerator&);

    virtual ~SyncRecordGenerator();

    SyncRecordGenerator* clone() const;

    virtual bool singleDSM() const { return false; }

    void connect(SampleInput* input) throw(atdUtil::IOException);
    
    void disconnect(SampleInput* input) throw(atdUtil::IOException);

    void connected(SampleOutput* output) throw();

    void disconnected(SampleOutput* output) throw();

    void sendHeader(dsm_time_t thead,IOStream* iostream) throw(atdUtil::IOException);

protected:
    void init() throw();

    void scanSensors(const std::list<DSMSensor*>& sensors);

    void allocateRecord(dsm_time_t timetag);

protected:

    SampleInput* input;

    SyncRecordSource syncRecSource;

};

}

#endif

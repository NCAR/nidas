
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
#include <SampleSorter.h>

#include <vector>
#include <map>
#include <string>

#define SYNC_RECORD_ID 3
#define SYNC_RECORD_HEADER_ID 2

namespace dsm {


class DSMSensor;

class SyncRecordGenerator: public SampleClient, public SampleSource
{
public:
    
    SyncRecordGenerator();

    virtual ~SyncRecordGenerator();

    void init(const std::list<const DSMConfig*>& dsms) throw();

    bool receive(const Sample*) throw();

    void sendHeader(dsm_time_t timetag) throw();

    void flush() throw();

protected:

    void scanSensors(const std::list<DSMSensor*>& sensors);

    void allocateRecord(dsm_time_t timetag);

protected:

    void sendHeader() throw();

    bool initialized;

    /**
     * A mapping between sample ids and group ids.
     * A variable "group" is a list of variables from similar
     * sensors that all have the same sampling rate.
     * Each group will have a unique id, a non-negative integer.
     */
    std::map<unsigned long, int> groupIds;

    /**
     * For each group id, how many variables in that group.
     */
    std::vector<int> numVarsInRateGroup;

    /**
     * All the variables in a SampleTag will belong to the same
     * group since they come from the same sensor and have the
     * same sampling rate. sampleOffsets is the index within the
     * group of the first variable in a sample.
     */
    std::map<unsigned long, int> sampleOffsets;

    /**
     * For each group, number of milliseconds per sample,
     * 1000/rate, truncated to an integer.
     */
    std::vector<int> msecsPerSample;

    /**
     * For each group, its offset into the whole record.
     */
    std::vector<int> groupOffsets;

    std::vector<std::vector<std::string> > variableNames;
    int recSize;

    dsm_time_t syncTime;

    SampleT<float>* syncRecord;
    float* floatPtr;

    size_t unrecognizedSamples;

    std::ostringstream headerStream;

    volatile bool doHeader;
    volatile dsm_time_t headerTime;

    int badTimes;
};

}

#endif

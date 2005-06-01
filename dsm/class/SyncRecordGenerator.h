
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
#include <Variable.h>

#include <vector>
#include <list>
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
     * A variable group is a list of variables with equal sampling rates,
     * from similar sensors, for example all 50Hz variables sampled by
     * an A2D, or all 20Hz serial variables.  Each group will have
     * a unique group id, a non-negative integer.
     * varsOfRate contains lists of variables in each group, indexed
     * by group id.
     */
    std::vector<std::list<const Variable*> > varsOfRate;

    /**
     * A mapping between sample ids and group ids. When we
     * receive a sample, what group does it belong to.
     */
    std::map<dsm_sample_id_t, int> groupIds;

    /**
     * For each group, the sampling rate, rounded up to an integer.
     */
    std::vector<int> samplesPerSec;

    /**
     * For each group, number of milliseconds per sample,
     * 1000/rate, truncated to an integer.
     */
    std::vector<int> msecsPerSample;

    /**
     * Number of floats in each group.
     */
    std::vector<size_t> groupLengths;

    /**
     * For each group, its offset into the whole record.
     */
    std::vector<size_t> groupOffsets;

    /**
     * Offsets into the sync record of each variable in a sample,
     * indexed by sampleId.
     */
    std::map<dsm_sample_id_t,int*> varOffsets;

    /**
     * Lengths of each variable in a sample,
     * indexed by sampleId.
     */
    std::map<dsm_sample_id_t,size_t*> varLengths;

    /**
     * Number of variables in each sample.
     */
    std::map<dsm_sample_id_t,size_t> numVars;

    /**
     * List of all variables in the sync record.
     */
    std::list<const Variable*> variables;

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


/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_SYNCRECORDGENERATOR_H
#define DSM_SYNCRECORDGENERATOR_H

#include <SampleClient.h>
#include <SampleSource.h>
#include <Aircraft.h>

#include <vector>
#include <map>
#include <string>

namespace dsm {

class SyncRecordGenerator: public SampleClient, public SampleSource
{
public:
    
    SyncRecordGenerator();

    virtual ~SyncRecordGenerator();

    void setAircraft(const Aircraft* val);

    bool receive(const Sample* samp)
        throw(SampleParseException, atdUtil::IOException);

protected:
    void scanSensors(const std::list<DSMSensor*>& sensors);

    void allocateRecord(int ndays,dsm_sample_time_t timetag);

protected:

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

    dsm_sample_time_t syncTime;
    int ndays;

    SampleT<float>* syncRecord;
    float* floatPtr;

    const float floatNAN;

};

}

#endif

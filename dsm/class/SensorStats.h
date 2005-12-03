/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-11-19 21:47:19 -0700 (Sat, 19 Nov 2005) $

    $LastChangedRevision: 3129 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:8080/svn/nids/trunk/dsm/class/DSMSensor.h $
 ********************************************************************

*/
#ifndef SENSORSTATS_H
#define SENSORSTATS_H

namespace dsm {

/**
 * Interface for an object which maintains sampling and error
 * statistics for a sensor.
 */
class SensorStatsInterface {
public:

    virtual void initStatistics() = 0;

    virtual void calcStatistics(unsigned long periodUsec) = 0;

    virtual size_t getMaxSampleLength() const = 0;

    virtual size_t getMinSampleLength() const = 0;

    virtual size_t getBadTimeTagCount() const = 0;

    virtual float getObservedSamplingRate() const = 0;

    virtual float getObservedDataRate() const = 0;

};

/**
 * Utility class which implements SensorStatsInterface.
 */
class SensorStatsImpl {

public:

    void initStatistics();

    /**
     * Update the sensor sampling statistics: samples/sec,
     * bytes/sec, min/max sample size, that can be accessed via
     * getObservedSamplingRate(), getObservedDataRate() etc.
     * Should be called every periodUsec by a user of this sensor.
     * @param periodUsec Statistics period.
     */
    virtual void calcStatistics(unsigned long periodUsec);

    size_t getMaxSampleLength() const
    	{ return maxSampleLength[currStatsIndex]; }

    size_t getMinSampleLength() const
    	{ return minSampleLength[currStatsIndex]; }

    size_t getBadTimeTagCount() const
    {
	return questionableTimeTags;
    }

    float getObservedSamplingRate() const;

    float getObservedDataRate() const;

    inline void incrementNSamples() { nsamples++; }

    inline void incrementNbytes(size_t val) { nbytes += val; }

    inline void updateSampleLengthMinMax(size_t val)
    {
        minSampleLength[currIndex] =
		std::min(minSampleLength[currIndex],val);
        maxSampleLength[currIndex] =
		std::max(maxSampleLength[currIndex],val);
    }


    inline void incrementBadTimeTags() { }
protected:

    time_t initialTimeSecs;
    size_t minSampleLength[2];
    size_t maxSampleLength[2];
    int currStatsIndex;
    int reportStatsIndex;
    size_t nsamples;
    size_t nbytes;
    size_t questionableTimeTags;

    /**
    * Observed number of samples per second.
    */
    float sampleRateObs;

    float dataRateObs;

};


}

#endif

SensorStatsImpl::SensorStatsImpl()
{
    initStatistics();
}

void SensorStatsImpl::initStatistics()
{
    currStatsIndex = reportStatsIndex = 0;

    sampleRateObs = 0.0;
    maxSampleLength[0] = maxSampleLength[1] = 0;
    minSampleLength[0] = minSampleLength[1] = MAX_ULONG;
    nsamples = 0;
    nbytes = 0;
    initialTimeSecs = time(0);
}

void SensorStatsImpl::calcStatistics(unsigned long periodUsec)
{
    reportStatsIndex = currStatsIndex;
    currStatsIndex = (currStatsIndex + 1) % 2;
    maxSampleLength[currStatsIndex] = 0;

    sampleRateObs = ((float)nsamples / periodUsec) * USECS_PER_SEC;

    dataRateObs = ((float)nbytes / periodUsec) * USECS_PER_SEC;

    nsamples = 0;
    nbytes = 0;
}

float DSMSensor::getObservedSamplingRate() const {
  
    if (reportStatsIndex == currStatsIndex)
	return (float)nsamples/
	    std::max(1,(time(0) - initialTimeSecs));
    else return sampleRateObs;
}

float DSMSensor::getObservedDataRate() const {
    if (reportStatsIndex == currStatsIndex)
	return (float)nbytes /
	    std::max(1,(time(0) - initialTimeSecs));
    else return dataRateObs;
}

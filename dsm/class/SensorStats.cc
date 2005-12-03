
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-12-02 12:21:36 -0700 (Fri, 02 Dec 2005) $

    $LastChangedRevision: 3168 $

    $LastChangedBy: cjw $

    $HeadURL: http://localhost:8080/svn/nids/trunk/dsm/class/DSMSensor.cc $
 ********************************************************************

*/

#include <SensorStats.h>

using namespace std;
using namespace dsm;

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

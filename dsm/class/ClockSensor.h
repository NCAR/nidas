
/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-11-22 14:41:27 -0700 (Mon, 22 Nov 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/DSMSerialSensor.h $

 ******************************************************************
*/
#ifndef CLOCKSENSOR_H
#define CLOCKSENSOR_H

#include <Sample.h>
#include <DSMTime.h>

namespace dsm {

/**
 * Interface that a clock sensor should implement. The processClockSample
 * method is called in real-time from the DSMSensor::readSamples method,
 * (not during  post processing), when a raw clock sample is read from
 * the sensor device.
 */
class ClockSensor {
public:
    /**
     * Do any required processing of a raw clock sample to
     * provide a SampleT<dsm_sys_time_t>* containing the
     * best available estimate of the time the sample
     * was taken. A ClockSensor could look at any status
     * field in the input sample, and compare the time
     * against the current system clock.
     */
    virtual SampleT<dsm_sys_time_t>* processClockSample(const Sample* samp)
    	throw(SampleParseException,atdUtil::IOException) = 0;
};

}

#endif


/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_SAMPLESTREAMDATER_H
#define DSM_SAMPLESTREAMDATER_H

#include <Sample.h>
#include <DSMTime.h>

namespace dsm {

class SampleStreamDater {
public:
    SampleStreamDater(int maxClockDiff = 180000):
    	maxClockDiffMsec(maxClockDiff),clockTime(0) {}

    const Sample* operator()(const Sample*);
    Sample* operator()(Sample*);

    dsm_sys_time_t getTime() const { return sampleTime; }

    dsm_sys_time_t getClockTime() const { return clockTime; }

    typedef enum { NO_CLOCK, OUT_OF_SPEC, OK } status_t;

    status_t getStatus() const { return status; }
private:

    status_t computeTime(const Sample*);

    int maxClockDiffMsec;

    dsm_sys_time_t t0day;

    dsm_sys_time_t sampleTime;

    dsm_sys_time_t clockTime;

    status_t status;

};

}

#endif

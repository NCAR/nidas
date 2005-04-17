
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_SAMPLEDATER_H
#define DSM_SAMPLEDATER_H

#include <Sample.h>
#include <DSMTime.h>

namespace dsm {

class SampleDater {
public:
    SampleDater(int maxClockDiff = 180000):
    	maxClockDiffMsec(maxClockDiff),clockTime(0) {}

    void setTime(dsm_time_t);

    dsm_time_t getTime() const { return clockTime; }

    typedef enum { NO_CLOCK, OUT_OF_SPEC, OK } status_t;

    status_t setSampleTime(Sample*) const;

private:

    int maxClockDiffMsec;

    dsm_time_t t0day;

    dsm_time_t clockTime;

};

}

#endif


/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_SAMPLEDATER_H
#define NIDAS_CORE_SAMPLEDATER_H

#include <nidas/core/Sample.h>
#include <nidas/core/DSMTime.h>

namespace nidas { namespace core {

/**
 * SampleClock adds date information to sample time tags.
 * Sample timetags from the various NIDAS driver modules
 * are only timetagged with the time since 00:00 GMT of 
 * the current day.  SampleClock adds the day offset to
 * the timetags, so that they are an absolute time.
 */
class SampleClock {
public:

    static SampleClock* getInstance() { return _instance; }

    /**
     * Constructor.
     */
    SampleClock();

    /**
     * @param val: A check for reasonable sample times (seconds).
     *        If the sample time differs from the clock
     * 		source time (as set by setTime) by more than
     * 		maxClockDiff, then the state is set to OUT_OF_SPEC.
     */
    void setMaxClockDiff(int val)
    {
    	_maxClockDiffSec = val;
    }

    /**
     * Update the SampleClock from the UNIX OS clock.
     */
    void setTime();

    /**
     * Enumeration of the result of addSampleDate().
     */
    typedef enum { NO_CLOCK, OUT_OF_SPEC, OK } status_t;

    /**
     * Add date information to a sample time tag.
     * @param samp A Sample, whose timetag value is
     *		a relative time in microseconds since 00:00 GMT.
     * @return Enumeration of time tag status:
     *		NO_CLOCK: we don't have absolute time information yet,
     *			sample time tag is not valid,
     *		OUT_OF_SPEC: sample time differs from absolute time,
     *			by more than maxClockDiff, sample time tag
     *			is not valid,
     *		OK: good sample time.
     */
    status_t addSampleDate(Sample* samp);

private:

    static SampleClock* _instance;

    int _maxClockDiffSec;

    dsm_time_t _t0day;

    dsm_time_t _clockTime;

};

}}	// namespace nidas namespace core

#endif

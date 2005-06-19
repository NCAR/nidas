
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SAMPLEDATER_H
#define DSM_SAMPLEDATER_H

#include <Sample.h>
#include <DSMTime.h>

namespace dsm {

/**
 * SampleDater adds date information to sample time tags.
 * Periodically a clock source must call setTime() to set
 * the current dsm_time_t (the number of microseconds
 * since Jan 1, 1970 00:00 GMT).
 * Samples which come from the UNIX driver level have
 * are only timetagged with the time since 00:00 GMT
 * of the current day.
 * SampleDater adds the day offset to the timetags, so
 * that they are an absolute time.
 */
class SampleDater {
public:

    /**
     * Constructor.
     * @param maxClockDiff A check for reasonable sample times.
     *        If the sample time differs from the clock
     * 		source time (as set by setTime) by more than
     * 		maxClockDiff, then the state is set to OUT_OF_SPEC.
     */
    SampleDater(int maxClockDiff = 180):
    	maxClockDiffUsec(maxClockDiff * MSECS_PER_SEC),t0day(0),clockTime(0) {}

    /**
     * Set the absolute time, microseconds since Jan 1, 1970 00:00 GMT.
     */
    void setTime(dsm_time_t);

    /**
     * Get the absolute time, microseconds since Jan 1, 1970 00:00 GMT.
     */
    dsm_time_t getTime() const { return clockTime; }

    /**
     * Enumeration of the result of setSampleTime().
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
    status_t setSampleTime(Sample* samp) const;

private:

    int maxClockDiffUsec;

    dsm_time_t t0day;

    dsm_time_t clockTime;

};

}

#endif


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
 * Periodically a clock source must call setTime() to set
 * the current dsm_time_t (the number of microseconds
 * since Jan 1, 1970 00:00 GMT).
 * Samples which come from the UNIX driver level
 * are only timetagged with the time since 00:00 GMT
 * of the current day.
 * SampleClock adds the day offset to the timetags, so
 * that they are an absolute time.
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
    	maxClockDiffSec = val;
    }

    /**
     * Set the absolute time, microseconds since Jan 1, 1970 00:00 GMT.
     * This is used if a timing card (e.g. IRIG) on the
     * system has a better clock than the OS clock. This method
     * updates the offset of the OS clock from the external clock,
     * which is used when returning the value of getDataSystemTime().
     */
    void setTime(dsm_time_t);

    /**
     * Update the SampleClock from the UNIX OS clock.
     */
    void setTime();

    /**
     * Get a time in microseconds since Jan 1,
     * 1970 00:00 GMT, as estimated by
     * nidas::core::getSystemTime() + offset.
     * offset is computed every time setTime() is called.
     * This just returns the last value passed
     * to setTime(), so it is not continuously updated
     * clock.
     */
    dsm_time_t getTime() const;

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

    /**
     * Get the current data system time.  If an external
     * clock is being used to update the SampleClock, this time
     * is the value of the UNIX OS clock, adjusted by the
     * last computed offset to the external clock. If an
     * external clock is not being used, this value is
     * simply the UNIX sytem time.
     */

    dsm_time_t getDataSystemTime() const; 

private:

    static SampleClock* _instance;

    int maxClockDiffSec;

    dsm_time_t t0day;

    dsm_time_t clockTime;

    mutable nidas::util::Mutex sysTimeMutex;

    long long sysTimeAhead;

    /**
     * Issue a warning log message if the external clock differs from the
     * system time by this number of microseconds.
     */
    const int TIME_DIFF_WARN_THRESHOLD;

    size_t timeWarnCount;

    /**
     * Is SampleClock set from samples of an external clock, e.g. IRIG?
     * If false, then SampleClock is from UNIX system clock.
     */
    bool externalClock;

};

}}	// namespace nidas namespace core

#endif

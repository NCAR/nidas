
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/SampleClock.h>
#include <nidas/core/DSMTime.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleClock* SampleClock::_instance = new SampleClock();

SampleClock::SampleClock():
    maxClockDiffSec(180),
    t0day(0),clockTime(0),sysTimeAhead(0),
    TIME_DIFF_WARN_THRESHOLD(USECS_PER_SEC),
    timeWarnCount(0),externalClock(false)
{
    clockTime = nidas::core::getSystemTime();
    t0day = timeFloor(clockTime,USECS_PER_DAY);
}


void SampleClock::setTime(dsm_time_t val)
{
    clockTime = val;
    t0day = timeFloor(clockTime,USECS_PER_DAY);

    dsm_time_t tnow = nidas::core::getSystemTime();

    sysTimeMutex.lock();
    sysTimeAhead = tnow - val;
    sysTimeMutex.unlock();
    if (::llabs(sysTimeAhead) > TIME_DIFF_WARN_THRESHOLD) {
	if (!(timeWarnCount++ % 100))
	    n_u::Logger::getInstance()->log(LOG_WARNING,
	    	"sysTimeAhead=%d usec, warn_count=%d (expected situation if no IRIG feed)",sysTimeAhead,timeWarnCount);
    }
    externalClock = true;
}

void SampleClock::setTime()
{
    clockTime = nidas::core::getSystemTime();
    t0day = timeFloor(clockTime,USECS_PER_DAY);
    sysTimeAhead = 0;
    externalClock = false;
}

dsm_time_t SampleClock::getTime() const 
{
    n_u::Synchronized autolock(sysTimeMutex);
    return nidas::core::getSystemTime() - sysTimeAhead;
}

SampleClock::status_t SampleClock::addSampleDate(Sample* samp)
{
    // assert(samp->getTimeTag() < USECS_PER_DAY);

    // This method is used to add an absolute time tag
    // to samples which have just been read from the I/O device.
    // Low level I/O device driver code just assigns sample timetags
    // which are the elapsed time since midnight UTC.
    // Add the time at beginning of day to the sample time tag
    // to get an absolute time tag.
    dsm_time_t sampleTime = t0day + samp->getTimeTag();

    // Due to midnight roll-over (from 86400,000,000 usecs to 0)
    // it may be off by a day.  Compare the sample against
    // the clockTime to correct for the rollover. 
    // Note that the microsecond difference exceeds the range
    // of an int, so convert to seconds first.
    int tdiff = (sampleTime - clockTime) / USECS_PER_SEC;

    if (!externalClock && abs(tdiff) > maxClockDiffSec) {
        setTime();
        tdiff = (sampleTime - clockTime) / USECS_PER_SEC;
    }

    if (abs(tdiff) > maxClockDiffSec) {
	/* midnight rollover */
	if (abs(tdiff + SECS_PER_DAY) < maxClockDiffSec) {
#ifdef DEBUG
            n_u::UTime tn(nidas::core::getSystemTime());
            n_u::UTime tt(sampleTime);
            n_u::UTime ct(clockTime);
            n_u::UTime t0(t0day);
            n_u::Logger::getInstance()->log(LOG_DEBUG,
                "SampleClock::addSampleDate, rollover tn=%s, st=%s, clockTime=%s, t0=%s, tt=%lld, id=%d,%d, diff=%d\n",
                tn.format(true,"%c").c_str(),
                tt.format(true,"%c").c_str(),
                ct.format(true,"%c").c_str(),
                t0.format(true,"%c").c_str(),
                samp->getTimeTag(),
                GET_DSM_ID(samp->getId()),GET_SHORT_ID(samp->getId()),
                tdiff);
#endif
	    sampleTime += USECS_PER_DAY;
	}
	else if (abs(tdiff - SECS_PER_DAY) < maxClockDiffSec) {
#ifdef DEBUG
            n_u::UTime tn(nidas::core::getSystemTime());
            n_u::UTime tt(sampleTime);
            n_u::UTime ct(clockTime);
            n_u::UTime t0(t0day);
            n_u::Logger::getInstance()->log(LOG_DEBUG,
                "SampleClock::addSampleDate, rollback tn=%s, st=%s, clockTime=%s, t0=%s, tt=%lld, id=%d,%d, diff=%d\n",
                tn.format(true,"%c").c_str(),
                tt.format(true,"%c").c_str(),
                ct.format(true,"%c").c_str(),
                t0.format(true,"%c").c_str(),
                samp->getTimeTag(),
                GET_DSM_ID(samp->getId()),GET_SHORT_ID(samp->getId()),
                tdiff);
#endif
	    sampleTime -= USECS_PER_DAY;
	}
	else {
#ifdef DEBUG
            n_u::UTime tn(nidas::core::getSystemTime());
            n_u::UTime tt(sampleTime);
            n_u::UTime ct(clockTime);
            n_u::UTime t0(t0day);
            n_u::Logger::getInstance()->log(LOG_DEBUG,
                "SampleClock::addSampleDate, bad time tn=%s, st=%s, clockTime=%s, t0=%s, tt=%lld, id=%d,%d, diff=%d\n",
                tn.format(true,"%c").c_str(),
                tt.format(true,"%c").c_str(),
                ct.format(true,"%c").c_str(),
                t0.format(true,"%c").c_str(),
                samp->getTimeTag(),
                GET_DSM_ID(samp->getId()),GET_SHORT_ID(samp->getId()),
                tdiff);
#endif
	    return OUT_OF_SPEC;
	}
    }
    samp->setTimeTag(sampleTime);
    return OK;
}

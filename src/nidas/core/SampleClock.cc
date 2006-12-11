
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

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleClock* SampleClock::_instance = new SampleClock();

SampleClock::SampleClock():
    maxClockDiffUsec(180 * USECS_PER_SEC),
    t0day(0),clockTime(0),sysTimeAhead(0),
    TIME_DIFF_WARN_THRESHOLD(USECS_PER_SEC),
    timeWarnCount(0)
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
    if (::llabs(tnow - val) > LONG_MAX) {
        if (tnow > val) sysTimeAhead = LONG_MAX;
        else sysTimeAhead = -(LONG_MAX-1);
    }
    else sysTimeAhead = tnow - val;
    sysTimeMutex.unlock();
    if (abs(sysTimeAhead) > TIME_DIFF_WARN_THRESHOLD) {
	if (!(timeWarnCount++ % 100))
	    n_u::Logger::getInstance()->log(LOG_WARNING,
	    	"sysTimeAhead=%d usec, warn_count=%d (expected situation if no IRIG feed)",sysTimeAhead,timeWarnCount);
    }
}

dsm_time_t SampleClock::getTime() const 
{
    n_u::Synchronized autolock(sysTimeMutex);
    return nidas::core::getSystemTime() - sysTimeAhead;
}

SampleClock::status_t SampleClock::addSampleDate(Sample* samp) const
{
    assert(samp->getTimeTag() < USECS_PER_DAY);
    dsm_time_t sampleTime = t0day + samp->getTimeTag();

    int tdiff = sampleTime - clockTime;

    if (abs(tdiff) > maxClockDiffUsec) {
	/* midnight rollover */
	if (abs(tdiff + USECS_PER_DAY) < maxClockDiffUsec) {
#ifdef DEBUG
	    cerr << "midnight rollover, sampleTime=" << sampleTime <<
	    	" (" << samp->getTimeTag() << '+' << t0day <<
		"), clockTime=" << clockTime <<
		" samp-clock=" << sampleTime - clockTime << endl;
#endif
	    sampleTime += USECS_PER_DAY;
	}
	else if (abs(tdiff - USECS_PER_DAY) < maxClockDiffUsec) {
	    cerr << "backwards time near midnight, sampleTime=" << sampleTime <<
	    	" (" << samp->getTimeTag() << '+' << t0day <<
		"), clockTime=" << clockTime <<
		" samp-clock=" << sampleTime - clockTime << endl;
	    sampleTime -= USECS_PER_DAY;
	}
	else {
	    cerr << "bad time? sampleTime=" << sampleTime <<
	    	" (" << samp->getTimeTag() << '+' << t0day <<
		"), clockTime=" << clockTime <<
		" samp-clock=" << sampleTime - clockTime <<
		", dsm=" << samp->getDSMId() <<
		", sampid=" << samp->getShortId() << endl;
	    return OUT_OF_SPEC;
	}
    }
    samp->setTimeTag(sampleTime);
    return OK;
}

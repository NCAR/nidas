
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/SampleDater.h>
#include <nidas/core/DSMTime.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

void SampleDater::setTime(dsm_time_t clockT)
{
    clockTime = clockT;
    t0day = timeFloor(clockTime,USECS_PER_DAY);
    dsm_time_t tnow = getSystemTime();
    sysTimeMutex.lock();
    if (::llabs(tnow - clockT) > LONG_MAX) {
        if (tnow > clockT) sysTimeAhead = LONG_MAX;
        else sysTimeAhead = -(LONG_MAX-1);
    }
    else sysTimeAhead = tnow - clockT;
    sysTimeMutex.unlock();
    if (abs(sysTimeAhead) > TIME_DIFF_WARN_THRESHOLD) {
	if (!(timeWarnCount++ % 100))
	    n_u::Logger::getInstance()->log(LOG_WARNING,
	    	"sysTimeAhead=%d usec, warn_count=%d (expected situation if no IRIG feed)",sysTimeAhead,timeWarnCount);
    }
}

dsm_time_t SampleDater::getDataSystemTime() const 
{
    n_u::Synchronized autolock(sysTimeMutex);
    return nidas::core::getSystemTime() - sysTimeAhead;
}


SampleDater::status_t SampleDater::setSampleTime(Sample* samp) const
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
	    if (t0day == 0) return NO_CLOCK;
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

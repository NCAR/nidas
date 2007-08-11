
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
    maxClockDiffUsec(180 * USECS_PER_SEC),
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
    externalClock = true;

#define DEBUG_MIDNIGHT
#ifdef DEBUG_MIDNIGHT
    if ( tnow % USECS_PER_DAY > (USECS_PER_DAY - 360 * USECS_PER_SEC) ||
        tnow % USECS_PER_DAY < 360 * USECS_PER_SEC) {
            n_u::UTime tt(tnow);
            n_u::UTime ct(clockTime);
            n_u::UTime t0(t0day);
            n_u::Logger::getInstance()->log(LOG_INFO,
                "SampleClock::setTime, externalClock tnow=%s, clockTime=%s, t0day=%s\n",
                tt.format(true,"%c").c_str(),
                ct.format(true,"%c").c_str(),
                t0.format(true,"%c").c_str());
    }
#endif
}

void SampleClock::setTime()
{
    clockTime = nidas::core::getSystemTime();
    t0day = timeFloor(clockTime,USECS_PER_DAY);
    sysTimeAhead = 0;
#ifdef DEBUG_MIDNIGHT
    if ( clockTime % USECS_PER_DAY > (USECS_PER_DAY - 360 * USECS_PER_SEC) ||
        clockTime % USECS_PER_DAY < 360 * USECS_PER_SEC) {
            dsm_time_t tnow = nidas::core::getSystemTime();
            n_u::UTime tt(tnow);
            n_u::UTime ct(clockTime);
            n_u::UTime t0(t0day);
            n_u::Logger::getInstance()->log(LOG_INFO,
                "SampleClock::setTime, internalClock tnow=%s, clockTime=%s, t0day=%s\n",
                tt.format(true,"%c").c_str(),
                ct.format(true,"%c").c_str(),
                t0.format(true,"%c").c_str());
    }
#endif
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
    dsm_time_t sampleTime = t0day + samp->getTimeTag();

    long long tdiff = sampleTime - clockTime;

    if (!externalClock && abs(tdiff) > maxClockDiffUsec) {
        setTime();
        tdiff = sampleTime - clockTime;
    }

    if (abs(tdiff) > maxClockDiffUsec) {
	/* midnight rollover */
	if (abs(tdiff + USECS_PER_DAY) < maxClockDiffUsec) {
#ifdef DEBUG_MIDNIGHT
            n_u::UTime tn(nidas::core::getSystemTime());
            n_u::UTime tt(sampleTime);
            n_u::UTime ct(clockTime);
            n_u::UTime t0(t0day);
            n_u::Logger::getInstance()->log(LOG_INFO,
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
	else if (abs(tdiff - USECS_PER_DAY) < maxClockDiffUsec) {
#ifdef DEBUG_MIDNIGHT
            n_u::UTime tn(nidas::core::getSystemTime());
            n_u::UTime tt(sampleTime);
            n_u::UTime ct(clockTime);
            n_u::UTime t0(t0day);
            n_u::Logger::getInstance()->log(LOG_INFO,
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
#ifdef DEBUG_MIDNIGHT
            n_u::UTime tn(nidas::core::getSystemTime());
            n_u::UTime tt(sampleTime);
            n_u::UTime ct(clockTime);
            n_u::UTime t0(t0day);
            n_u::Logger::getInstance()->log(LOG_INFO,
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

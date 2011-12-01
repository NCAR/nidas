// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <cstdlib>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleClock* SampleClock::_instance = new SampleClock();

SampleClock::SampleClock():
    _maxClockDiffSec(180),
    _t0day(0),_clockTime(0)
{
    _clockTime = n_u::getSystemTime();
    _t0day = n_u::timeFloor(_clockTime,USECS_PER_DAY);
}

void SampleClock::setTime()
{
    _clockTime = n_u::getSystemTime();
    _t0day = n_u::timeFloor(_clockTime,USECS_PER_DAY);
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
    dsm_time_t sampleTime = _t0day + samp->getTimeTag();

    // Due to midnight roll-over (from 86400,000,000 usecs to 0)
    // it may be off by a day.  Compare the sample against
    // the _clockTime to correct for the rollover. 
    // Note that the microsecond difference exceeds the range
    // of an int, so convert to seconds first.
    int tdiff = (sampleTime - _clockTime) / USECS_PER_SEC;

    if (abs(tdiff) > _maxClockDiffSec) {
        setTime();
        tdiff = (sampleTime - _clockTime) / USECS_PER_SEC;
    }

    if (abs(tdiff) > _maxClockDiffSec) {
	/* midnight rollover */
	if (abs(tdiff + SECS_PER_DAY) < _maxClockDiffSec) {
#ifdef DEBUG
            n_u::UTime tn(n_u::getSystemTime());
            n_u::UTime tt(sampleTime);
            n_u::UTime ct(_clockTime);
            n_u::UTime t0(_t0day);
            n_u::Logger::getInstance()->log(LOG_DEBUG,
                "SampleClock::addSampleDate, rollover tn=%s, st=%s, _clockTime=%s, t0=%s, tt=%lld, id=%d,%d, diff=%d\n",
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
	else if (abs(tdiff - SECS_PER_DAY) < _maxClockDiffSec) {
#ifdef DEBUG
            n_u::UTime tn(n_u::getSystemTime());
            n_u::UTime tt(sampleTime);
            n_u::UTime ct(_clockTime);
            n_u::UTime t0(_t0day);
            n_u::Logger::getInstance()->log(LOG_DEBUG,
                "SampleClock::addSampleDate, rollback tn=%s, st=%s, _clockTime=%s, t0=%s, tt=%lld, id=%d,%d, diff=%d\n",
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
            n_u::UTime tn(n_u::getSystemTime());
            n_u::UTime tt(sampleTime);
            n_u::UTime ct(_clockTime);
            n_u::UTime t0(_t0day);
            n_u::Logger::getInstance()->log(LOG_DEBUG,
                "SampleClock::addSampleDate, bad time tn=%s, st=%s, _clockTime=%s, t0=%s, tt=%lld, id=%d,%d, diff=%d\n",
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

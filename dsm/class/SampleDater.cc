
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <SampleDater.h>
#include <DSMTime.h>

using namespace dsm;
using namespace std;

void SampleDater::setTime(dsm_time_t clockT)
{
    clockTime = clockT;
    t0day = timeFloor(clockTime,USECS_PER_DAY);
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


/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SampleStreamDater.h>
#include <DSMTime.h>

using namespace dsm;
using namespace std;

SampleStreamDater::status_t SampleStreamDater::computeTime(const Sample* samp)
{
    if (samp->getShortId() == CLOCK_SAMPLE_ID &&
	samp->getType() == LONG_LONG_ST &&
	samp->getDataLength() == 1) {

	clockTime = ((long long*)samp->getConstVoidDataPtr())[0];
	t0day = timeFloor(clockTime,MSECS_PER_DAY);
	// cerr << "t0day=" << t0day << endl;
    }

    if (clockTime == 0) return NO_CLOCK;

    sampleTime = t0day + samp->getTimeTag();

    if (abs(sampleTime - clockTime) > maxClockDiffMsec) {
	/* midnight rollover */
	if (abs(sampleTime + MSECS_PER_DAY - clockTime) < maxClockDiffMsec) {
	    cerr << "midnight rollover, tt=" << samp->getTimeTag() <<
		" sampleTime=" << sampleTime <<
		" t0day=" << t0day << endl;
	    t0day += MSECS_PER_DAY;
	    sampleTime += MSECS_PER_DAY;
	}
	else {
	    cerr << "bad time? sampleTime=" << sampleTime <<
	    	" (" << samp->getTimeTag() << '+' << t0day <<
		"), clockTime=" << clockTime <<
		" samp-clock=" << sampleTime - clockTime << endl;
	    return OUT_OF_SPEC;
	}
    }
    return OK;
}
const Sample* SampleStreamDater::operator()(const Sample* samp)
{
    status = computeTime(samp);
    return samp;
}

Sample* SampleStreamDater::operator()(Sample* samp)
{
    status = computeTime(samp);
    return samp;
}


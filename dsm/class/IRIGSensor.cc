/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-01-03 13:26:59 -0700 (Mon, 03 Jan 2005) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/DSMSerialSensor.cc $

 ******************************************************************
*/

#include <pc104sg.h>

#include <IRIGSensor.h>
#include <DSMTime.h>
#include <RTL_DevIoctlStore.h>

#include <atdUtil/Logger.h>

#include <iostream>
#include <sstream>
#include <math.h>
#include <unistd.h>	// for sleep()

using namespace std;
using namespace dsm;
using namespace xercesc;

CREATOR_ENTRY_POINT(IRIGSensor)

IRIGSensor::IRIGSensor(): GOOD_CLOCK_LIMIT(60000),questionableClock(0)

{
}

IRIGSensor::~IRIGSensor() {
}
void IRIGSensor::open(int flags) throw(atdUtil::IOException)
{
    // It's magic, we can do an ioctl before the device is open!
    ioctl(IRIG_OPEN,(const void*)0,0);

    RTL_DSMSensor::open(flags);
    checkClock();
}

void IRIGSensor::checkClock() throw(atdUtil::IOException)
{
    struct timeval tval;
    dsm_time_t unixTime,unixTimeLast;
    dsm_time_t irigTime,irigTimeLast;
    unsigned char status;

    ioctl(IRIG_GET_STATUS,&status,sizeof(status));
    cerr << "IRIG_GET_STATUS=" << hex << (unsigned int)status << dec << endl;

    ioctl(IRIG_GET_CLOCK,&tval,sizeof(tval));
    cerr << "IRIG_GET_CLOCK=" << tval.tv_sec << ' ' << tval.tv_usec << endl;
    irigTime = ((dsm_time_t)tval.tv_sec) + tval.tv_usec / 1000;

    gettimeofday(&tval,0);
    cerr << "UNIX     CLOCK=" << tval.tv_sec << ' ' << tval.tv_usec << endl;
    unixTime = ((dsm_time_t)tval.tv_sec) + tval.tv_usec / 1000;

    if (status & CLOCK_STATUS_NOCODE) {
	cerr << "No IRIG time code. Setting IRIG clock to unix clock" << endl;
	ioctl(IRIG_SET_CLOCK,&tval,sizeof(tval));
    }

    struct timespec nsleep;
    nsleep.tv_sec = 0;
    nsleep.tv_nsec = 200000000;
    int ntry = 0;
    const int NTRY = 50;
    for (ntry = 0; ntry < NTRY; ntry++) {
	::nanosleep(&nsleep,0);

	ioctl(IRIG_GET_STATUS,&status,sizeof(status));
	ioctl(IRIG_GET_CLOCK,&tval,sizeof(tval));
	cerr << "IRIG_GET_CLOCK=" << tval.tv_sec << ' ' <<
	    tval.tv_usec << ", status=" << hex << (int)status << dec << endl;
	irigTime = ((dsm_time_t)tval.tv_sec) + tval.tv_usec / 1000;

	gettimeofday(&tval,0);
	cerr << "UNIX     CLOCK=" << tval.tv_sec << ' ' <<
	    tval.tv_usec << endl;
	unixTime = ((dsm_time_t)tval.tv_sec) + tval.tv_usec / 1000;

	if (ntry > 0) {
	    double dtunix = unixTime - unixTimeLast;
	    double dtirig = irigTime- irigTimeLast;
	    cerr << "UNIX-IRIG=" << unixTime - irigTime <<
		", dtunix=" << dtunix << ", dtirig=" << dtirig <<
		", ratio=" << fabs(dtunix - dtirig) / dtunix << endl;
	    if (::llabs(unixTime - irigTime) < 10000 &&
		fabs(dtunix - dtirig) / dtunix < 1.e-2) break;
	}

	unixTimeLast = unixTime;
	irigTimeLast = irigTime;
    }
    if (ntry == NTRY)
	cerr << "IRIG clock not behaving, UNIX-IRIG=" <<
	    unixTime-irigTime << " msecs" << endl;
}

void IRIGSensor::close() throw(atdUtil::IOException)
{
    ioctl(IRIG_CLOSE,(const void*)0,0);
    RTL_DSMSensor::close();
}

SampleDater::status_t IRIGSensor::setSampleTime(SampleDater* dater,Sample* samp)
{
    const dsm_clock_data* dp = (dsm_clock_data*)samp->getConstVoidDataPtr();
    assert(((unsigned long)dp % 8) == 0);

    dsm_time_t clockt = (dsm_time_t)(dp->tval.tv_sec) * 1000 +
	dp->tval.tv_usec / 1000;
    unsigned int status = dp->status;

    if (!(status & CLOCK_STATUS_NOCODE)) dater->setTime(clockt);

    return DSMSensor::setSampleTime(dater,samp);
    
}

bool IRIGSensor::process(const Sample* samp,std::list<const Sample*>& result)
	throw()
{
    dsm_time_t syst = getCurrentTimeInMillis();

    const dsm_clock_data* dp = (dsm_clock_data*)samp->getConstVoidDataPtr();
    assert(((unsigned long)dp % 8) == 0);


    dsm_time_t sampt = (dsm_time_t)(dp->tval.tv_sec) * 1000 +
	dp->tval.tv_usec / 1000;
    unsigned int status = dp->status;

    SampleT<dsm_time_t>* clksamp = getSample<dsm_time_t>(1);
    clksamp->setTimeTag(samp->getTimeTag());
    clksamp->setId(CLOCK_SAMPLE_ID);
    clksamp->getDataPtr()[0] = sampt;

    if (::llabs(syst - sampt) > GOOD_CLOCK_LIMIT) {
	if (false && !(questionableClock++ % 100)) {
	    const char* msg;
	    if (sampt > syst) msg = "ahead of";
	    else msg = "behind";

	    atdUtil::Logger::getInstance()->log(LOG_WARNING,
	    "IRIG clock is %lld msecs %s unix clock, status=0x%x, llabs=%lld",
	    sampt - syst,msg,status,::llabs(syst-sampt));
	}
	// if (status & CLOCK_STATUS_NOCODE) clksamp->getDataPtr()[0] = syst;
    }
    result.push_back(clksamp);
    return true;
}

void IRIGSensor::fromDOMElement(const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{
    RTL_DSMSensor::fromDOMElement(node);

    // Set sample rate to 1.0 (fixed in driver module)
    list<SampleTag*>::const_iterator si;
    for (si = sampleTags.begin(); si != sampleTags.end(); ++si) {
	SampleTag* samp = *si;
	samp->setRate(1.0);
    }
}

DOMElement* IRIGSensor::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* IRIGSensor::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}


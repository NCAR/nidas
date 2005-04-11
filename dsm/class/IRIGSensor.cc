/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

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

    struct timeval tv;

#define DEBUG
#ifdef DEBUG
    unsigned char status;
    ioctl(IRIG_GET_STATUS,&status,sizeof(status));
    cerr << "IRIG_GET_STATUS=" << hex << (unsigned int)status << dec << endl;

    ioctl(IRIG_GET_CLOCK,&tv,sizeof(tv));
    cerr << "IRIG_GET_CLOCK=" << tv.tv_sec << ' ' << tv.tv_usec << endl;
#endif

    gettimeofday(&tv,0);
    ioctl(IRIG_SET_CLOCK,&tv,sizeof(tv));

#ifdef DEBUG
    cerr << "IRIG_SET_CLOCK=" << tv.tv_sec << ' ' << tv.tv_usec << endl;

    ioctl(IRIG_GET_CLOCK,&tv,sizeof(tv));
    cerr << "IRIG_GET_CLOCK=" << tv.tv_sec << ' ' << tv.tv_usec << endl;

    sleep(1);
    ioctl(IRIG_GET_CLOCK,&tv,sizeof(tv));
    cerr << "IRIG_GET_CLOCK=" << tv.tv_sec << ' ' << tv.tv_usec << endl;

    sleep(1);
    ioctl(IRIG_GET_CLOCK,&tv,sizeof(tv));
    cerr << "IRIG_GET_CLOCK=" << tv.tv_sec << ' ' << tv.tv_usec << endl;

    sleep(1);
    ioctl(IRIG_GET_CLOCK,&tv,sizeof(tv));
    cerr << "IRIG_GET_CLOCK=" << tv.tv_sec << ' ' << tv.tv_usec << endl;
#endif
}

void IRIGSensor::close() throw(atdUtil::IOException)
{
    ioctl(IRIG_CLOSE,(const void*)0,0);
    RTL_DSMSensor::close();
}

bool IRIGSensor::process(const Sample* samp,std::list<const Sample*>& result)
	throw()
{
    dsm_sys_time_t syst = getCurrentTimeInMillis();

    const dsm_clock_data* dp = (dsm_clock_data*)samp->getConstVoidDataPtr();
    assert(((unsigned long)dp % 8) == 0);


    dsm_sys_time_t sampt = (dsm_sys_time_t)(dp->tval.tv_sec) * 1000 +
	dp->tval.tv_usec / 1000;
    unsigned int status = dp->status;

    SampleT<dsm_sys_time_t>* clksamp = getSample<dsm_sys_time_t>(1);
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


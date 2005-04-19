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
#include <DSMEngine.h>

#include <atdUtil/Logger.h>

#include <iostream>
#include <sstream>
#include <math.h>
#include <unistd.h>	// for sleep()

using namespace std;
using namespace dsm;
using namespace xercesc;

CREATOR_ENTRY_POINT(IRIGSensor)

IRIGSensor::IRIGSensor()
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
    dsm_time_t unixTime,unixTimeLast=0;
    dsm_time_t irigTime,irigTimeLast=0;
    unsigned char status;

    ioctl(IRIG_GET_STATUS,&status,sizeof(status));
    cerr << "IRIG_GET_STATUS=0x" << hex << (unsigned int)status << dec << 
    	" (" << statusString(status) << ')' << endl;

    ioctl(IRIG_GET_CLOCK,&tval,sizeof(tval));
    cerr << "IRIG_GET_CLOCK=" << tval.tv_sec << ' ' << tval.tv_usec << endl;
    irigTime = ((dsm_time_t)tval.tv_sec) * 1000 + tval.tv_usec / 1000;

    gettimeofday(&tval,0);
    cerr << "UNIX     CLOCK=" << tval.tv_sec << ' ' << tval.tv_usec << endl;
    unixTime = ((dsm_time_t)tval.tv_sec) * 1000 + tval.tv_usec / 1000;

    if ((status & CLOCK_STATUS_NOCODE) || (status & CLOCK_STATUS_NOYEAR) ||
	(status & CLOCK_STATUS_NOMAJT)) {
	cerr << "Setting IRIG clock to unix clock" << endl;
	ioctl(IRIG_SET_CLOCK,&tval,sizeof(tval));
    }
    else if (::llabs(unixTime-irigTime) > 365*8640000LL) {
	cerr << "Setting year in IRIG clock" << endl;
	ioctl(IRIG_SET_CLOCK,&tval,sizeof(tval));
    }

    struct timespec nsleep;
    nsleep.tv_sec = 0;
    nsleep.tv_nsec = 100000000;
    int ntry = 0;
    const int NTRY = 50;
    for (ntry = 0; ntry < NTRY; ntry++) {

	::nanosleep(&nsleep,0);

	ioctl(IRIG_GET_STATUS,&status,sizeof(status));
	ioctl(IRIG_GET_CLOCK,&tval,sizeof(tval));
	cerr << "IRIG_GET_CLOCK=" << tval.tv_sec << ' ' <<
	    tval.tv_usec << ", status=0x" << hex << (int)status << dec << endl;
	irigTime = ((dsm_time_t)tval.tv_sec) * 1000 + tval.tv_usec / 1000;

	gettimeofday(&tval,0);
	cerr << "UNIX     CLOCK=" << tval.tv_sec << ' ' <<
	    tval.tv_usec << endl;
	unixTime = ((dsm_time_t)tval.tv_sec) * 1000 + tval.tv_usec / 1000;

	if (ntry > 0) {
	    double dtunix = unixTime - unixTimeLast;
	    double dtirig = irigTime- irigTimeLast;
	    cerr << "UNIX-IRIG=" << unixTime - irigTime <<
		", dtunix=" << dtunix << ", dtirig=" << dtirig <<
		", rate ratio diff=" << fabs(dtunix - dtirig) / dtunix << endl;
	    if (::llabs(unixTime - irigTime) < 10000 &&
		fabs(dtunix - dtirig) / dtunix < 1.e-2) break;
	}

	unixTimeLast = unixTime;
	irigTimeLast = irigTime;
    }
    if (ntry == NTRY)
	cerr << "IRIG clock not behaving, UNIX-IRIG=" <<
	    unixTime-irigTime << " msecs" << endl;

    cerr << "setting SampleDater clock to " << irigTime << endl;
    DSMEngine::getInstance()->getSampleDater()->setTime(irigTime);
}

void IRIGSensor::close() throw(atdUtil::IOException)
{
    ioctl(IRIG_CLOSE,(const void*)0,0);
    RTL_DSMSensor::close();
}

string IRIGSensor::statusString(unsigned char status) const
{
    static const struct IRIGStatusCode {
        unsigned char mask;
	const char* str[2];	// state when bit=0, state when bit=1
    } statusCode[] = {
	{0x10,{"YEAR","NOYEAR"}},
    	{0x08,{"MAJTM","NOMAJTM"}},
    	{0x04,{"PPS","NOPPS"}},
    	{0x02,{"CODE","NOCODE"}},
    	{0x01,{"SYNC","NOSYNC"}}
    };
    ostringstream ostr;
    for (unsigned int i = 0; i < sizeof(statusCode)/sizeof(struct IRIGStatusCode); i++) {
	if (i > 0) ostr << '|';
	ostr << statusCode[i].str[(status & statusCode[i].mask) != 0];
    }
    return ostr.str();
}

void IRIGSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);

    struct timeval tval;
    dsm_time_t unixTime;
    dsm_time_t irigTime;
    unsigned char status;

    try {
	ioctl(IRIG_GET_STATUS,&status,sizeof(status));

	ostr << "<td>status: " << statusString(status) <<
		" (0x" << hex << (int)status << dec << ')';
	ioctl(IRIG_GET_CLOCK,&tval,sizeof(tval));
	irigTime = ((dsm_time_t)tval.tv_sec) * 1000 + tval.tv_usec / 1000;

	gettimeofday(&tval,0);
	unixTime = ((dsm_time_t)tval.tv_sec) * 1000 + tval.tv_usec / 1000;
	ostr << ", IRIG-UNIX=" << irigTime - unixTime << " msecs</td>" << endl;
    }
    catch(const atdUtil::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
    }
}
SampleDater::status_t IRIGSensor::setSampleTime(SampleDater* dater,Sample* samp)
{
    const dsm_clock_data* dp = (dsm_clock_data*)samp->getConstVoidDataPtr();
    assert(((unsigned long)dp % 8) == 0);

    dsm_time_t clockt = (dsm_time_t)(dp->tval.tv_sec) * 1000 +
	dp->tval.tv_usec / 1000;
    // unsigned int status = dp->status;
    dater->setTime(clockt);
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
    clksamp->setId(sampleId);
    clksamp->getDataPtr()[0] = sampt;

    result.push_back(clksamp);
    return true;
}

void IRIGSensor::fromDOMElement(const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{
    RTL_DSMSensor::fromDOMElement(node);

    if (sampleTags.size() != 1) 
    	throw atdUtil::InvalidParameterException(getName(),"<sample>",
		"should only be one <sample> tag");

    SampleTag* samp = sampleTags.front();
    samp->setRate(1.0);
    sampleId = samp->getId();
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


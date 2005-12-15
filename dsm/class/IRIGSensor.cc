/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <pc104sg.h>

#include <IRIGSensor.h>
#include <DSMTime.h>
#include <RTL_DevIoctlStore.h>
#include <DSMEngine.h>

#include <atdUtil/Logger.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <math.h>
#include <unistd.h>	// for sleep()

using namespace std;
using namespace dsm;
using namespace xercesc;

CREATOR_FUNCTION(IRIGSensor)

IRIGSensor::IRIGSensor()
{
}

IRIGSensor::~IRIGSensor() {
}
void IRIGSensor::open(int flags) throw(atdUtil::IOException)
{
    // checkClock sends the Unix time down to the pc104sg card.
    // If the card status indicates the pc104sg has good time codes, then
    // only the year is updated, since time codes don't contain the year.
    // If the pc104sg doesn't have good time codes, then all the
    // time fields (of second resolution and greater) are updated
    // to unix time.
    // It takes a while (~1/2 sec) for the update to take effect
    // and in the meantime the time fields are wrong (off by many years).
    // checkClock waits a maximum of 5 seconds until the pc104sg
    // time fields agree with the Unix time. 
    checkClock();

    // A function in the pc104sg driver checks once per second
    // that the timetag counter matches the time fields, and
    // corrects the counter if needed.
    // So for up to a second after the time fields are good
    // the timetag counter may still be wrong. So we wait
    // some more for the timetag counter to be updated.
    sleep(2);

    // Request that fifo be opened at driver end.
    ioctl(IRIG_OPEN,(const void*)0,0);

    RTL_DSMSensor::open(flags);
}

/**
 * Get the current time from the IRIG card.
 * This is not meant to be used for frequent use.
 */
dsm_time_t IRIGSensor::getIRIGTime() throw(atdUtil::IOException)
{
    unsigned char status;
    ioctl(IRIG_GET_STATUS,&status,sizeof(status));

    struct timeval tval;
    ioctl(IRIG_GET_CLOCK,&tval,sizeof(tval));

#ifdef DEBUG
    cerr << "IRIG_GET_CLOCK=" << tval.tv_sec << ' ' <<
	tval.tv_usec << ", status=0x" << hex << (int)status << dec << endl;
#endif
    return ((dsm_time_t)tval.tv_sec) * USECS_PER_SEC + tval.tv_usec;
}

void IRIGSensor::setIRIGTime(dsm_time_t val) throw(atdUtil::IOException)
{
    struct timeval tval;
    tval.tv_sec = val / USECS_PER_SEC;
    tval.tv_usec = val % USECS_PER_SEC;
    ioctl(IRIG_SET_CLOCK,&tval,sizeof(tval));
}

void IRIGSensor::checkClock() throw(atdUtil::IOException)
{
    dsm_time_t unixTime,unixTimeLast=0;
    dsm_time_t irigTime,irigTimeLast=0;
    unsigned char status;

    ioctl(IRIG_GET_STATUS,&status,sizeof(status));

    atdUtil::Logger::getInstance()->log(LOG_DEBUG,
    	"IRIG_GET_STATUS=0x%x (%s)",(unsigned int)status,
    	statusString(status,false).c_str());

    // cerr << "IRIG_GET_STATUS=0x" << hex << (unsigned int)status << dec << 
    // 	" (" << statusString(status,false) << ')' << endl;

    irigTime = getIRIGTime();
    unixTime = getSystemTime();

    if ((status & CLOCK_STATUS_NOCODE) || (status & CLOCK_STATUS_NOYEAR) ||
	(status & CLOCK_STATUS_NOMAJT)) {
	atdUtil::Logger::getInstance()->log(LOG_INFO,
	    "NOCODE, NOYEAR or NOMAJT: Setting IRIG clock to unix clock");
	setIRIGTime(unixTime);
    }
    else if (::llabs(unixTime-irigTime) > 180LL*USECS_PER_DAY) {
	atdUtil::Logger::getInstance()->log(LOG_INFO,
	    "Setting year in IRIG clock");
	setIRIGTime(unixTime);
    }

    struct timespec nsleep;
    nsleep.tv_sec = 0;
    nsleep.tv_nsec = NSECS_PER_SEC / 10;		// 1/10th sec
    int ntry = 0;
    const int NTRY = 50;
    for (ntry = 0; ntry < NTRY; ntry++) {

	::nanosleep(&nsleep,0);

	ioctl(IRIG_GET_STATUS,&status,sizeof(status));

	irigTime = getIRIGTime();
	unixTime = getSystemTime();

	if (ntry > 0) {
	    double dtunix = unixTime - unixTimeLast;
	    double dtirig = irigTime- irigTimeLast;
	    atdUtil::Logger::getInstance()->log(LOG_INFO,
	    "UNIX-IRIG=%lld usec, dtunix=%f, dtirig=%f, rate ratio diff=%f",
		unixTime - irigTime,dtunix,dtirig,
		fabs(dtunix - dtirig) / dtunix);

	    // cerr << "UNIX-IRIG=" << unixTime - irigTime <<
	    // 	", dtunix=" << dtunix << ", dtirig=" << dtirig <<
	    // 	", rate ratio diff=" << fabs(dtunix - dtirig) / dtunix << endl;

	    if (::llabs(unixTime - irigTime) < 10 * USECS_PER_SEC &&
		fabs(dtunix - dtirig) / dtunix < 1.e-2) break;
	}

	unixTimeLast = unixTime;
	irigTimeLast = irigTime;
    }
    if (ntry == NTRY)
	atdUtil::Logger::getInstance()->log(LOG_WARNING,
	    "IRIG clock not behaving, UNIX-IRIG=%lld usec",
	    unixTime-irigTime);

    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"setting SampleDater clock to %lld",irigTime);
    DSMEngine::getInstance()->getSampleDater()->setTime(irigTime);
}

void IRIGSensor::close() throw(atdUtil::IOException)
{
    ioctl(IRIG_CLOSE,(const void*)0,0);
    RTL_DSMSensor::close();
}

/* static */
string IRIGSensor::statusString(unsigned char status,bool xml)
{
    static const struct IRIGStatusCode {
        unsigned char mask;
	const char* str[2];	// state when bit=0, state when bit=1
	const char* xml[2];	// state when bit=0, state when bit=1
    } statusCode[] = {
	{0x20,{"SYNC","NOSYNC"},
		{"SYNC","<font color=red><b>NOSYNC</b></font>"}},
	{0x10,{"YEAR","NOYEAR"},
		{"YEAR","<font color=red><b>NOYEAR</b></font>"}},
    	{0x08,{"MAJTM","NOMAJTM"},
		{"MAJTM","<font color=red><b>NOMAJTM</b></font>"}},
    	{0x04,{"PPS","NOPPS"},
		{"PPS","<font color=red><b>NOPPS</b></font>"}},
    	{0x02,{"CODE","NOCODE"},
		{"CODE","<font color=red><b>NOCODE</b></font>"}},
    	{0x01,{"SYNC","NOSYNC"},
		{"SYNC","<font color=red><b>NOSYNC</b></font>"}},
    };
    ostringstream ostr;
    for (unsigned int i = 0; i < sizeof(statusCode)/sizeof(struct IRIGStatusCode); i++) {
	if (i > 0) ostr << ',';
	if (xml) ostr << statusCode[i].xml[(status & statusCode[i].mask) != 0];
	else ostr << statusCode[i].str[(status & statusCode[i].mask) != 0];
    }
    return ostr.str();
}

void IRIGSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    dsm_time_t unixTime;
    dsm_time_t irigTime;
    unsigned char status;

    try {
	ioctl(IRIG_GET_STATUS,&status,sizeof(status));

	ostr << "<td align=left>" << statusString(status,true) <<
		" (status=0x" << hex << (int)status << dec << ')';
	irigTime = getIRIGTime();
	unixTime = getSystemTime();
	ostr << ", IRIG-UNIX=" << fixed << setprecision(3) <<
		(float)(irigTime - unixTime)/USECS_PER_SEC << " sec</td>" << endl;
    }
    catch(const atdUtil::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
	atdUtil::Logger::getInstance()->log(LOG_ERR,
            "%s: printStatus: %s",getName().c_str(),
            ioe.what());
    }
}

SampleDater::status_t IRIGSensor::setSampleTime(SampleDater* dater,Sample* samp)
{
    dsm_time_t clockt = getTime(samp);

    // since we're a clock sensor, we are responsible for setting
    // the absolute time in the SampleDater.
    dater->setTime(clockt);

    return DSMSensor::setSampleTime(dater,samp);
}

bool IRIGSensor::process(const Sample* samp,std::list<const Sample*>& result)
	throw()
{
    dsm_time_t sampt = getTime(samp);

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
    const vector<const Variable*>& vars = samp->getVariables();
    assert(vars.size() == 1);

    // Warning: cast to non-const kludge!
    // Somehow we need to provide non-const Variables to DSMSensor
    // so that DSMSensor can change them.
    const_cast<Variable*>(vars.front())->setType(Variable::CLOCK);
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


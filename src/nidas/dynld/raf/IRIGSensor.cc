/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/linux/irig/pc104sg.h>

#include <nidas/dynld/raf/IRIGSensor.h>
#include <nidas/core/DSMTime.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/RTL_IODevice.h>
#include <nidas/core/UnixIODevice.h>

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <unistd.h>	// for sleep()

using namespace std;
using namespace nidas::core;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,IRIGSensor)

IRIGSensor::IRIGSensor(): _nvars(0)
{
}

IRIGSensor::~IRIGSensor() {
}

IODevice* IRIGSensor::buildIODevice() throw(n_u::IOException)
{
    if (DSMEngine::getInstance()->isRTLinux())
        return new RTL_IODevice();
    else return new UnixIODevice();
}

SampleScanner* IRIGSensor::buildSampleScanner()
{
    setDriverTimeTagUsecs(USECS_PER_MSEC);
    return new DriverSampleScanner();
}

void IRIGSensor::open(int flags) throw(n_u::IOException,
    n_u::InvalidParameterException)
{

    DSMSensor::open(flags);

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
    if (DSMEngine::getInstance()->isRTLinux())
	ioctl(IRIG_OPEN,0,0);

}

/**
 * Get the current time from the IRIG card.
 * This is not meant to be used for frequent use.
 */
dsm_time_t IRIGSensor::getIRIGTime() throw(n_u::IOException)
{
    unsigned char status;
    ioctl(IRIG_GET_STATUS,&status,sizeof(status));

    struct timeval32 tval;
    ioctl(IRIG_GET_CLOCK,&tval,sizeof(tval));

#ifdef DEBUG
    cerr << "IRIG_GET_CLOCK=" << tval.tv_sec << ' ' <<
	tval.tv_usec << ", status=0x" << hex << (int)status << dec << endl;
#endif
    return ((dsm_time_t)tval.tv_sec) * USECS_PER_SEC + tval.tv_usec;
}

void IRIGSensor::setIRIGTime(dsm_time_t val) throw(n_u::IOException)
{
    struct timeval32 tval;
    tval.tv_sec = val / USECS_PER_SEC;
    tval.tv_usec = val % USECS_PER_SEC;
    ioctl(IRIG_SET_CLOCK,&tval,sizeof(tval));
}

void IRIGSensor::checkClock() throw(n_u::IOException)
{
    dsm_time_t unixTime,unixTimeLast=0;
    dsm_time_t irigTime,irigTimeLast=0;
    unsigned char status;

    ioctl(IRIG_GET_STATUS,&status,sizeof(status));

    n_u::Logger::getInstance()->log(LOG_DEBUG,
    	"IRIG_GET_STATUS=0x%x (%s)",(unsigned int)status,
    	statusString(status,false).c_str());

    // cerr << "IRIG_GET_STATUS=0x" << hex << (unsigned int)status << dec << 
    // 	" (" << statusString(status,false) << ')' << endl;

    irigTime = getIRIGTime();
    unixTime = getSystemTime();

    if ((status & CLOCK_STATUS_NOCODE) || (status & CLOCK_STATUS_NOYEAR) ||
	(status & CLOCK_STATUS_NOMAJT)) {
	n_u::Logger::getInstance()->log(LOG_INFO,
	    "NOCODE, NOYEAR or NOMAJT: Setting IRIG clock to unix clock");
	setIRIGTime(unixTime);
    }
    else if (::llabs(unixTime-irigTime) > 180LL*USECS_PER_DAY) {
	n_u::Logger::getInstance()->log(LOG_INFO,
	    "Setting year in IRIG clock");
	setIRIGTime(unixTime);
    }

    struct timespec nsleep;
    nsleep.tv_sec = 0;
    nsleep.tv_nsec = NSECS_PER_SEC / 10;		// 1/10th sec
    int ntry = 0;
    const int NTRY = 50;

    string timeFormat="%Y %b %d %H:%M:%S.%3f";

    for (ntry = 0; ntry < NTRY; ntry++) {

	::nanosleep(&nsleep,0);

	ioctl(IRIG_GET_STATUS,&status,sizeof(status));

	irigTime = getIRIGTime();
	unixTime = nidas::core::getSystemTime();

	if (ntry > 0) {
	    int dtunix = unixTime - unixTimeLast;
	    int dtirig = irigTime- irigTimeLast;
            n_u::UTime it(irigTime);
            n_u::UTime ut(unixTime);
	    n_u::Logger::getInstance()->log(LOG_INFO,
                "UNIX: %s, dt=%7d usec",
                ut.format(true,timeFormat).c_str(),dtunix);
            n_u::Logger::getInstance()->log(LOG_INFO,
                "IRIG: %s, dt=%7d usec, unix-irig=%10lld usec, rate ratio diff=%f",
                it.format(true,timeFormat).c_str(),dtirig,
                unixTime - irigTime,fabs((float)(dtunix - dtirig)) / dtunix);

	    // cerr << "UNIX-IRIG=" << unixTime - irigTime <<
	    // 	", dtunix=" << dtunix << ", dtirig=" << dtirig <<
	    // 	", rate ratio diff=" << fabs(dtunix - dtirig) / dtunix << endl;

	    if (::llabs(unixTime - irigTime) < 10 * USECS_PER_SEC &&
		fabs((float)(dtunix - dtirig)) / dtunix < 1.e-2) break;
	}

	unixTimeLast = unixTime;
	irigTimeLast = irigTime;
    }
    if (ntry == NTRY)
	n_u::Logger::getInstance()->log(LOG_WARNING,
	    "IRIG clock not behaving, UNIX-IRIG=%lld usec",
	    unixTime-irigTime);

    n_u::UTime it(irigTime);
    n_u::UTime ut(unixTime);
    n_u::Logger::getInstance()->log(LOG_INFO,
            "UNIX: %s",ut.format(true,timeFormat).c_str());
    n_u::Logger::getInstance()->log(LOG_INFO,
            "IRIG: %s",it.format(true,timeFormat).c_str());
    n_u::Logger::getInstance()->log(LOG_INFO,
	"setting SampleClock to IRIG time");
    DSMEngine::getInstance()->getSampleClock()->setTime(irigTime);
}

void IRIGSensor::close() throw(n_u::IOException)
{
    if (DSMEngine::getInstance()->isRTLinux())
	ioctl(IRIG_CLOSE,0,0);
    DSMSensor::close();
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
    if (getReadFd() < 0) {
	ostr << "<td align=left><font color=red><b>not active</b></font></td>" << endl;
	return;
    }
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
    catch(const n_u::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
	n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: printStatus: %s",getName().c_str(),
            ioe.what());
    }
}

/*
 * Override nextSample in order to set the clock.
 */
Sample* IRIGSensor::nextSample()
{
    Sample* samp = DSMSensor::nextSample();
    // since we're a clock sensor, we are responsible for setting
    // the absolute time in the SampleClock.
    if (samp) {
        dsm_time_t clockt = getIRIGTime(samp);

// #define DEBUG_MIDNIGHT
#ifdef DEBUG_MIDNIGHT
        if ( clockt % USECS_PER_DAY > (USECS_PER_DAY - 360 * USECS_PER_SEC) ||
                clockt % USECS_PER_DAY < 360 * USECS_PER_SEC) {
            n_u::UTime tt(samp->getTimeTag());
            n_u::UTime ct(clockt);
            n_u::Logger::getInstance()->log(LOG_INFO,
                "IRIGSensor::nextSample tt= %s, clockt=%s\n",
                tt.format(true,"%c").c_str(),ct.format(true,"%c").c_str());
        }
#endif
        SampleClock::getInstance()->setTime(clockt);
    }
    return samp;
}

dsm_time_t IRIGSensor::getIRIGTime(const Sample* samp) const {
    const dsm_clock_data* dp = (dsm_clock_data*)samp->getConstVoidDataPtr();
    return (dsm_time_t)__le32_to_cpu(dp->tval.tv_sec) * USECS_PER_SEC +
            __le32_to_cpu(dp->tval.tv_usec);
}

dsm_time_t IRIGSensor::getUnixTime(const Sample* samp) const {
    if (samp->getDataByteLength() < 2 * sizeof(struct timeval32) + 1) return 0LL;
    const dsm_clock_data_2* dp = (const dsm_clock_data_2*)samp->getConstVoidDataPtr();
    return (dsm_time_t)__le32_to_cpu(dp->unixt.tv_sec) * USECS_PER_SEC +
            __le32_to_cpu(dp->unixt.tv_usec);
}

unsigned char IRIGSensor::getStatus(const Sample* samp) const {
    if (samp->getDataByteLength() < 2 * sizeof(struct timeval32) + 1) {
        const dsm_clock_data* dp = (const dsm_clock_data*)samp->getConstVoidDataPtr();
        return dp->status;
    }
    else {
        const dsm_clock_data_2* dp = (const dsm_clock_data_2*)samp->getConstVoidDataPtr();
        return dp->status;
    }
}

bool IRIGSensor::process(const Sample* samp,std::list<const Sample*>& result)
	throw()
{
    SampleT<float>* osamp = getSample<float>(_nvars);
    osamp->setTimeTag(samp->getTimeTag());
    osamp->setId(_sampleId);
    // clock difference, IRIG-UNIX
    if (samp->getDataByteLength() >= 2 * sizeof(struct timeval32) + 1)
        osamp->getDataPtr()[0] = (float)(getIRIGTime(samp) - getUnixTime(samp)) / USECS_PER_SEC;
    else
        osamp->getDataPtr()[0] = floatNAN;
    if (_nvars > 1)
        osamp->getDataPtr()[1] = (float)getStatus(samp);     // status value
    result.push_back(osamp);

    return true;
}

void IRIGSensor::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    DSMSensor::fromDOMElement(node);
    int ntags = getSampleTags().size();

    if (ntags == 0 || ntags > 1)
    	throw n_u::InvalidParameterException(getName(),"<sample>",
		"should only be one <sample> tag");

    const SampleTag* stag = *getSampleTags().begin();
    // hack for old XML configs that don't set the rate of the IRIG data.
    if (stag->getRate() == 0.0) {
        ILOG(("%s: setting rate to 1.0",getName().c_str()));
        SampleTag* nc_stag = *getncSampleTags().begin();
        nc_stag->setRate(1.0);
    }

    _sampleId = stag->getId();
    _nvars = stag->getVariables().size();
}


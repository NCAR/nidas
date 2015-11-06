// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include <nidas/linux/irig/pc104sg.h>

#include <nidas/dynld/raf/IRIGSensor.h>
#include <nidas/core/DSMEngine.h>
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

/* static */
const n_u::EndianConverter* IRIGSensor::lecvtr = n_u::EndianConverter::getConverter(
        n_u::EndianConverter::EC_LITTLE_ENDIAN);

IRIGSensor::IRIGSensor():
    _sampleId(0),_nvars(0),_nStatusPrints(0),_slews()
{
}

IRIGSensor::~IRIGSensor() {
}

IODevice* IRIGSensor::buildIODevice() throw(n_u::IOException)
{
    return new UnixIODevice();
}

SampleScanner* IRIGSensor::buildSampleScanner()
    throw(n_u::InvalidParameterException)
{
    setDriverTimeTagUsecs(USECS_PER_TMSEC);
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
}

/**
 * Get the current time from the IRIG card.
 * This is not meant to be used for frequent use.
 */
dsm_time_t IRIGSensor::getIRIGTime() throw(n_u::IOException)
{
    struct pc104sg_status status;
    ioctl(IRIG_GET_STATUS,&status,sizeof(status));

    struct timeval32 tval;
    ioctl(IRIG_GET_CLOCK,&tval,sizeof(tval));

#ifdef DEBUG
    cerr << "IRIG_GET_CLOCK=" << tval.tv_sec << ' ' <<
	tval.tv_usec << ", status=0x" << hex << (int)status.statusOR << dec << endl;
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

    struct pc104sg_status status;
    unsigned char statusOR;
    ioctl(IRIG_GET_STATUS,&status,sizeof(status));

    statusOR = status.statusOR;

    n_u::Logger::getInstance()->log(LOG_DEBUG,
    	"IRIG_GET_STATUS=0x%x (%s)",(unsigned int)statusOR,
    	statusString(statusOR,false).c_str());

    // cerr << "IRIG_GET_STATUS=0x" << hex << (unsigned int)statusOR << dec << 
    // 	" (" << statusString(statusOR,false) << ')' << endl;

    irigTime = getIRIGTime();
    unixTime = n_u::getSystemTime();

    if (statusOR & (CLOCK_STATUS_NOSYNC | CLOCK_STATUS_NOCODE | CLOCK_STATUS_NOYEAR | CLOCK_STATUS_NOMAJT)) {
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
        statusOR = status.statusOR;

	irigTime = getIRIGTime();
	unixTime = n_u::getSystemTime();

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
}

void IRIGSensor::close() throw(n_u::IOException)
{
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
    	{0x04,{"PPS_LOCK","NOPPS_LOCK"},
		{"PPS_LOCK","<font color=red><b>NOPPS_LOCK</b></font>"}},
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

/* static */
string IRIGSensor::shortStatusString(unsigned char status,bool xml)
{
    static const struct IRIGStatusCode {
        unsigned char mask;
	const char* str[2];	// state when bit=0, state when bit=1
	const char* xml[2];	// state when bit=0, state when bit=1
    } statusCode[] = {
	{0x20,{"S","s"},
		{"S","<font color=red><b>s</b></font>"}},
	{0x10,{"Y","y"},
		{"Y","<font color=red><b>y</b></font>"}},
    	{0x08,{"M","m"},
		{"M","<font color=red><b>m</b></font>"}},
    	{0x04,{"P","p"},
		{"P","<font color=red><b>p</b></font>"}},
    	{0x02,{"C","c"},
		{"C","<font color=red><b>c</b></font>"}},
    	{0x01,{"S","s"},
		{"S","<font color=red><b>s</b></font>"}},
    };
    ostringstream ostr;
    for (unsigned int i = 0; i < sizeof(statusCode)/sizeof(struct IRIGStatusCode); i++) {
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
    struct pc104sg_status status;
    unsigned char statusOR;

    try {
	ioctl(IRIG_GET_STATUS,&status,sizeof(status));
        statusOR = status.statusOR;

	ostr << "<td align=left>" << statusString(statusOR,true) <<
		" (0x" << hex << (int)statusOR << dec << ')';
	irigTime = getIRIGTime();
	unixTime = n_u::getSystemTime();
        float dt = (float)(irigTime - unixTime)/USECS_PER_SEC; 
        bool iwarn = fabsf(dt) > .05;

	ostr << ", IRIG-UNIX=" <<
            (iwarn ? "<font color=red><b>" : "") <<
            fixed << setprecision(4) << dt << " sec" <<
            (iwarn ? "</b></font>" : "");

        iwarn = status.syncToggles > 20;
        ostr <<
            ", syncTgls=" <<
            (iwarn ? "<font color=red><b>" : "") << status.syncToggles <<
            (iwarn ? "</b></font>" : "") <<
            ",clockResets=" << status.softwareClockResets <<
            "</td>" << endl;
    }
    catch(const n_u::IOException& ioe) {
        ostr << "<td>" << ioe.what() << "</td>" << endl;
	n_u::Logger::getInstance()->log(LOG_ERR,
            "%s: printStatus: %s",getName().c_str(),
            ioe.what());
    }

    // Currently the status thread in DSMEngine queries the sensors once 
    // every 3 seconds.  For initial testing we'll log this information
    // every minute.  The driver accumulates the slew counts. We'll print
    // the difference from the last.
    if (!(_nStatusPrints++ % 20)) {
        ostringstream ostr2;
        for (unsigned int i = 0; i < sizeof(status.slews)/sizeof(status.slews[0]); i++) {
            if (i > 0) ostr2 << ' ';
            if (i + IRIG_MIN_DT_DIFF == 0) ostr2 << (i + IRIG_MIN_DT_DIFF) << ":";
            ostr2 << status.slews[i] - _slews[i];
            _slews[i] = status.slews[i];
        }
        NLOG(("%s: slews=",getName().c_str()) << ostr2.str() <<
                ", resets=" << status.softwareClockResets <<
                ", tgls=" << status.syncToggles);
    }
}

dsm_time_t IRIGSensor::getIRIGTime(const Sample* samp) const {
    const dsm_clock_data* dp = (dsm_clock_data*)samp->getConstVoidDataPtr();
    return (dsm_time_t)lecvtr->int32Value(dp->tval.tv_sec) * USECS_PER_SEC +
            lecvtr->int32Value(dp->tval.tv_usec);
}

dsm_time_t IRIGSensor::getUnixTime(const Sample* samp) const {
    if (samp->getDataByteLength() < 2 * sizeof(struct timeval32) + 1) return 0LL;
    const dsm_clock_data_2* dp = (const dsm_clock_data_2*)samp->getConstVoidDataPtr();
    return (dsm_time_t)lecvtr->int32Value(dp->unixt.tv_sec) * USECS_PER_SEC +
            lecvtr->int32Value(dp->unixt.tv_usec);
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

float IRIGSensor::get100HzBacklog(const Sample* samp) const {
    if (samp->getDataByteLength() < 2 * sizeof(struct timeval32) + 5) return floatNAN;
    const dsm_clock_data_2* dp = (const dsm_clock_data_2*)samp->getConstVoidDataPtr();
    return (int)dp->max100HzBacklog; // convert from unsigned char
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
    if (_nvars > 2)
        osamp->getDataPtr()[2] = get100HzBacklog(samp);
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

    SampleTag* stag = getSampleTags().front();

    // hack for XML configs that don't set the rate of the IRIG data.
    if (stag->getRate() == 0.0) stag->setRate(1.0);

    _sampleId = stag->getId();
    _nvars = stag->getVariables().size();
}


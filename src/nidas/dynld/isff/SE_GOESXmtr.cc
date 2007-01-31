/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/dynld/isff/SE_GOESXmtr.h>
#include <nidas/dynld/isff/GOES.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/dynld/isff/GOESException.h>
#include <nidas/util/UTime.h>

#include <nidas/util/Logger.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>

using namespace std;
using namespace nidas::core;
using namespace nidas::dynld::isff;

namespace n_u = nidas::util;

// #define DEBUG

NIDAS_CREATOR_FUNCTION_NS(isff,SE_GOESXmtr)

/* static */
const std::string SE_GOESXmtr::SOH_STR = string(1,SOH);

/* static */
struct SE_GOESXmtr::SE_Codes SE_GOESXmtr::cmdCodes[] = {
    {PKT_GET_ID,		"Read ID"},
    {PKT_SET_ID,		"Set ID in ram or eeprom"},
    {PKT_SET_TIME,		"Set time-of-day"},
    {PKT_GET_TIME,		"Get time-of-day"},
    {PKT_XMT_DATA,		"Transmit data, model SE110"},
    {PKT_CANCEL_XMT,		"Cancel tranmission"},
    {PKT_GET_XMT_QUE,		"Display transmit queue"},
    {PKT_QUERY,			"Query"},
    {PKT_SET_GLOBALS,		"Set global parameters"},
    {PKT_GET_GLOBALS,		"Get global parameters"},
    {PKT_DISPLAY_VERSION,	"Display version"},
    {PKT_XMT_DATA_SE120,	"Transmit data, model SE120/1200"},
    {PKT_GET_SE1200_STATUS,	"Get SE1200 status info"},
    {PKT_RESET_XMTR,		"Reset"},
    {PKT_SELFTEST_DISPL,	"Display SELF TEST RESULTS"},
    {PKT_SELFTEST_START,	"Initiate SELF TEST"},
    {PKT_SEND_FIXED_CHAN,	"Turn on UNMODULATED Channel CARRIER"},
    {PKT_SOFTWARE_LOAD,		"Software Load"},
    {PKT_ACKNOWLEDGE,		"Acknowledge"},
    {PKT_ERR_RESPONSE,		"Error response packet"},
};

/* static */
string SE_GOESXmtr::codeString(char pktType)
{
    for (unsigned int i = 0; i < sizeof(cmdCodes)/sizeof(cmdCodes[0]); i++)
        if (pktType == cmdCodes[i].value) return cmdCodes[i].msg;
    ostringstream ost;
    ost << "unknown packet type code: " << hex << pktType;
    return ost.str();
}


/* static */
const char* SE_GOESXmtr::statusCodeStrings[] = {
    "Packet transmission OK",
    "Illegal request",
    "Unknown status",
    "Item not found",
    "Invalid data or time",
    "Transmission time overlap",
    "Invalid channel",
    "Invalid transmit interval",
    "EEPROM not updated",
    "Invalid repeat count",
    "Clock not loaded",
    "CRC error on load",
};

/* static */
const char* SE_GOESXmtr::errorCodeStrings[] = {
    "Packet OK",
    "Received Packet Too Long",
    "Received Packet Too Short",
    "Received Packet Checksum Error",
    "Invalid Type Code",
    "Length Field mismatch",
    "Unable to Allocate Memory",
    "Invalid Sequence Number",
    "Command Timeout for multi-packet Command",
};

SE_GOESXmtr::selfTest SE_GOESXmtr::selfTestCodes[2][10] = {
    // model 110
    {
    	{0x0001,"Ext RAM Test Failure"},
    	{0x0002,"Ext RAM PageTest Failure"},
    	{0x0004,"Battery Voltage < 10.5 Volts"},
    	{0x0008,"EEPROM1 Program CRC Test Failure"},
    	{0x0100,"RF Supply Voltage Failure"},
    	{0x0200,"RF PLL Lock Failure"},
    	{0x0000,""},
    	{0x0000,""},
    	{0x0000,""},
    	{0x0000,""},
    },
    // model 120, 1200
    {
    	{0x0004,"Battery Voltage < 10.0 Volts"},
    	{0x0008,"Software Boot Code Flash CRC Error"},
    	{0x0010,"RS232 Software Flash CRC Error"},
    	{0x0020,"Temperature Sensor Test Failure"},
    	{0x0040,"TCX0 DAC Test Failure"},
    	{0x0100,"HSB Software Flash CRC Error"},
    	{0x0200,"RF PLL Lock Failure"},
    	{0x0400,"TOD Interrrupt Test Failure"},
    	{0x0800,"Modulation Interrupt Test Failure"},
    	{0x1000,"Manufacturing Data Flash CRC Error"},
    },
};

void SE_GOESXmtr::checkResponse(char ptype,const string& resp)
	throw(n_u::IOException)
{
    if (resp.length() < 2) {
	tosleep();
        throw n_u::IOException(getName(),codeString(ptype),
		"short response");
    }

    if (resp[0] == PKT_ERR_RESPONSE) {
	tosleep();
	// return negative value in status indicating
	// one of the GOES error codes, like "invalid type code".
        throw GOESException(getName(),codeString(ptype),
		errorCodeStrings[(int)resp[1]],-(int)resp[1]);
    }

    if (resp[0] != ptype) {
	tosleep();
        throw n_u::IOException(getName(),codeString(ptype),
		codeString((int)resp[0]));
    }

    if (resp[1] != 0) {
	tosleep();
        throw GOESException(getName(),codeString(ptype),
		statusCodeStrings[(int)resp[1]],resp[1]);
    }
}

void SE_GOESXmtr::checkACKResponse(char ptype,const string& resp,char seqnum)
	throw(n_u::IOException)
{
    if (resp.length() < 2) {
	tosleep();
        throw n_u::IOException(getName(),codeString(ptype),
		"short response");
    }

    if (resp[0] == PKT_ERR_RESPONSE) {
	tosleep();
        throw GOESException(getName(),codeString(ptype),
		errorCodeStrings[(int)resp[1]],-(int)resp[1]);
    }

    if (resp[0] != PKT_ACKNOWLEDGE) {
	tosleep();
        throw n_u::IOException(getName(),codeString(ptype),
		codeString((int)resp[0]));
    }

    if (resp.length() < 4) {
	tosleep();
        throw n_u::IOException(getName(),codeString(ptype),
		"short ACK response");
    }

    if (resp[2] != PKT_XMT_DATA) {
	tosleep();
        throw n_u::IOException(getName(),codeString(ptype),
		errorCodeStrings[(int)resp[2]]);
    }

    if (resp[3] != seqnum) {
	tosleep();
	ostringstream ost;
	ost << "wrong sequence number, " << (int)resp[3] << " should be " <<
		(int)seqnum;
        throw n_u::IOException(getName(),codeString(ptype),ost.str());
    }
}

SE_GOESXmtr::SE_GOESXmtr():
	model(0),clockDiffMsecs(99999),
	transmitQueueTime((time_t)0),
	transmitAtTime((time_t)0),
	transmitSampleTime((time_t)0),
	lastXmitStatus("unknown"),
	selfTestStatus(0),
	maxRFRate(0),
	gpsNotInstalled(false),
	xmitNbytes(0),
	activeId(0)
{
    logger = n_u::Logger::getInstance();
    port.setBaudRate(9600);
    port.setParity(port.NONE);
    port.setDataBits(8);
    port.setStopBits(1);
    port.setFlowControl(port.NOFLOWCONTROL);
    port.setLocal(true);
    port.setRaw(true);
    port.setRawLength(0);
    port.setRawTimeout(10);
}

SE_GOESXmtr::SE_GOESXmtr(const SE_GOESXmtr& x):
	GOESXmtr(x),
	model(0),clockDiffMsecs(99999),
	transmitQueueTime((time_t)0),
	transmitAtTime((time_t)0),
	transmitSampleTime((time_t)0),
	lastXmitStatus("unknown"),
	selfTestStatus(0),
	maxRFRate(0),
	gpsNotInstalled(false),
	xmitNbytes(0),
	activeId(0)
{
    logger = n_u::Logger::getInstance();
    port.setBaudRate(9600);
    port.setParity(port.NONE);
    port.setDataBits(8);
    port.setStopBits(1);
    port.setFlowControl(port.NOFLOWCONTROL);
    port.setLocal(true);
    port.setRaw(true);
    port.setRawLength(0);
    port.setRawTimeout(10);
}

SE_GOESXmtr::~SE_GOESXmtr()
{
}

void SE_GOESXmtr::init() throw(n_u::IOException)
{
    try {
	query();
	detectModel();
	setXmtrId();
	checkClock();
        lastXmitStatus = "OK";
    }
    catch(const n_u::IOException& e) {
        lastXmitStatus = e.what();
	throw;
    }
}

string SE_GOESXmtr::getSelfTestStatusString() 
{
    string res;
    int imodel = 0;
    if (getModel() == 120 || getModel() == 1200) imodel = 1;
    int ncodes =
    	sizeof(selfTestCodes[imodel]) / sizeof(selfTestCodes[imodel][0]);
    for (int i = 0; i < ncodes; i++) {
	if (selfTestStatus & selfTestCodes[imodel][i].mask) {
	    if (res.length() > 0) res += ", ";
	    res += selfTestCodes[imodel][i].text;
	}
    }
    if (gpsNotInstalled) res += "No GPS Rcvr";
    if (res.length() == 0) res = "OK";
    return res;
}

void SE_GOESXmtr::query() throw(n_u::IOException)
{
    wakeup();
    send(PKT_QUERY);

    string resp = recv();
    checkResponse(PKT_QUERY,resp);
    tosleep();
}

// This takes 20 seconds on a model 110, 15 seconds on a 120 or 1200
void SE_GOESXmtr::doSelfTest() throw(n_u::IOException)
{
    wakeup();
    send(PKT_SELFTEST_START);

    string resp = recv();
    checkResponse(PKT_SELFTEST_START,resp);
    tosleep();
}

void SE_GOESXmtr::reset() throw(n_u::IOException)
{
    logger->log(LOG_INFO,"%s: doing transmitter reset and self test",
	    getName().c_str());
    wakeup();
    send(PKT_RESET_XMTR);

    string resp = recv();
    checkResponse(PKT_RESET_XMTR,resp);

    // This takes 20 seconds on a model 110, 15 seconds on a 120 or 1200
    send(PKT_SELFTEST_START);

    resp = recv();
    checkResponse(PKT_SELFTEST_START,resp);
    tosleep();

    // this will force a detectModel() before the next tranmission
    setModel(0);	
}

int SE_GOESXmtr::checkStatus() throw(n_u::IOException)
{
    wakeup();
    send(PKT_DISPLAY_VERSION);
    string resp = recv();

    // According to
    // "Interface Control Doc for Serial Control of SE110 GOES DCP Transmitter",
    // Rev A, 15 May 1995, PKT_DISPLAY_VERSION is not supported on an SE110,
    // and results in an error packet (type=0xf0) with an error=ERR_BADTYE.
    //
    // However: Testing has shown that SE110 SN:1005 returns a good
    // response to PKT_DISPLAY_VERSION, and so this test will
    // determine that it is a 120.
    // However that unit does not support model 120 transmissions
    // of type PKT_XMT_DATA_SE120, so we must try a test transmission
    // on units that appear to be a 120.

    int lmodel = 110;
    maxRFRate = 100;

    softwareBuildDate = "unknown";
    try {
	checkResponse(PKT_DISPLAY_VERSION,resp);
    }
    catch(const GOESException& e) {
	// checkResponse does a toSleep() if it throws an exception
        if (e.getStatus() == -ERR_BADTYPE) {
	    logger->log(LOG_INFO,"%s: checkStatus, model=%d",
		    getName().c_str(),lmodel);
	    return lmodel;
	}
	throw;
    }

    softwareBuildDate = resp.substr(34,10) + " " + resp.substr(18,8);

    send(PKT_SELFTEST_DISPL);
    resp = recv();
    checkResponse(PKT_SELFTEST_DISPL,resp);
    tosleep();

    if (resp.length() < 10)
        throw n_u::IOException(getName(),"display self test",
		"short response");

    // grab the self test status bytes, least signif bits in first byte
#if __BYTE_ORDER == __BIG_ENDIAN
    swab(resp.c_str()+2,&selfTestStatus,2);
#else
    memcpy(&selfTestStatus,resp.c_str()+2,2);
#endif

    if (resp[9] == 0) lmodel = 120;
    if (resp[9] == 5) {
        lmodel = 1200;
	maxRFRate = 300;
    }
    if (resp[9] == 6) {
        lmodel = 1200;
	maxRFRate = 1200;
    }

    gpsNotInstalled = false;

    if (lmodel == 1200 && resp[8] != 1) {
        gpsNotInstalled = true;
    	logger->log(LOG_WARNING,"GOES transmitter %s: GPS not installed",
		getName().c_str());
    }
    return lmodel;
}

int SE_GOESXmtr::detectModel() throw(n_u::IOException)
{
    int lmodel = checkStatus();

    // If model looks like a 120, double check with a test transmission.
    if (lmodel == 120) {
	checkClock();	// set clock before test transmission
        if (!testTransmitSE120()) lmodel = 110;
    }

    setModel(lmodel);
#ifdef DEBUG
    logger->log(LOG_INFO,"%s: detectModel, model=%d",
	    getName().c_str(),lmodel);
#endif
    return lmodel;
}

void SE_GOESXmtr::printStatus() throw()
{
    if (getStatusFile().length() > 0) {
        ofstream ost(getStatusFile().c_str());
	if (ost.fail())
	    logger->log(LOG_ERR,"GOES status file %s: %s",
		    getStatusFile().c_str(),strerror(errno));
	printStatus(ost);
	ost.close();
    }
    else printStatus(cout);
}

void SE_GOESXmtr::printStatus(ostream& ost) throw()
{
        
    ost << "SE GOES Transmitter\n" <<
	"dev:\t\t" << port.getName() << '\n' <<
    	"model:\t\t" << getModel() << '\n' <<
	"SE software:\t" << softwareBuildDate << '\n' <<
	"self test:\t" << getSelfTestStatusString() << '\n' <<
	"id:\t\t" << hex << setw(8) << setfill('0') <<
		activeId << dec << '\n' <<
	"channel:\t" << getChannel() << '\n' <<
	"RFbaud:\t\t" << getRFBaud() << ", max=" << maxRFRate << '\n' <<
	"xmit interval:\t" << getXmitInterval() << " sec\n" <<
	"xmit offset:\t" << getXmitOffset() << " sec\n" <<
	"xmtr clock:\t" << abs(clockDiffMsecs) << " msec " <<
	    	(clockDiffMsecs > 0 ? "ahead of" : "behind") <<
		" UNIX clock\n" <<
	"xmit queued at:\t" << transmitQueueTime.format(true,"%c") << '\n' <<
	"xmit time:\t" << transmitAtTime.format(true,"%c") << '\n' <<
	"data time:\t" << transmitSampleTime.format(true,"%c") << '\n' <<
	"last transmit:\t" << xmitNbytes << " bytes\n" <<
	"last status:\t" << lastXmitStatus << endl;
}

void SE_GOESXmtr::setXmtrId() throw(n_u::IOException)
{
    char cmd[] = {PKT_SET_ID,0,0,0,0,0};
    unsigned long lid = getId();

#if __BYTE_ORDER == __BIG_ENDIAN
    memcpy(cmd+2,&lid);
#else
    union {
        unsigned long id;
	char bytes[4];
    } idu;
    idu.id = lid;
    // flip the bytes
    cmd[2] = idu.bytes[3];
    cmd[3] = idu.bytes[2];
    cmd[4] = idu.bytes[1];
    cmd[5] = idu.bytes[0];
#endif

    wakeup();
    send(string(cmd,6));

    string resp = recv();
    checkResponse(PKT_SET_ID,resp);
    tosleep();
    activeId = lid;
}

unsigned long SE_GOESXmtr::getXmtrId() throw(n_u::IOException)
{
    char cmd[] = {PKT_GET_ID,0};

    wakeup();
    send(string(cmd,2));
    string resp = recv();
    checkResponse(PKT_GET_ID,resp);
    tosleep();

#if __BYTE_ORDER == __BIG_ENDIAN
    memcpy(&activeId,resp.c_str()+2,sizeof(activeId));
#else
    union {
        unsigned long id;
	char bytes[4];
    } idu;
    // flip the bytes
    idu.bytes[3] = resp[2];
    idu.bytes[2] = resp[3];
    idu.bytes[1] = resp[4];
    idu.bytes[0] = resp[5];
    activeId = idu.id;
#endif
    return activeId;
}

unsigned long SE_GOESXmtr::checkId() throw(n_u::IOException)
{
    unsigned long lid = getXmtrId();
    if (lid != getId()) {
	logger->log(LOG_WARNING,
		"%s: incorrect id: %x, should be %x. Resetting",
		getName().c_str(),activeId,getId());
	setXmtrId();
    }
    return lid;
}

int SE_GOESXmtr::checkClock() throw(n_u::IOException)
{
    long long diff = USECS_PER_SEC * 10;
    try {
	diff = getXmtrClock() - n_u::UTime() + getXmtrClockDelay(14);
    }
    catch (const GOESException& e) {
        if (e.getStatus() != PKT_STATUS_CLOCK_NOT_LOADED) throw e;
    }

    // Precision of SE clocks is 1/10th of a second,
    // so we'll check that it is within .150 secs of
    // system clock.
    if (::llabs(diff) > USECS_PER_MSEC * 150) {
	logger->log(LOG_WARNING,
		"%s: goes clock is %s system clock by %d milliseconds. Setting GOES clock",
		getName().c_str(),(diff > 0 ? "ahead of" : "behind"),
			::llabs(diff) / USECS_PER_MSEC);
	setXmtrClock();
	diff = getXmtrClock() - n_u::UTime() + getXmtrClockDelay(14);
    }
    clockDiffMsecs = diff / USECS_PER_MSEC;
    return clockDiffMsecs;
}

void SE_GOESXmtr::setXmtrClock() throw(n_u::IOException)
{
    char cmd[] = {PKT_SET_TIME,0,0,0,0,0,0,0,0};

    n_u::UTime ut = n_u::UTime() + getXmtrClockDelay(sizeof(cmd) + 4);
    encodeClock(ut,cmd+2,true);

    wakeup();
    send(string(cmd,9));
    string resp = recv();
    checkResponse(PKT_SET_TIME,resp);
    tosleep();
}

n_u::UTime SE_GOESXmtr::getXmtrClock() throw(n_u::IOException)
{
    char cmd[] = {PKT_GET_TIME};

    wakeup();
    send(string(cmd,1));
    string resp = recv();

    try {
	checkResponse(PKT_GET_TIME,resp);
    }
    catch (const GOESException& e) {
        // if (e.getStatus() != PKT_STATUS_CLOCK_NOT_LOADED) throw e;
	// if status is PKT_STATUS_CLOCK_NOT_LOADED the time data is all 0
	// which will be returned as 1991 Dec 31 00:00
	throw e;
    }
    tosleep();

    if (resp.length() > 9) return decodeClock(resp.c_str() + 3);
    return n_u::UTime((time_t)0);
}
int SE_GOESXmtr::getXmtrClockDelay(int nchar) const
{
    // Assume 10 transmitted bits per byte
    return  nchar * 10 * USECS_PER_SEC / port.getBaudRate();
}

void SE_GOESXmtr::encodeClock(const n_u::UTime& ut,char* out,bool fractsecs)
{
    struct tm tfields;
    int usecs;
    ut.toTm(true,&tfields,&usecs);

    out[0] = tfields.tm_year - 92;
    short int yday = tfields.tm_yday + 1;
#if __BYTE_ORDER == __BIG_ENDIAN
    memcpy(out+1,&yday,2);
#else
    swab(&yday,out+1,2);
#endif
    out[3] = tfields.tm_hour;
    out[4] = tfields.tm_min;
    out[5] = tfields.tm_sec;
    // fractional seconds field on 110s is 1/100 of a sec
    // on 120 and 1200s it is 1/10 sec
    if (fractsecs) {
        if (getModel() == 110) out[6] = usecs / (USECS_PER_SEC / 100);
	else out[6] = usecs / (USECS_PER_SEC / 10);
    }
}

n_u::UTime SE_GOESXmtr::decodeClock(const char* pkt)
{
    int year = pkt[0] + 1992;
    short int yday;
#if __BYTE_ORDER == __BIG_ENDIAN
    memcpy(&yday,pkt+1,2);
#else
    swab(pkt+1,&yday,2);
#endif
    int hour = pkt[3];
    int min = pkt[4];
    int sec = pkt[5];

    // fractional seconds field on 110s is 1/100 of a sec
    // on 120 and 1200s it is 1/10 sec
    int usec = 0;

#ifdef DEBUG
    logger->log(LOG_INFO,"%s: model=%d, fract secs=%d",
	    getName().c_str(),getModel(),pkt[6]);
#endif

    if (getModel() == 110) {
	// check that fractional seconds are as documented in Signal Eng manual.
	if (pkt[6] % 10) {
	    logger->log(LOG_WARNING,"%s: model=%d, unexpected fract secs=%d",
		    getName().c_str(),getModel(),pkt[6]);
	}
	usec = pkt[6] * 10 * USECS_PER_MSEC;
    }
    else {
	if (pkt[6] > 9) {
	    logger->log(LOG_WARNING,"%s: model=%d, unexpected fract secs=%d",
		    getName().c_str(),getModel(),pkt[6]);
	}
	usec = pkt[6] * 100 * USECS_PER_MSEC;
    }

#ifdef DEBUG
    cerr << "clock=" << year << ' ' << yday << ' ' <<
    	hour << ' ' << min << ' ' << sec << ' ' << usec << endl;
    cerr << "UTime=" <<
    	n_u::UTime(true,year,yday,hour,min,sec,usec).format(true,"%%Y %m %d %H:%M:%S.%3f") << endl;;
#endif
    return n_u::UTime(true,year,yday,hour,min,sec,usec);
}

void SE_GOESXmtr::transmitData(const n_u::UTime& at, int configid,
	const Sample* samp) throw(n_u::IOException)
{
    for (int i = 0; i < 2; i++) {
	try {
	    transmitQueueTime = n_u::UTime();
	    transmitAtTime = at;
	    transmitSampleTime = samp->getTimeTag();
	    if (getModel() == 0) detectModel();
	    checkId();
	    checkClock();

	    if (getModel() == 110) transmitDataSE110(at,configid,samp);
	    else if (getModel() != 0) transmitDataSE120(at,configid,samp);
	    lastXmitStatus = "OK";
	    break;
	}
	catch(const GOESException& e) {
	    lastXmitStatus = string(e.what());

	    // sending a 120 transmit command to a 110
	    // results in an ERR_BADTYPE
	    // sending a 110 transmit command to a 120
	    // results in an ERR_TOOLONG
	    if (i < 2 &&
	    	(e.getStatus() == -(int)ERR_BADTYPE ||
			e.getStatus() == -(int)ERR_TOOLONG)) {
		    logger->log(LOG_ERR,"%s: %s. Will re-check model number",
			getName().c_str(),e.what());
		    setModel(0);
	    }
	    else throw;
	}
	catch(const n_u::IOException& e) {
	    lastXmitStatus = string(e.what());
	    throw;
	}
    }
}

void SE_GOESXmtr::transmitDataSE110(const n_u::UTime& at, int configid,
	const Sample* samp) throw(n_u::IOException)
{
    xmitNbytes = 0;

    char pkt[256];
    char seqnum = 0;

    // initial transmit request packet containing transmit time
    memset(pkt,0,sizeof(pkt));

    // When assembling these transmission packets we use the
    // byte indices as shown in the manual, to make it easier
    // to understand. For example, the packet type code goes in byte 2.
    // But since the send method adds the SOH and
    // length byte, we don't send the first two bytes.
    pkt[2] = PKT_XMT_DATA;
    pkt[3] = seqnum;		// sequence number

    encodeClock(at,pkt+6,false);

    short int schan = getChannel();
#if __BYTE_ORDER == __BIG_ENDIAN
    memcpy(pkt+36,&schan,2);
#else
    swab(&schan,pkt+36,2);
#endif
    
    wakeup();
    // Since the send method adds the SOH and length bytes,
    // we send the packet starting at byte 2.
    send(string(pkt+2,38));

    string resp = recv();
    checkACKResponse(PKT_XMT_DATA,resp,seqnum);

    // send comments packet (content-free!)
    memset(pkt,0,sizeof(pkt));
    pkt[2] = PKT_XMT_DATA;
    pkt[3] = ++seqnum;		// sequence number
    send(string(pkt+2,3));	// send method adds SOH,length, CRC, EOT

    resp = recv();
    checkACKResponse(PKT_XMT_DATA,resp,seqnum);

    assert(samp->getType() == FLOAT_ST);
    const SampleT<float>* fsamp = static_cast<const SampleT<float>*>(samp);

    const float* fptr = fsamp->getConstDataPtr();
    const float* endInput = fptr + fsamp->getDataLength();

    bool first = true;
    char* endPkt = pkt + 242;	// max amount of data in packet

    // When assembling these data packets we use the first
    // two bytes.
    while (fptr < endInput) {
	// setup data packet
	char* pktptr = pkt;
	*pktptr++ = PKT_XMT_DATA;
	*pktptr++ = ++seqnum;		// sequence number
	if (first) {
	    *pktptr++ = (configid & 0x3f) | 0x40;
	    *pktptr++ = (samp->getShortId() & 0x3f) | 0x40;
	    first = false;
	    xmitNbytes += 2;
	}

	while (fptr < endInput && pktptr+4 <= endPkt) {
	    GOES::float_encode_4x6(*fptr++,pktptr);
	    pktptr += 4;
	    xmitNbytes += 4;
	}

	send(string(pkt,pktptr-pkt));
	resp = recv();
	checkACKResponse(PKT_XMT_DATA,resp,seqnum);
    }

    // send trailer packet with no data
    memset(pkt,0,sizeof(pkt));
    pkt[2] = PKT_XMT_DATA;
    pkt[3] = ++seqnum;		// sequence number
    send(string(pkt+2,2));	// send method adds SOH and length

    resp = recv();
    checkResponse(PKT_XMT_DATA,resp);
    tosleep();
}

/* This method also works for the SE1200 at 100 BPS.
 * If using 300 or 1200 BPS, one must twiddle with bytes 33 and 34,
 * which is not done in this version.
 */
void SE_GOESXmtr::transmitDataSE120(const n_u::UTime& at, int configid,
	const Sample* samp) throw(n_u::IOException)
{
    xmitNbytes = 0;

    assert(samp->getType() == FLOAT_ST);
    const SampleT<float>* fsamp = static_cast<const SampleT<float>*>(samp);
    short int ndata = fsamp->getDataLength() * 4 + 2;

    int pktlen = ndata + 64;

    // use auto_ptr so this is automatically deleted on exit
    auto_ptr<char> autopkt(new char[pktlen]);

    char* pkt = autopkt.get();

    memset(pkt,0,pktlen);

    // When assembling these transmission packets we use the
    // byte indices as shown in the manual, to make it easier
    // to understand. For example, the packet type code goes in byte 2.
    // But since the send method adds the SOH and
    // length byte, we don't send the first two bytes.
    pkt[2] = PKT_XMT_DATA_SE120;

    encodeClock(at,pkt+5,false);

    short int schan = getChannel();

#if __BYTE_ORDER == __BIG_ENDIAN
    memcpy(pkt+29,&schan,2);
    memcpy(pkt+31,&ndata,2);
#else
    swab(&schan,pkt+29,2);
    swab(&ndata,pkt+31,2);
#endif

    const float* fptr = fsamp->getConstDataPtr();
    const float* endInput = fptr + fsamp->getDataLength();
    char* pktptr = pkt + 64;

    *pktptr++ = (configid & 0x3f) | 0x40;
    *pktptr++ = (samp->getShortId() & 0x3f) | 0x40;
    xmitNbytes += 2;

    while (fptr < endInput) {
	GOES::float_encode_4x6(*fptr++,pktptr);
	pktptr += 4;
	xmitNbytes += 4;
    }

    wakeup();
#ifdef DEBUG
    cerr << "send length=" << (pktptr-pkt-2) <<
    	" pktlen=" << pktlen << endl;;
#endif
    send(string(pkt+2,pktptr-pkt-2));
    string resp = recv();
    checkResponse(PKT_XMT_DATA_SE120,resp);
    tosleep();
}

/*
 * Schedule a test transmission of 0 data bytes using the SE120 
 * transmit command.  If it fails with an invalid command
 * type code then it isn't really a SE120.
 * If it succeeds, cancel it.
 */
bool SE_GOESXmtr::testTransmitSE120()
	throw(n_u::IOException)
{
    unsigned int periodUsec = getXmitInterval() * USECS_PER_SEC;
    unsigned int offsetUsec = getXmitOffset() * USECS_PER_SEC;

    n_u::UTime at;

    at += periodUsec - (at.toUsecs() % periodUsec) + offsetUsec;
#ifdef DEBUG
    cerr << "test transmit at=" << at.format(true,"%c") << endl;
#endif

    short int ndata = 0;
    int pktlen = ndata + 64;

    // use auto_ptr so this is automatically deleted on exit
    auto_ptr<char> autopkt(new char[pktlen]);

    char* pkt = autopkt.get();

    memset(pkt,0,pktlen);

    pkt[2] = PKT_XMT_DATA_SE120;

    encodeClock(at,pkt+5,false);

    short int schan = getChannel();
#ifdef DEBUG
    cerr << "schan=" << schan << endl;
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
    memcpy(pkt+29,&schan,2);
    memcpy(pkt+31,&ndata,2);
#else
    swab(&schan,pkt+29,2);
    swab(&ndata,pkt+31,2);
#endif

    char* pktptr = pkt + 64;
    wakeup();

#ifdef DEBUG
    cerr << "send length=" << (pktptr-pkt-2) <<
    	" pktlen=" << pktlen << endl;;
#endif
    send(string(pkt+2,pktptr-pkt-2));
    string resp = recv();

    try {
	checkResponse(PKT_XMT_DATA_SE120,resp);
    }
    catch(const GOESException& e) {
	logger->log(LOG_WARNING,"%s: testTransmit: %s",
		getName().c_str(),e.what());
        if (e.getStatus() == -ERR_BADTYPE) return false;
	else throw e;
    }
    tosleep();

    cancelTransmit(at);
    return true;
}

void SE_GOESXmtr::cancelTransmit(const n_u::UTime& at) throw(n_u::IOException)
{
    char cmd[] = {PKT_CANCEL_XMT,0,0,0,0,0,0,0,0};

    encodeClock(at,cmd+2,false);

    wakeup();
    send(string(cmd,9));
    string resp = recv();
    try {
	checkResponse(PKT_CANCEL_XMT,resp);
    }
    catch(const GOESException& e) {
	logger->log(LOG_ERR,"%s: cancelTransmit: %s",
		getName().c_str(),e.what());
	// these indicate transmit is in progress or done
    	if (e.getStatus() == PKT_STATUS_ILLEGAL_REQUEST ||
		e.getStatus() == PKT_STATUS_ITEM_NOT_FOUND) {
	    return;
	}
	throw e;
    }
    tosleep();
}

size_t SE_GOESXmtr::send(char c) throw(n_u::IOException)
{
    return send(string(1,c));
}

size_t SE_GOESXmtr::send(const string& msg) throw(n_u::IOException)
{
    if (getModel() == 110 && msg.length() > 242) {
        ostringstream ost;
	ost << "message too long: " << msg.length() << " bytes";
	throw n_u::IOException(getName(),"send",ost.str());
    }
    // The 2nd byte in a packet for a model 110 transmitter
    // is message length field, set to the length of the message
    // excluding the SOH, EOT, and the 3rd byte type field but
    // including the checksum, before the "fixing" is done.
    // The manuals for models 120 and 1200 show the 2nd byte
    // field to be Reserved, and not a packet length.
    // It doesn't seem to harm things to set it to the message
    // length.
    //
    // The CRC is computed before the "fixing".
    // The length field excludes the SOH,length,
    // packet type and EOT, it is just the data length and
    // the CRC.  The msg argument here includes the packet-type,
    // but not the CRC, so msg.length() gives the correct length.

    string buf = SOH_STR +
    	fix(string(1,(char)msg.length()) + msg + crc(msg)) + EOT;
    return write(buf.c_str(),buf.length());
}

/* static */
string SE_GOESXmtr::fix(const string& msg) 
{
    static string specials("\x1\x4#");
    string res = msg;

    // cerr << "res.length=" << res.length() << endl;
    for (size_t i = 0; i < res.length() &&
    	(i = res.find_first_of(specials,i)) < string::npos; i+=2) {
	res.insert(i+1,1,~(unsigned char)res[i]);
	res[i] = '#';
    }
    return res;
}

/* static */
string SE_GOESXmtr::unfix(const string& msg) 
{
    // leading SOH and trailing EOT have been removed from message
    string res = msg;
    for (size_t i = 0; i < res.length() &&
    	(i = res.find('#',i)) < string::npos; i++)
	res.replace(i,2,1,~(unsigned char)res[i+1]);
    return res;
}

/* static */
char SE_GOESXmtr::crc(const string& msg) 
{
    /*
     * msg here is the basic message, without the SOH, length byte, or the
     * trailing EOT.  The crc includes the length byte, so we
     * use the msg.length().
     */
    unsigned char crc = msg.length();
    for (size_t bi = 0; bi < msg.length(); bi++)
    	crc += (unsigned) msg[bi]; 
    return (signed) crc;
}

void SE_GOESXmtr::wakeup() throw(n_u::IOException)
{
    port.clearModemBits(TIOCM_CTS);
    // SE_120 responds with CTS after one 10 msec sleep
    if (!(port.getModemStatus() & TIOCM_CTS)) {
	port.setModemBits(TIOCM_RTS);
	// sleep(10);

	const int NTRY_FOR_CTS = 5;
	int itry;
	for (itry = 0; itry < NTRY_FOR_CTS; itry++) {
	    struct timespec ctsSleep = { 0, NSECS_PER_SEC / 100 };
	    nanosleep(&ctsSleep,0);
	    if (port.getModemStatus() & TIOCM_CTS) break;
	    if (!(port.getModemStatus() & TIOCM_RTS))
		    cerr << "RTS not on, itry=" << itry << endl;
	}
	if (itry == NTRY_FOR_CTS) {
	    port.clearModemBits(TIOCM_RTS);
	    throw n_u::IOTimeoutException(getName(),
	    	"wakeup (waiting for CTS)");
	}
	// cerr << "itry = " << itry << endl;
    }
}

void SE_GOESXmtr::tosleep() throw(n_u::IOException)
{
    port.clearModemBits(TIOCM_RTS);
    struct timespec ctsSleep = { 0, NSECS_PER_SEC / 100 };
    nanosleep(&ctsSleep,0);

    if ((port.getModemStatus() & TIOCM_CTS)) {
	const int NTRY_FOR_CTS = 5;
	int itry;
	for (itry = 0; itry < NTRY_FOR_CTS; itry++) {
	    struct timespec ctsSleep = { 0, NSECS_PER_SEC / 100 };
	    nanosleep(&ctsSleep,0);
	    if (!port.getModemStatus() & TIOCM_CTS) break;
	    if ((port.getModemStatus() & TIOCM_RTS))
		    cerr << "RTS on, itry=" << itry << endl;
	}
	if (itry == NTRY_FOR_CTS)
	    throw n_u::IOTimeoutException(getName(),
	    	"tosleep (waiting for notCTS)");
    }
}

string SE_GOESXmtr::recv() throw(n_u::IOException)
{
    // cerr << "reading" << endl;
    char buf[256];
    size_t len = read(buf,sizeof(buf));
    if (len == 0) throw n_u::IOTimeoutException(getName(),"read");

    // all responses have a SOH, length, packet-type,at least one byte of data,
    // the CRC, and finally EOT.  Therefore they should be at least 6 bytes.
    if (len < 6) throw n_u::IOException(getName(),"read","short message");

    if (buf[0] != SOH) throw n_u::IOException(getName(),"read","no SOH");
    if (buf[len-1] != EOT) throw n_u::IOException(getName(),"read","no EOT");

    // unfix string without SOH,EOT
    string rstring = unfix(string(buf+1,len-2));

    len = rstring.length();
    int plen = rstring[0];
    unsigned char rcrc = (unsigned) rstring[len-1];

    // remove length and CRC.
    len -= 2;
    rstring = rstring.substr(1,len);

#ifdef DEBUG
    cerr << "recv=" << hex;
    for (unsigned int i = 0; i < len; i++)
        cerr << (int)(unsigned char) rstring[i] << ' ';
    cerr << dec << endl;
#endif

    // check length
    if (plen != (signed)len) {
	cerr << "plen=" << plen << " len=" << len << endl;
	throw n_u::IOException(getName(),"read","bad length");
    }

    // compute CRC from what's remaining.
    unsigned char ccrc = crc(rstring);
    if (ccrc != rcrc) {
	cerr << "received crc=" << hex << (int) rcrc <<
	    " calculated=" << (int) ccrc << dec << endl;
	throw n_u::IOException(getName(),"read","bad CRC");
    }

    return rstring;
}

size_t SE_GOESXmtr::read(void* buf, size_t len) throw(n_u::IOException)
{
    // cerr << "readUntil, len=" << len << endl;
    len = port.readUntil((char*)buf,len,EOT);

#ifdef DEBUG
    char* cbuf = (char*) buf;
    cerr << "read=" << hex;
    for (unsigned int i = 0; i < len; i++)
	cerr << (int)(unsigned char) cbuf[i] << ' ';
    cerr << dec << endl;
#endif

    return len;
}

size_t SE_GOESXmtr::write(const void* buf, size_t len) throw(n_u::IOException)
{

#ifdef DEBUG
    const char* cbuf = (const char*) buf;
    cerr << "write=" << hex;
    for (unsigned int i = 0; i < len; i++)
	cerr << (int)(unsigned char) cbuf[i] << ' ';
    cerr << dec << endl;
#endif

    for (size_t left = len; left > 0; ) {
        size_t l = port.write(buf,len);
	buf = (const char*) buf + l;
	left -= l;
    }
    return len;
}


// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#include "CSAT3_Sonic.h"

#include <nidas/core/DSMConfig.h>
#include <nidas/core/Variable.h>
#include <nidas/core/PhysConstants.h>
#include <nidas/core/Project.h>
#include <nidas/core/TimetagAdjuster.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>
#include <nidas/util/IOTimeoutException.h>

#include <math.h>

#include <string>
#include <sstream>
#include <fstream>
#include <boost/regex.hpp>

using namespace nidas::dynld::isff;
using namespace std;
using namespace boost;

namespace n_u = nidas::util;
namespace n_c = nidas::core;

NIDAS_CREATOR_FUNCTION_NS(isff,CSAT3_Sonic)

/**
 * AUTOCONFIG Stuff
 */

const PortConfig CSAT3_Sonic::DEFAULT_PORT_CONFIG(DEFAULT_BAUD_RATE, DEFAULT_DATA_BITS, DEFAULT_PARITY,
												  DEFAULT_STOP_BITS, DEFAULT_PORT_TYPE, DEFAULT_LINE_TERMINATION,
											      DEFAULT_RTS485, DEFAULT_CONFIG_APPLIED);
const char* CSAT3_Sonic::DEFAULT_MSG_SEP_CHARS = "\x55\xaa";
const int CSAT3_Sonic::SENSOR_BAUDS[CSAT3_Sonic::NUM_SENSOR_BAUDS] = {9600, 19200};
const WordSpec CSAT3_Sonic::SENSOR_WORD_SPECS[CSAT3_Sonic::NUM_WORD_SPECS] =
{
	WordSpec(8,Termios::NONE,1),
};

const PORT_TYPES CSAT3_Sonic::SENSOR_PORT_TYPES[NUM_PORT_TYPES] = {RS232};

/*
 *  AutoConfig stuff
 */

/*  Typical ?? response
 *
 *  v3.0-v3.9
 *  ET= 10 ts=i XD=d GN=111a TK=1 UP=5 FK=0 RN=1 IT=1 DR=102 rx=2 fx=038 BX=0
 *  AH=1 AT=0 RS=0 BR=0 RI=0 GO=00000 HA=0 6X=3 3X=2 PD=2 SD=0 ?d sa=1 WM=o
 *  ar=0 ZZ=0 DC=6 ELo=021 021 021 ELb=021 021 021 TNo=dbb d TNb=ccc JD= 007
 *  C0o=-2-2-2 C0b=-2-2-2 RC=0 tlo=9 9 9 tlb=9 9 9 DTR=01740 CA=0 TD=
 *  duty=026 AQ= 10 AC=1 CD=0 SR=1 UX=0 MX=0 DTU=02320 DTC=01160 RD=o ss=1
 *  XP=2 RF=018 DS=007 SN0315 06aug01 HF=005 JC=3 CB=3 MD=5 DF=05000 RNA=1 rev
 *  3.0a cs=22486 &=0 os=
 *
 *  v4.0+
 *  (from device)
 *  SN1121 26may16 rev 5.0f &=1 AA=040 AC=1 AF=040 AH=1 ao=00300 ar=0 AS=-005\r\n
 *   AQ= 20 BR=0 BX=0 c0o= 0 0 0 c0b= 0 0 0 CA=1 CD=0 cf=1 cs=46886 CX=d DC=8\r\n
 *   dl=015 dm=c DR=03465 duty=053 DT=16240 et= 20 fa=00050 FD=  02880 FL=007\r\n
 *   fx=038 GN=848a go=00000 ha=0 hg=01560 HH=02700 kt=0 lg=00832 LH=00100 MA=-020\r\n
 *   MS=-010 mx=0 N5=0 ND=1 NI=2 OC=0 or=1 os=0 pd=2 ra=00020 RC=0 rf=00900 rh=015\r\n
 *   RI=1 RS=1 rx=002 SD=0 SL=035 sr=1 ss=1 t0123=1000 TD=a TF=02600 02600 02600\r\n
 *   TK=1 TO= 0 0 0 tp=t ts=i UF=0 ux=0 WM=o WR=006 WT=06000 XD=d xp=2 xx=00875 ZZ=0\r\n
 *   5T= 1.0000e+01 5k= 5.0000e-03\r\n
 *
 *   (from doc)
 *   SN0315 02mar04 rev 4.0s &=0 AC=1 AF=050 AH=1 AO=00300 ar=0 AQ= 20 BR=0 BX=0
 *    CF=1 C0o= 0 0 0 C0b= 0 0 0 CA=1 CD=0 cs=25417 DC=8 dl=015 DM=c DR=03465
 *    duty=048 DT=16240 ET= 20 FA=00050 FL=007 FX=038 GN=121a GO=00000 HA=0
 *    HG=01560 HH=02700 KT=0 LG=00832 LH=00100 MA=-020 MS=-010 MX=0 ND=1 NI=2
 *    ns=00223 OR=1 os=0 PD=2 RA=00020 RC=0 RF=00900 RH=015 RI=0 RS=0 RX=002
 *    SD=0 SL=035 SR=1 ss=1 T0123=1000 TD=a TF=02600 02600 02600 TK=1 TO= 0 0 0
 *    TP=t ts=i UX=0 WM=o WR=006 XD=d xp=2 XX=00875 ZZ=0.
 *
 */

//regex V4_PLUS_CONFIG_RESPONSE(
//      "(?<serno>SN[[:digit:]]+) (?<caldate>[[:digit:]]{2}[[:lower:]]{3}[[:digit:]]{2}) "
//      "rev (?<fwrev>[[:digit:]]+\\.[[:digit:]]+[[:lower:]]) &=[[:digit:]] (AA=[[:digit:]]+ )*"
//      "AC=[[:digit:]] AF=[[:digit:]]+ AH=[[:digit:]] (ao|AO)=[[:digit:]]+ ar=[[:digit:]] "
//      "(AS=-*[[:digit:]]+)*[[:space:]]+"
//
//      "AQ= (?<acqrate>[[:digit:]]{1,2}) BR=(?<baud>0|1) BX=[[:digit:]][[:space:]]+"
//      "(CF=[[:digit:]] )*c0o= [[:digit:]]+ [[:digit:]]+ [[:digit:]]+ c0b= [[:digit:]]+ [[:digit:]]+ [[:digit:]]+ "
//      "CA=[[:digit:]] CD=[[:digit:]] (cf=[[:digit:]] )*cs=[[:digit:]]+ (CX=. )*DC=[[:digit:]]"
//      "[[:space:]]+"
//
//      "dl=[[:digit:]]+ (dm|dM==DM)=. DR=[[:digit:]]+[[:space:]]+duty=[[:digit:]]+ DT=[[:digit:]]+ "
//      "(et|ET)= [[:digit:]]+ fa=[[:digit:]]+ (FD=  [[:digit:]]+ )*FL=[[:digit:]]+[[:space:]]+"
//
//      "(fx|FX)=[[:digit:]]+ GN=[[:digit:]]+[[:lower:]] (go|GO)=[[:digit:]]+ (ha|HA)=[[:digit:]][[:space:]]+"
//      "(hg|HG)=[[:digit:]]+ HH=[[:digit:]]+ (kt|KT)=[[:digit:]] (lg|LG)=[[:digit:]]+ LH=[[:digit:]]+ "
//      "MA=-*[[:digit:]]+[[:space:]]"
////START HERE
//      "MS=-*[[:digit:]]+ (mx|MX)=[[:digit:]] N5=[[:digit:]] ND=[[:digit:]] NI=[[:digit:]][[:space:]]+"
//      "OC=[[:digit:]] or=[[:digit:]] os=[[:digit:]] pd=[[:digit:]] ra=[[:digit:]]+ RC=[[:digit:]]"
//      "rf=[[:digit:]]+ rh=[[:digit:]]+[[:space:]]"
//
//      "RI=(?<rtsindep>[[:digit:]]) RS=(?<msgsep>[[:digit:]]) rx=[[:digit:]]+ SD=[[:digit:]] SL=[[:digit:]]+ sr=[[:digit:]] "
//      "ss=[[:digit:]]+ t0123=[[:digit:]]+ TD=[[:lower:]] TF=[[:digit:]]+ [[:digit:]]+ [[:digit:]]+[[:space:]]+"
//
//      "TK=[[:digit:]] TO= [[:digit:]]+ [[:digit:]]+ [[:digit:]]+ tp=[[:lower:]] ts=[[:lower:]] "
//      "UF=[[:digit:]] ux=[[:digit:]] WM=[[:lower:]] WR=[[:digit:]]+ WT=[[:digit:]]+ XD=[[:lower:]] "
//      "xp=[[:digit:]] xx=[[:digit:]]+ ZZ=[[:digit:]]"
//      );

std::string DATA_RATE_CFG_DESC("Data Rate");
std::string OVERSAMPLE_CFG_DESC("Over Sampling");
std::string RTS_INDEP_CFG_DESC("RTSIdep");
std::string BAUD_RATE_CFG_DESC("Baud");


/*
 * CSAT3 stuff
 */
const float CSAT3_Sonic::GAMMA_R = 402.684;

CSAT3_Sonic::CSAT3_Sonic():
    Wind3D(DEFAULT_PORT_CONFIG),
    _windInLen(12),	// two bytes each for u,v,w,tc,diag, and 0x55aa
    _totalInLen(12),
    _windNumOut(0),
    _spikeIndex(-1),
    _windSampleId(0),
    _extraSampleTags(),
    _nttsave(-2),
    _counter(-1),
#if __BYTE_ORDER == __BIG_ENDIAN
    _swapBuf(),
#endif
    _rate(0),
    _oversample(false),
    _sonicLogFile(),
    _gapDtUsecs(0),
    _ttlast(0),
    _nanIfDiag(true),
    _consecutiveOpenFailures(0),
    _checkConfiguration(true),
    _checkCounter(true),
    _ttadjuster(0),
    defaultMessageConfig(DEFAULT_MESSAGE_LENGTH, DEFAULT_MSG_SEP_CHARS, DEFAULT_MSG_SEP_EOM),
    rateCmd(""),
    acqrate(0),
    serialNumber(""),
    osc(0),
    revision(""),
    rtsIndep(-1),
    recSep(-1),
    baudRate(-1)
{
    // We set the defaults at construction,
    // letting the base class modify according to fromDOMElement()
    setMessageParameters(defaultMessageConfig);

	// copy the serial comm parameters to the SerialSensor lists
    // Let the base class know about PTB210 RS232 limitations
    for (int i=0; i<NUM_PORT_TYPES; ++i) {
    	_portTypeList.push_back(SENSOR_PORT_TYPES[i]);
    }

    for (int i=0; i<NUM_SENSOR_BAUDS; ++i) {
    	_baudRateList.push_back(SENSOR_BAUDS[i]);
    }

    for (int i=0; i<NUM_WORD_SPECS; ++i) {
    	_serialWordSpecList.push_back(SENSOR_WORD_SPECS[i]);
    }

    initCustomMetaData();
    setAutoConfigSupported();
}

CSAT3_Sonic::~CSAT3_Sonic()
{
    delete _ttadjuster;
}


/* static */
string CSAT3_Sonic::parseSerialNumber(const string& str,
        string::size_type & idx)
{
    // Version 3 and 4 serial numbers: "SNXXXX", version 5: "SnXXXX".
    // Serial number of a special test version 4 sonic was "PR0001"
    const char* sn[] = {
        "SN", "Sn", "PR"
    };

    idx = string::npos;
    for (unsigned int i = 0; idx == string::npos &&
            i < sizeof(sn)/sizeof(sn[0]); i++)
        idx = str.find(sn[i]);

    string empty;
    if (idx == string::npos) return empty;

    string::size_type bl = str.find(' ',idx);
    if (bl != string::npos) return str.substr(idx,bl-idx);
    return empty;
}

string CSAT3_Sonic::querySonic(int &acqrate,char &osc, string& serialNumber,
        string& revision, int& rtsIndep, int& recSep, int& baudRate)
throw(n_u::IOException)
{
    std::string result = "";
    acqrate = 0;
    osc = ' ';
    serialNumber = "";
    revision = "unknown";
    rtsIndep = -1;  // ri setting, RTS independent, -1=unknown
    recSep = -1;  // rs setting, record separator, -1=unknown

    /*
     *  Assumption is that with autoconfig, the system is already in terminal mode
     *  when this function is called and only ASCII is coming back.
     */

    findConfigPrompt(false, true);

    DLOG(("%s:%s sending ?? CR",getName().c_str(), getClassName().c_str()));
    std::string qryCmd("??\r");    // must send CR
    writePause(qryCmd.c_str(), qryCmd.length());

    // sonic takes a while to respond to ??
    string::size_type stidx = string::npos;

    const int BUF_SIZE = 600;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    int l = readEntireResponse(buf, BUF_SIZE-1, MSECS_PER_SEC, true);
    // strings will not be null terminated
    const char * cp = (const char*)buf;
    // sonic echoes back "T" or "??" command
    if (l > 0) {
        while (*cp == 'T' || *cp == '?' || ::isspace(*cp)) {
            cp++;
            l--;
        }
        string rec(cp,l);
        DLOG(("%s: CSAT3 query: len=%i, \"%s\"",getName().c_str(), rec.length(), rec.c_str()));

        result = rec;
    }
    else {
        return result;
    }

    // rev 3 message, starts with ET=, contains serial number
    // rev 4 or 5 message, starts with serial number
//    if (stidx == string::npos) {
//        string::size_type etidx = result.find("ET=");
        string::size_type snidx;
        serialNumber = parseSerialNumber(result, snidx);
//        stidx = std::min(snidx, etidx);
//
//        if (stidx != string::npos) {
//            result = result.substr(stidx);
//            stidx = 0;
//        }
//    }
//    if (stidx != string::npos) {
//        string::size_type ri = result.find("\n>");
//        if (ri != string::npos) {
//            result.resize(ri);
//        }
//    }

//    clearBuffer();

//    if (result.empty()) return result;

    string::size_type rlen = result.length();
    DLOG(("%s: query=",getName().c_str()) << n_u::addBackslashSequences(result) << " result length=" << rlen);

    // find and get AQ parameter, e.g. AQ=1.0 (raw sampling rate)
    string::size_type fs = result.find("AQ= ");
    if (fs != string::npos && fs + 4 < rlen) {
        acqrate = atoi(result.substr(fs+4, 2).c_str());
    }
    else {
        DLOG(("CSAT3_Sonic::querySensor(): Didn't find AQ parameter in query response."));
        return std::string();
    }

    // get os parameter, e.g. "os=g"
    // g for 10Hz 6x oversampling
    // h for 20Hz 3x oversampling
    // ' ' otherwise
    // For version 4, os=0 means no oversampling
    fs = result.find("os=");
    if (fs != string::npos && fs + 3 < rlen) {
        osc = result[fs+3];
    }
    else {
        DLOG(("CSAT3_Sonic::querySensor(): Didn't find os parameter in query response."));
        return std::string();
    }

    // get software revision, e.g. "rev 3.0f"
    fs = result.find("rev ");
    if (fs != string::npos && fs + 4 < rlen) {
        string::size_type bl = result.find(' ',fs+4);
        revision = result.substr(fs+4,bl-fs-4);
    }
    else {
        DLOG(("CSAT3_Sonic::querySensor(): Didn't find rev parameter in query response."));
        return std::string();
    }

    // get RI=n setting. 0=power RS-232 drivers on RTS, 1=power always
    fs = result.find("RI=");
    if (fs != string::npos && fs + 3 < rlen) {
        rtsIndep = atoi(result.substr(fs+3, 1).c_str());
    }
    else {
        DLOG(("CSAT3_Sonic::querySensor(): Didn't find RI parameter in query response."));
        return std::string();
    }

    // get RS=n setting. 0=no record separator, 1=0x55AA
    fs = result.find("RS=");
    if (fs != string::npos && fs + 3 < rlen) {
        recSep = atoi(result.substr(fs+3, 1).c_str());
    }
    else {
        DLOG(("CSAT3_Sonic::querySensor(): Didn't find RS parameter in query response."));
        return std::string();
    }

    // get BR=n setting. 0=9600, 1=19200
    fs = result.find("BR=");
    if (fs != string::npos && fs + 3 < rlen) {

        baudRate = atoi(result.substr(fs+3, 1).c_str());
    }
    else {
        DLOG(("CSAT3_Sonic::querySensor(): Didn't find BR parameter in query response."));
        return std::string();
    }

    return result;
}

void CSAT3_Sonic::sendBaudCmd(int baud)
{
	std::ostringstream baudCmd;
	baudCmd << "br "<< baud << "\r";
    DLOG(("%s: sending %s", getName().c_str(), baudCmd.str().c_str()));
    writePause(baudCmd.str().c_str(), baudCmd.str().length());
}

void CSAT3_Sonic::sendRTSIndepCmd(bool on)
{
	std::ostringstream rtsIndepCmd;
	rtsIndepCmd << "ri " << (on ? '1' : '0') << "\r";
    DLOG(("%s: sending %s", getName().c_str(), rtsIndepCmd.str().c_str()));
    writePause(rtsIndepCmd.str().c_str(), rtsIndepCmd.str().length());
}

void CSAT3_Sonic::sendRecSepCmd()
{
    std::string cmd("rs 1\r");
    DLOG(("%s: sending %s", getName().c_str(), cmd.c_str()));
    writePause(cmd.c_str(), cmd.length());
}

void CSAT3_Sonic::sendRateCommand(const std::string cmd)
throw(n_u::IOException)
{
    DLOG(("%s: sending %s",getName().c_str(),cmd.c_str()));
    writePause(cmd.c_str(), cmd.length());
}

void CSAT3_Sonic::checkSerPortSettings(bool drain)
{
    const int BUF_SIZE = 100;
    char buf[BUF_SIZE];
    int numCharsRead = 0;

    if (drain) {
        VLOG(("CSAT3_Sonic::checkSerPortSettings(): Drain requested..."));
        if (findConfigPrompt(drain, drain ? true : false)) {
            VLOG(("CSAT3_Sonic::checkSerPortSettings(): Found the config prompt: '>'"));
        }
        else {
            VLOG(("CSAT3_Sonic::checkSerPortSettings(): Config prompt, '>', not found..."));
            VLOG(("CSAT3_Sonic::checkSerPortSettings(): Don't bother looking at RI/BR settings..."));
            return;
        }
    }

    std::string serPortSettingsTestCmd("ri\r");
    DLOG(("%s: sending %s",getName().c_str(),serPortSettingsTestCmd.c_str()));
    writePause(serPortSettingsTestCmd.c_str(), serPortSettingsTestCmd.length());

    memset(buf, 0, BUF_SIZE);
    numCharsRead = readEntireResponse((void*)&buf[0], BUF_SIZE-1, MSECS_PER_SEC);
    DLOG(("CSAT3_Sonic::checkSerPortSettings(): ri:") << std::string(buf, numCharsRead));

    serPortSettingsTestCmd.assign("br\r");
    DLOG(("%s: sending %s",getName().c_str(),serPortSettingsTestCmd.c_str()));
    writePause(serPortSettingsTestCmd.c_str(), serPortSettingsTestCmd.length());

    memset(buf, 0, BUF_SIZE);
    numCharsRead = readEntireResponse((void*)&buf[0], BUF_SIZE-1, MSECS_PER_SEC);
    DLOG(("CSAT3_Sonic::checkSerPortSettings(): br: ") << std::string(buf, numCharsRead));
}

const std::string& CSAT3_Sonic::getRateCommand(int rate,bool oversample) const
{
    struct acqSigTable {
        int rate;
        bool oversample;
        std::string cmd;
    };
    static const struct acqSigTable acqSigCmds[] = {
        {1,false,"A2"},
        {2,false,"A5"},
        {3,false,"A6"},
        {5,false,"A7"},
        {6,false,"A8"},
        {10,false,"A9"},
        {12,false,"Aa"},
        {15,false,"Ab"},
        {20,false,"Ac"},
        {30,false,"Ad"},
        {60,false,"Ae"},
        {10,true,"Ag"},
        {20,true,"Ah"},
    };

    static std::string retval("");

    int nr =  (signed)(sizeof(acqSigCmds)/sizeof(acqSigCmds[0]));
    for (int i = 0; i < nr; i++) {
        if (acqSigCmds[i].rate == rate &&
                (oversample == acqSigCmds[i].oversample)) {
            retval.assign(acqSigCmds[i].cmd);
            break;
        }
    }

    return retval;
}

void CSAT3_Sonic::fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException)
{
    NLOG(("CSAT3_Sonic::fromDOMElement(): Checking for sensor customizations in the DSM/Sensor Catalog XML..."));

    // Let the base classes have first shot at it.
    SerialSensor::fromDOMElement(node);

    // Handle common autoconfig attributes first...
    fromDOMElementAutoConfig(node);

/*
 *  Nothing to do at this time...
 */
//    xercesc::DOMNode* child;
//    for (child = node->getFirstChild(); child != 0;
//        child=child->getNextSibling())
//    {
//        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
//            continue;
//        XDOMElement xchild((xercesc::DOMElement*) child);
//        const string& elname = xchild.getNodeName();
//
//        if (elname == "autoconfig") {
//            DLOG(("Found the <autoconfig /> tag..."));
//
//            // get all the attributes of the node
//            xercesc::DOMNamedNodeMap *pAttributes = child->getAttributes();
//            int nSize = pAttributes->getLength();
//
//            for(int i=0; i<nSize; ++i) {
//                XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
//                // get attribute name
//                const std::string& aname = attr.getName();
//                const std::string& aval = attr.getValue();
//
//                // xform everything to uppercase - this shouldn't affect numbers
//                string upperAval = aval;
//                std::transform(upperAval.begin(), upperAval.end(), upperAval.begin(), ::toupper);
//                 DLOG(("CSAT3_Sonic::fromDOMElement(): attribute: ") << aname << " : " << upperAval);
//
//                // start with science parameters, assuming SerialSensor took care of any overrides to
//                // the default port config.
//                if (aname == "units") {
//                }
//            }
//        }
//        else if (elname == "message");
//        else if (elname == "prompt");
//        else if (elname == "sample");
//        else if (elname == "parameter");
//        else if (elname == "calfile");
//        else
//            throw n_u::InvalidParameterException(
//                    string("SerialSensor:") + getName(), "unknown element",
//                    elname);
//    //}

    NLOG(("CSAT3_Sonic::fromDOMElement() - exit"));
}

void CSAT3_Sonic::open(int flags) throw(n_u::IOException,n_u::InvalidParameterException)
{
	// Gotta set up the rate command before it's used.
	// Should have everything needed at this point to configure this.
	// But if being instantiated outside the context of a DOM, this may not be the case.
	if (getCatalogName().length() == 0) {
		DLOG(("Not operating in the context of a DOM, so init _rate"));
		_rate = 20;
	}
	DLOG(("%s: _rate=%d",getName().c_str(),_rate));
	if (_rate > 0) {
		rateCmd = getRateCommand(_rate,_oversample);
		if (rateCmd == "") {
			ostringstream ost;
			ost << "rate=" << _rate << " Hz not supported with oversample=" << _oversample;
			throw n_u::InvalidParameterException(getName(),
					"sample rate",ost.str());
		}
	}

	// AutoConfig happens here...
	SerialSensor::open(flags);

	if (!_checkConfiguration) return;

//	const int NOPEN_TRY = 5;
//
	/*
	 * Typical session when a sonic port is opened. If the sonic rate is determined
	 * to be correct after the first query, then the "change rate" and "second query"
	 * steps are not done.
	 * --------------------------------------------------
	 * action         user         sonic response
	 * --------------------------------------------------
	 * initial                     spewing binary data
	 * terminal mode  "T"          ">"
	 * query          "??\r"       status message "ET=...." followed by ">"
	 * change rate    "Ah"         "Acq sigs 60->20 Type x to abort......< ...>"
	 * second query   "??\r"       status message "ET=...." followed by ">"
	 * data mode      "D"
	 *
	 * If Campbell changes the format of the status message, or any of the other
	 * responses, then this code may have to be modified.
	 */

	/*
	 * Code originally set the message parameters to those already held by the base class. However,
	 * the AutoConfig code sets those values to that expected when entering terminal mode, which is 0 + '>'.
	 * So use the defaultMessageConfig instead to enter data mode.
     * 
     * HUH? TODO - shouldn't the requested parameters take precedence? Save those off before AutoConfig
     *             restore here?
	 */
	try {
		setMessageParameters(defaultMessageConfig);
	}
	catch(const n_u::InvalidParameterException& e) {
		throw n_u::IOException(getName(),"open",e.what());
	}
}

float CSAT3_Sonic::correctTcForPathCurvature(float tc, float, float, float)
{
    // no correction necessary. CSAT outputs speed of sound
    // that is already corrected for path curvature.
    return tc;
}

bool CSAT3_Sonic::process(const Sample* samp, std::list<const Sample*>& results) throw()
{

    std::size_t inlen = samp->getDataByteLength();
    if (inlen < _windInLen) return false;	// not enough data
    if (inlen > _totalInLen + 2) return false;  // exclude wacko records

    const char* dinptr = (const char*) samp->getConstVoidDataPtr();
    // check for correct termination bytes
#ifdef DEBUG
    cerr << "inlen=" << inlen << ' ' << hex << (int)dinptr[inlen-2] <<
        ',' << (int)dinptr[inlen-1] << dec << endl;
#endif

    // Sometimes a serializer is connected to a sonic, in which
    // case it generates records longer than 12 bytes.
    // Check here that the record ends in 0x55 0xaa.
    if (dinptr[inlen-2] != '\x55' || dinptr[inlen-1] != '\xaa') return false;

    if (inlen > _totalInLen) inlen = _totalInLen;

#if __BYTE_ORDER == __BIG_ENDIAN
    /* Swap bytes of input. Campbell output is little endian */
    swab(dinptr,(char *)&_swapBuf.front(),inlen-2);     // dont' swap 0x55 0xaa
    const short* win = &_swapBuf.front();
#else
    const short* win = (const short*) dinptr;
#endif

    dsm_time_t timetag = samp->getTimeTag();

    // Reduce latency jitter in time tags
    if (_ttadjuster) timetag = _ttadjuster->adjust(timetag);

    /*
     * CSAT3 has an internal two sample buffer, so shift
     * wind time tags backwards by two samples.
     */

    /* restart sample time shifting on a data gap */
    if (_gapDtUsecs > 0 && (timetag - _ttlast) > _gapDtUsecs) _nttsave = -2;
    _ttlast = timetag;

    if (_nttsave < 0)
        _timetags[_nttsave++ + 2] = timetag;
    else {
        SampleT<float>* wsamp = getSample<float>(_windNumOut);
        wsamp->setTimeTag(_timetags[_nttsave]);
        wsamp->setId(_windSampleId);

        _timetags[_nttsave] = timetag;
        _nttsave = (_nttsave + 1) % 2;

        float* dout = wsamp->getDataPtr();

        unsigned short diag = (unsigned) win[4];
        int cntr = (diag & 0x003f);

        // special NaN encodings of diagnostic value
        // (F03F=61503 and F000=61440), where diag
        // bits (12 to 15) are set and a counter
        // value of 0 or 63. Range codes all 0.
        if (diag == 61503 || diag == 61440) {
            for (int i = 0; i < 4; i++) 
                dout[i] = floatNAN;
            diag = (diag & 0xf000) >> 12;
        }
        else {
            int range[3];
            range[0] = (diag & 0x0c00) >> 10;
            range[1] = (diag & 0x0300) >> 8;
            range[2] = (diag & 0x00c0) >> 6;
            diag = (diag & 0xf000) >> 12;

            if (diag && _nanIfDiag) {
                for (int i = 0; i < 4; i++) {
                        dout[i] = floatNAN;
                }
            }
            else {
                const float scale[] = {0.002,0.001,0.0005,0.00025};
                int nmissing = 0;
                for (int i = 0; i < 3; i++) {
                    if (win[i] == -32768) {
                        dout[i] = floatNAN;
                        nmissing++;
                    }
                    else {
                        dout[i] = win[i] * scale[range[i]];
                    }
                }

                /*
                 * Documentation also says speed of sound should be a NaN if
                 * ALL the wind components are NaNs.
                 */
                if (nmissing == 3 || win[3] == -32768)
                    dout[3] = floatNAN;
                else {
                    /* convert raw word to speed of sound in m/s */
                    float c = (win[3] * 0.001) + 340.0;
                    /* Convert speed of sound to Tc */
                    dout[3] = (c * c) / GAMMA_R - KELVIN_AT_0C;
                }
            }
        }

        if (_checkCounter) {
            if (_counter >=0 && ((++_counter % 64) != cntr)) diag += 16;
            _counter = cntr;
        }
        dout[4] = diag;

        // logical diagnostic value: set to 0 if all sonic
        // diagnostics are zero, otherwise one.
        if (_ldiagIndex >= 0) dout[_ldiagIndex] = (float)(diag != 0);

        // Note that despiking is optionally done before
        // transducer shadow correction. The idea is that
        // the despiking "should" replace spike values
        // with a value  more close to what should have
        // been actually measured.
        if (_spikeIndex >= 0 || getDespike()) {
            bool spikes[4] = {false,false,false,false};
            despike(wsamp->getTimeTag(),dout,4,spikes);

            if (_spikeIndex >= 0) {
                for (int i = 0; i < 4; i++) 
                    dout[i + _spikeIndex] = (float) spikes[i];
            }
        }

#ifdef HAVE_LIBGSL
        // apply shadow correction before correcting for unusual orientation
        transducerShadowCorrection(wsamp->getTimeTag(),dout);
#endif

        applyOrientation(wsamp->getTimeTag(), dout);

        offsetsTiltAndRotate(wsamp->getTimeTag(), dout);

        if (_spdIndex >= 0)
            dout[_spdIndex] = sqrt(dout[0] * dout[0] + dout[1] * dout[1]);
        if (_dirIndex >= 0) {
            float dr = atan2f(-dout[0],-dout[1]) * 180.0 / M_PI;
            if (dr < 0.0) dr += 360.;
            dout[_dirIndex] = dr;
        }
        results.push_back(wsamp);
    }

    // inlen is now less than or equal to the expected input length
    bool goodterm = dinptr[inlen-2] == '\x55' && dinptr[inlen-1] == '\xaa';

    for (unsigned int i = 0; i < _extraSampleTags.size(); i++) {

        if (inlen < _windInLen + (i+1) * 2) break;

        SampleTag* stag = _extraSampleTags[i];
        const vector<Variable*>& vars = stag->getVariables();
        std::size_t nvars = vars.size();
        SampleT<float>* hsamp = getSample<float>(nvars);
        hsamp->setTimeTag(timetag);
        hsamp->setId(stag->getId());

        unsigned short counts = ((const unsigned short*) win)[i+5];
        float volts;
        if (counts < 65500 && goodterm) volts = counts * 0.0001;
        else volts = floatNAN;

        for (unsigned int j = 0; j < nvars; j++) {
            Variable* var = vars[j];
            VariableConverter* conv = var->getConverter();
            if (!conv) hsamp->getDataPtr()[j] = volts;
            else hsamp->getDataPtr()[j] =
                conv->convert(hsamp->getTimeTag(),volts);
        }
        results.push_back(hsamp);
    }
    return true;
}

void CSAT3_Sonic::parseParameters() throw(n_u::InvalidParameterException)
{

    Wind3D::parseParameters();

    const list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi = params.begin();

    for ( ; pi != params.end(); ++pi) {
        const Parameter* parameter = *pi;

        if (parameter->getName() == "oversample") {
            if (parameter->getType() != Parameter::BOOL_PARAM ||
                    parameter->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),
                        parameter->getName(),
                        "must be boolean true or false");
            _oversample = (int)parameter->getNumericValue(0);
        }
        else if (parameter->getName() == "soniclog") {
            if (parameter->getType() != Parameter::STRING_PARAM ||
                    parameter->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),
                        parameter->getName(),
                        "must be a string");
            _sonicLogFile = parameter->getStringValue(0);
        }
        else if (parameter->getName() == "configure") {
            if (parameter->getType() != Parameter::BOOL_PARAM ||
                    parameter->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),
                        parameter->getName(),
                        "must be boolean true or false");
            _checkConfiguration = (int)parameter->getNumericValue(0);
        }
        else if (parameter->getName() == "checkCounter") {
            if (parameter->getType() != Parameter::BOOL_PARAM ||
                    parameter->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),
                        parameter->getName(),
                        "must be boolean true or false");
            _checkCounter = (int)parameter->getNumericValue(0);
        }
    }

#ifdef HAVE_LIBGSL
    if (_shadowFactor != 0.0 && !_atCalFile) 
            throw n_u::InvalidParameterException(getName(),
                "shadowFactor","transducer shadowFactor is non-zero, but no abc2uvw cal file is specified");
#endif
}

void CSAT3_Sonic::checkSampleTags() throw(n_u::InvalidParameterException)
{

    Wind3D::checkSampleTags();

    list<SampleTag*>& tags= getSampleTags();

    if (tags.size() > 2 || tags.size() < 1)
        throw n_u::InvalidParameterException(getName() +
                " can only create two samples (wind and extra)");

    std::list<SampleTag*>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        SampleTag* stag = *si;
        /*
         * nvars
         * 5	u,v,w,tc,diag
         * 6	u,v,w,tc,diag,ldiag
         * 7	u,v,w,tc,diag,spd,dir
         * 8	u,v,w,tc,diag,ldiag,spd,dir
         * 9	u,v,w,tc,diag,uflag,vflag,wflag,tcflag
         * 11	u,v,w,tc,diag,spd,dir,uflag,vflag,wflag,tcflag
         */
        if (_windSampleId == 0) {
            std::size_t nvars = stag->getVariables().size();
            _rate = (int)rint(stag->getRate());
            if (!_ttadjuster && _rate > 0.0 && stag->getTimetagAdjustPeriod() > 0.0) 
                _ttadjuster = new TimetagAdjuster(_rate,
                        stag->getTimetagAdjustPeriod(),
                        stag->getTimetagAdjustSampleGap());
            _gapDtUsecs = 5 * USECS_PER_SEC;

            _windSampleId = stag->getId();
            _windNumOut = nvars;
            switch(nvars) {
            case 5:
            case 6:
            case 9:
                if (nvars == 9) _spikeIndex = 5;
                if (nvars == 6) _ldiagIndex = 5;
                break;
            case 11:
            case 7:
            case 8:
                if (nvars == 8) _ldiagIndex = 5;
                if (nvars == 11) _spikeIndex = 7;
                if (_spdIndex < 0 || _dirIndex < 0)
                    throw n_u::InvalidParameterException(getName() +
                            " CSAT3 cannot find speed or direction variables");
                break;
            default:
                throw n_u::InvalidParameterException(getName() +
                        " unsupported number of variables. Must be: u,v,w,tc,diag,[spd,dir][4xflags]]");
            }
        }
        else {
            _extraSampleTags.push_back(stag);
            _totalInLen += 2;	// 2 bytes for each additional input
        }
    }
#if __BYTE_ORDER == __BIG_ENDIAN
    _swapBuf.resize(_totalInLen/2);
#endif
}

bool CSAT3_Sonic::findConfigPrompt(bool drain, bool prompt)
{
    bool retval = false;
    unsigned int l = 0;
    const int BUF_SIZE = 50;
    char buf[BUF_SIZE];
    VLOG(("CSAT3_Sonic::findConfigPrompt(): ") << (drain ? "Draining" : "NOT draining"));
    VLOG(("CSAT3_Sonic::findConfigPrompt(): ") << (prompt ? "Prompting" : "NOT prompting"));
    do {
        if (prompt) {
            writePause("\r\r", 2);
        }
        memset(buf, 0, BUF_SIZE);
        l = readEntireResponse(buf, BUF_SIZE-1, MSECS_PER_SEC);
        if (l > 0 && strstr(buf, ">") != 0) {
            VLOG(("CSAT3_Sonic::findConfigPrompt(): Found config prompt: '>'"));
            retval = true;
            break;
        }
        else {
            if (l > 0) {
                LogContext ctx(LOG_VERBOSE);
                if (ctx.active()) {
                    printResponseHex(l, buf);
                }
            }
        }
    } while (l != 0 && drain);

    if (!retval) {
        VLOG(("CSAT3_Sonic::findConfigPrompt(): Config prompt, '>', not found"));
    }

    return retval;
}

n_c::CFG_MODE_STATUS CSAT3_Sonic::enterConfigMode() throw(n_u::IOException)
{
    n_c::CFG_MODE_STATUS cfgStatus = getConfigMode();

    for (int i = 0; i < 3 && cfgStatus != ENTERED; i++) {
        // First just see if we're already in terminal mode...
        if (findConfigPrompt(false, true)) {
            cfgStatus = ENTERED;
            setConfigMode(cfgStatus);
            ILOG(("%s:%s Successfully entered config mode!",
                  getName().c_str(), getClassName().c_str()));
            break;
        }
        else {
            // No, so send the T command and then check.
            DLOG(("%s: sending (P)T\\r",getName().c_str()));
            /*
             * P means print status and turn off internal triggering.
             * If rev 5 sonics are set to 60 Hz raw sampling or
             * 3x or 6x oversampling, they don't respond until sent a P
             */
            /*if (i > 1) writePause("PT",2);
            else*/ writePause("T",1);
            /*
             *  DO NOT use drain in findConfigPrompt(),
             *  in case sensor is in data collection mode, which is binary.
             */
            if (findConfigPrompt()) {
                cfgStatus = ENTERED;
                setConfigMode(cfgStatus);
                ILOG(("%s:%s Successfully entered config mode!",
                      getName().c_str(), getClassName().c_str()));
                break;
            }
        }
    }

    if (cfgStatus != ENTERED) {
        WLOG(("%s:%s cannot switch CSAT3 to terminal mode",
              getName().c_str(), getClassName().c_str()));
    }

    return cfgStatus;
}

void CSAT3_Sonic::exitConfigMode() throw(n_u::IOException)
{
    clearBuffer();

    DLOG(("%s:%s sending D (nocr)",getName().c_str(), getClassName().c_str()));
    write("D",1);
}

bool CSAT3_Sonic::checkResponse()
{
    acqrate = -1;
    osc = 'Z';
    serialNumber = "";
    revision = "";
    rtsIndep = -1;
    recSep = -1;
    baudRate = -1;

    string query = querySonic(acqrate, osc, serialNumber, revision, rtsIndep, recSep, baudRate);
    if (query.length() == 0) {
    	return false;
    }

    DLOG(("%s: AQ=%d,os=%c,serial number=\"", getName().c_str(), acqrate, osc)
    		<< serialNumber << "\" rev=" << revision << ", rtsIndep(RI)=" << rtsIndep
			<< ", recSep(RS)=" << recSep << ", baud(BR)=" << baudRate);

	if (!serialNumber.empty()) {
		// Is current sonic rate OK?  If requested rate is 0, don't change.
		bool rateOK = _rate == 0;
		if (!_oversample && acqrate == _rate) {
			// osc blank or 0 means no oversampling
			if (osc == ' ' || osc == '0') rateOK = true;
		}
		if (_oversample && acqrate == 60) {
			if (_rate == 10 && osc == 'g') rateOK = true;
			if (_rate == 20 && osc == 'h') rateOK = true;
		}

		string rateResult;
		// set rate if it is different from what is desired, or sonic doesn't respond to first query.
		if (!rateOK) {
			assert(rateCmd != "");
			sendRateCommand(rateCmd);
			sleep(4);
			query = querySonic(acqrate, osc, serialNumber, revision, rtsIndep, recSep, baudRate);
			DLOG(("%s: AQ=%d,os=%c,serial number=",getName().c_str(),acqrate,osc) << serialNumber << " rev=" << revision);
		}

		// On rate or serial number change, log to file.
		if (!serialNumber.empty()
		    && (!rateOK || serialNumber != getSerialNumber())
		    && _sonicLogFile.length() > 0) {
			n_u::UTime now;
			string fname = getDSMConfig()->expandString(_sonicLogFile);
			ofstream fst(fname.c_str(),ios_base::out | ios_base::app);
			fst << "csat3: " << getName() <<
				", id=" << getDSMId() << ',' << getSensorId() <<
				", " << serialNumber << ", " << revision << endl;
			fst << "time: " << now.format(true,"%Y %m %d %H:%M:%S") << endl;
			n_u::trimString(rateResult);
			if (rateResult.length() > 0) fst << rateResult << endl;
			n_u::trimString(query);
			fst << query << endl;
			fst << "##################" << endl;
			if (fst.fail()) PLOG(("%s: write failed",_sonicLogFile.c_str()));
			fst.close();
		}
	}

    return (query.length() != 0);
}

int baudToCSATCmdArg(int baud)
{
		if (baud == 9600) {
			return 0;
		}
		else if (baud == 19200) {
			return 1;
		}
		else {
			std::ostringstream msg("illegal parameter: ");
			msg << baud;
			throw InvalidParameterException("CSAT3_Sonic", "baudToCSATCmdArg()", msg.str());
		}
	return 0;
}

bool CSAT3_Sonic::installDesiredSensorConfig(const PortConfig& rDesiredConfig)
{
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): ") << rDesiredConfig);

    serPortFlush(O_RDWR);

    LogContext logCtx(LOG_VERBOSE);
    if (logCtx.active()) {
        checkSerPortSettings(true);
        serPortFlush(O_RDWR);
    }

    /*
     * First send the commands to change the baud rate
     */
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): turn off RS232 drivers"));
    sendRTSIndepCmd(false); // Turn off RTS Independent before changing the baud rate

    serPortFlush(O_WRONLY);
    findConfigPrompt();

    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): set new baud rate"));
    sendBaudCmd(baudToCSATCmdArg(rDesiredConfig.termios.getBaudRate()));

    serPortFlush(O_WRONLY);
    findConfigPrompt();

    if (logCtx.active()) {
        checkSerPortSettings(true);
        serPortFlush(O_RDWR);
    }

    SerialPortIODevice* pIODevice = reinterpret_cast<SerialPortIODevice*>(getIODevice());
    PortConfig workingPortConfig = pIODevice->getPortConfig();
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): modem lines before force RTS deassert: ")
         << pIODevice->modemFlagsToString(pIODevice->getModemStatus()));
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): force deassert RTS on serial port"));
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): flow control was: ") << workingPortConfig.termios.getFlowControlString());
    bool local = workingPortConfig.termios.getLocal();
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): local was: ") << local);
    workingPortConfig.termios.setFlowControl(Termios::HARDWARE);
    workingPortConfig.termios.setLocal(true);
    setPortConfig(workingPortConfig);
    applyPortConfig();
    serPortFlush(O_RDWR);

    pIODevice->clearModemBits(TIOCM_RTS);
    serPortFlush(O_RDWR);

	VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): modem lines after RTS deassert: ")
	     << pIODevice->modemFlagsToString(pIODevice->getModemStatus()));

	/*
	 * Then change DSM baud rate according to rDesiredConfig
	 */
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): Change DSM port config"));
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): force re-assert RTS on serial port"));
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): flow control was: ") << rDesiredConfig.termios.getFlowControlString());
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): local was: ") << rDesiredConfig.termios.getLocal());
    PortConfig desiredConfig = rDesiredConfig;
    desiredConfig.termios.setFlowControl(Termios::HARDWARE);
    desiredConfig.termios.setLocal(true);
    setPortConfig(desiredConfig);
    applyPortConfig();
    serPortFlush(O_RDWR);

    pIODevice->setModemBits(TIOCM_RTS);
//    pIODevice->setModemBits(TIOCM_DTR);
    serPortFlush(O_RDWR);

    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): Desired Port Config: ") << desiredConfig);

    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): flow control is: ") << getPortConfig().termios.getFlowControlString());
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): local is: ") << getPortConfig().termios.getLocal());
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): modem lines after apply desired config: ")
         << pIODevice->modemFlagsToString(pIODevice->getModemStatus()));

    /*
	 * Now check that we can still communicate
	 */
    VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): make sure we're in config mode..."));
    setConfigMode(NOT_ENTERED);
    serPortFlush(O_RDWR);
    setConfigMode(enterConfigMode());
    if (getConfigMode() == ENTERED) {
        VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): turn RS232 drivers back on"));
        sendRTSIndepCmd();

        VLOG(("CSAT3_Sonic::installDesiredSensorConfig(): double check response"));
        return doubleCheckResponse();
	}

	return false;
}

void CSAT3_Sonic::sendScienceParameters()
{
	sendRTSIndepCmd();
	sendRecSepCmd();
	sendRateCommand(rateCmd);
	// rate command gives user a chance to abort for about 4 seconds
	usleep(5*USECS_PER_SEC);
}

bool CSAT3_Sonic::checkScienceParameters()
{
	return doubleCheckResponse();
}

void CSAT3_Sonic::updateMetaData()
{
    setManufacturer("Campbell Scientific, Inc.");

    /*
     *  All these details should already be available at this point in the process.
     */

    if (!serialNumber.empty()) {
        setSerialNumber(serialNumber);
    }

    if (!revision.empty()) {
        setFwVersion(revision);
    }

    std::ostringstream tmpCfg;
    tmpCfg << osc;
    updateMetaDataItem(MetaDataItem(OVERSAMPLE_CFG_DESC, tmpCfg.str()));

    tmpCfg.str("");
    tmpCfg << acqrate;
    updateMetaDataItem(MetaDataItem(DATA_RATE_CFG_DESC, tmpCfg.str()));

    tmpCfg.str("");
    tmpCfg << rtsIndep;
    updateMetaDataItem(MetaDataItem(RTS_INDEP_CFG_DESC, tmpCfg.str()));

    tmpCfg.str("");
    tmpCfg << baudRate;
    updateMetaDataItem(MetaDataItem(BAUD_RATE_CFG_DESC, tmpCfg.str()));

}


void CSAT3_Sonic::initCustomMetaData()
{
    addMetaDataItem(MetaDataItem(DATA_RATE_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(OVERSAMPLE_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(RTS_INDEP_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(BAUD_RATE_CFG_DESC, ""));
}


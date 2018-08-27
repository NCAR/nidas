// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2014, Copyright University Corporation for Atmospheric Research
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

#include "PTB220.h"
#include <nidas/core/SerialPortIODevice.h>
#include <nidas/util/ParseException.h>

#include <sstream>
#include <limits>
#include <regex.h>

using namespace nidas::core;
using namespace nidas::util;
using namespace std;

NIDAS_CREATOR_FUNCTION_NS(isff,PTB220)

namespace nidas { namespace dynld { namespace isff {

const char* PTB220::DEFAULT_MSG_SEP_CHARS = "\r\n";

const char* PTB220::SENSOR_SEND_MODE_CMD_STR = "SMODE \r\n";
// Requires SW4 set to write enable
//const char* PTB220::SENSOR_MEAS_MODE_CMD_STR = "MMODE \r\n";
//const char* PTB220::SENSOR_LINEAR_CORR_CMD_STR = "LC \r\n";
//const char* PTB220::SENSOR_MULTPT_CORR_CMD_STR = "MPC \r\n";
//const char* PTB220::SENSOR_SET_LINEAR_CORR_CMD_STR = "LCI \r\n";
//const char* PTB220::SENSOR_SET_MULTPT_CORR_CMD_STR = "MPCI \r\n";
//const char* PTB220::SENSOR_SET_CAL_DATE_CMD_STR = "CALD \r\n";
const char* PTB220::SENSOR_SERIAL_BAUD_CMD_STR = "SERI \r\n";
const char* PTB220::SENSOR_SERIAL_PARITY_CMD_STR = "SERI \r\n";
const char* PTB220::SENSOR_SERIAL_DATABITS_CMD_STR = "SERI \r\n";
const char* PTB220::SENSOR_SERIAL_STOPBITS_CMD_STR = "SERI \r\n";
const char* PTB220::SENSOR_ECHO_CMD_STR = "ECHO \r\n";
const char* PTB220::SENSOR_DATA_OUTPUT_FORMAT_CMD_STR = "FORM \r\n";
const char* PTB220::SENSOR_ERROR_OUTPUT_FORMAT_CMD_STR = "EFORM \r\n";
// not managing external display
//const char* PTB220::SENSOR_DISPLAY_OUTPUT_FORMAT_CMD_STR = "DFORM \r\n";
//const char* PTB220::SENSOR_KEYBD_LCK_CMD_STR = "KEYLOCK \r\n";
const char* PTB220::SENSOR_PRESS_UNIT_CMD_STR = "UNIT \r\n";
const char* PTB220::SENSOR_TEMP_UNIT_CMD_STR = "UNIT \r\n";
const char* PTB220::SENSOR_HEIGHT_CORR_CMD_STR = "HHCP \r\n";
const char* PTB220::SENSOR_AVG_TIME_CMD_STR = "AVRG \r\n";
const char* PTB220::SENSOR_OUTPUT_INTERVAL_VAL_CMD_STR = "INTV \r\n";
const char* PTB220::SENSOR_OUTPUT_INTERVAL_UNIT_CMD_STR = "INTV \r\n";
const char* PTB220::SENSOR_ADDR_CMD_STR = "ADDR \r\n";
// Not managing this use case
//const char* PTB220::SENSOR_USR_DEFINED_SEND_CMD_STR = "SCOM \r\n";
const char* PTB220::SENSOR_PRESS_STABILITY_CMD_STR = "PSTAB \r\n";
const char* PTB220::SENSOR_PRESS_LIMIT_ALARMS_CMD_STR = "PLARM \r\n";
// Our units only have one xducer
//const char* PTB220::SENSOR_PRESS_DIFF_LIMIT_CMD_STR = "PDMAX \r\n";
const char* PTB220::SENSOR_RUN_CMD_STR = "R\r\n";
const char* PTB220::SENSOR_STOP_CMD_STR = "S\r\n";
const char* PTB220::SENSOR_STOP_SEND_CMD_STR = "SEND\r\n";
const char* PTB220::SENSOR_POLL_SEND_CMD_STR = "SEND \r\n";
const char* PTB220::SENSOR_VERIF_CMD_STR = "V\r\n";
const char* PTB220::SENSOR_SELF_DIAG_CMD_STR = "DNOS\r\n";
const char* PTB220::SENSOR_PRESS_TRACK_CMD_STR = "TRACK \r\n";
const char* PTB220::SENSOR_PRESS_LIMIT_LIST_CMD_STR = "PLIM\r\n";
const char* PTB220::SENSOR_RESET_CMD_STR = "RESET\r\n";
const char* PTB220::SENSOR_POLL_OPEN_CMD_STR = "OPEN \r\n";
const char* PTB220::SENSOR_POLL_CLOSE_CMD_STR = "CLOSE \r\n";
const char* PTB220::SENSOR_CORR_STATUS_CMD_STR = "CORR \r\n";
const char* PTB220::SENSOR_CONFIG_QRY_CMD_STR = "S\r\n?\r\n";
const char* PTB220::SENSOR_SW_VER_CMD_STR = "VERS\r\n";
const char* PTB220::SENSOR_SER_NUM_CMD_STR = "SNUM_CPU\r\n";
const char* PTB220::SENSOR_ERR_LIST_CMD_STR = "ERRS\r\n";
const char* PTB220::SENSOR_TEST_CMD_STR = "TEST \r\n";
const char* PTB220::SENSOR_XDUCER_COEFF_LIST_CMD_STR = "C \r\n";
// Requires SW3 set
//const char* PTB220::SENSOR_PULSE_MODE_TEST_CMD_STR = "PTEST \r\n";

const char* PTB220::cmdTable[NUM_SENSOR_CMDS] =
{
	SENSOR_SEND_MODE_CMD_STR,
	// Req SW4 set to write enable
	// SENSOR_MEAS_MODE_CMD_STR,
	// SENSOR_LINEAR_CORR_CMD_STR,
	// SENSOR_MULTPT_CORR_CMD_STR,
	// SENSOR_SET_LINEAR_CORR_CMD_STR,
	// SENSOR_SET_MULTPT_CORR_CMD_STR,
	// SENSOR_SET_CAL_DATE_CMD_STR,
	SENSOR_SERIAL_BAUD_CMD_STR,
	SENSOR_SERIAL_PARITY_CMD_STR,
	SENSOR_SERIAL_DATABITS_CMD_STR,
	SENSOR_SERIAL_STOPBITS_CMD_STR,
	SENSOR_ECHO_CMD_STR,
	SENSOR_DATA_OUTPUT_FORMAT_CMD_STR,
	SENSOR_ERROR_OUTPUT_FORMAT_CMD_STR,
	// Not managing external display
	// SENSOR_DISPLAY_OUTPUT_FORMAT_CMD_STR,
	// SENSOR_KEYBD_LCK_CMD_STR,
	SENSOR_PRESS_UNIT_CMD_STR,
	SENSOR_TEMP_UNIT_CMD_STR,
	SENSOR_HEIGHT_CORR_CMD_STR,
	SENSOR_AVG_TIME_CMD_STR,
	SENSOR_OUTPUT_INTERVAL_VAL_CMD_STR,
	SENSOR_OUTPUT_INTERVAL_UNIT_CMD_STR,
	SENSOR_ADDR_CMD_STR,
	// Not managing this use case
	//SENSOR_USR_DEFINED_SEND_CMD_STR,
	SENSOR_PRESS_STABILITY_CMD_STR,
	SENSOR_PRESS_LIMIT_ALARMS_CMD_STR,
	// Only one xducer in our units
	// SENSOR_PRESS_DIFF_LIMIT_CMD_STR,
	SENSOR_RUN_CMD_STR,
	SENSOR_STOP_CMD_STR,
	SENSOR_STOP_SEND_CMD_STR,
	SENSOR_POLL_SEND_CMD_STR,
	SENSOR_VERIF_CMD_STR,
	SENSOR_SELF_DIAG_CMD_STR,
	SENSOR_PRESS_TRACK_CMD_STR,
	SENSOR_PRESS_LIMIT_LIST_CMD_STR,
	SENSOR_RESET_CMD_STR,
	SENSOR_POLL_OPEN_CMD_STR,
	SENSOR_POLL_CLOSE_CMD_STR,
	SENSOR_CORR_STATUS_CMD_STR,
	SENSOR_CONFIG_QRY_CMD_STR,
	SENSOR_SW_VER_CMD_STR,
	SENSOR_SER_NUM_CMD_STR,
	SENSOR_ERR_LIST_CMD_STR,
	SENSOR_TEST_CMD_STR,
	SENSOR_XDUCER_COEFF_LIST_CMD_STR,
	// Requires SW3 set to ON
	// SENSOR_PULSE_MODE_TEST_CMD_STR
};

// NOTE: list sensor bauds from highest to lowest as the higher 
//       ones are the most likely
const int PTB220::SENSOR_BAUDS[NUM_SENSOR_BAUDS] = {9600, 19200, 4800, 2400, 1200};
// list the possible word specifications - most likely first

const WordSpec PTB220::SENSOR_WORD_SPECS[PTB220::NUM_SENSOR_WORD_SPECS] = {
	{7,Termios::EVEN,1},
	{7,Termios::ODD,1},
	{8,Termios::NONE,1},
	{8,Termios::EVEN,1},
	{8,Termios::ODD,1},

	// put 2 stop bits last because these are really rarely used.
	{7,Termios::NONE,2},
	{7,Termios::EVEN,2},
	{7,Termios::ODD,2},
	{8,Termios::NONE,2},
};

const n_c::PORT_TYPES PTB220::SENSOR_PORT_TYPES[PTB220::NUM_PORT_TYPES] = {n_c::RS232, n_c::RS422, n_c::RS485_HALF };

// static default configuration to send to base class...
const PortConfig PTB220::DEFAULT_PORT_CONFIG(PTB220::DEFAULT_BAUD_RATE, PTB220::DEFAULT_DATA_BITS,
                                             PTB220::DEFAULT_PARITY, PTB220::DEFAULT_STOP_BITS,
                                             PTB220::DEFAULT_PORT_TYPE, PTB220::DEFAULT_SENSOR_TERMINATION,
                                             PTB220::DEFAULT_SENSOR_POWER, PTB220::DEFAULT_RTS485,
                                             PTB220::DEFAULT_CONFIG_APPLIED);

const PTB_CMD_ARG PTB220::DEFAULT_SCIENCE_PARAMETERS[] = {
	PTB_CMD_ARG(DEFAULT_PRESS_UNITS_CMD, PTB_ARG(pressTempUnitsArgsToStr(DEFAULT_PRESS_UNITS))),
	PTB_CMD_ARG(DEFAULT_TEMP_UNITS_CMD, PTB_ARG(pressTempUnitsArgsToStr(DEFAULT_TEMP_UNITS))),
	PTB_CMD_ARG(DEFAULT_OUTPUT_RATE_CMD, PTB_ARG(DEFAULT_OUTPUT_RATE)),
	PTB_CMD_ARG(DEFAULT_OUTPUT_RATE_UNITS_CMD, PTB_ARG(DEFAULT_OUTPUT_RATE_UNIT)),
	PTB_CMD_ARG(DEFAULT_SAMPLE_AVERAGING_CMD, PTB_ARG(DEFAULT_AVG_TIME)),
	PTB_CMD_ARG(DEFAULT_OUTPUT_FORMAT_CMD, PTB_ARG(DEFAULT_SENSOR_OUTPUT_FORMAT)),
	PTB_CMD_ARG(DEFAULT_SENSOR_SEND_MODE_CMD, PTB_ARG(DEFAULT_SENSOR_SEND_MODE))
};

const int PTB220::NUM_DEFAULT_SCIENCE_PARAMETERS = sizeof(DEFAULT_SCIENCE_PARAMETERS)/sizeof(PTB_CMD_ARG);

const char* PTB220::DEFAULT_SENSOR_OUTPUT_FORMAT = "\"B\" ADDR " " 4.3 P1 3.1 T1 #r #n";
const char* PTB220::DEFAULT_SENSOR_SEND_MODE = "RUN";
const char* PTB220::DEFAULT_OUTPUT_RATE_UNIT = "s";


/* Typical PTB220 ? query response. All line endings are \r\n
 * 
 * Software version          PTB220 / 1.04
 * Serial number             S0610002
 * Configuration             1
 * Linear adjustments        OFF
 * Multipoint adjustments    ON
 * Calibration date          ????-??-??
 * Baud Parity Data Stop Dpx 9600  N 8 1 F
 * Echo                      ON
 * Sending mode              RUN
 * Measurement mode          NORMAL
 * Pulse mode                OFF  SLOW LOW       0.0
 * Address                   5
 * Output interval           1 s
 * Output format             "B5 " 4.3 P1 " " 3.1 T1 #r #n
 * Error output format
 * SCOM format
 * Pressure unit             hPa
 * Temperature unit          'C
 * Averaging time            1.0 s
 *
 */


// regular expression strings, contexts, compilation
// NOTE: the regular expressions need to search a buffer w/multiple lines separated by \r\n

static const char* PTB220_VER_REGEX_STR =           "Software version[[:blank:]]+PTB220 / ([[:digit:]].[[:digit:]]{1,2})[[:space:]]+";
static const char* PTB220_SERIAL_NUMBER_REGEX_STR = "(.*[[:space:]]+)+Serial number[[:blank:]]+([[:upper:]][[:digit:]]+)[[:space:]]+";
static const char* PTB220_CONFIG_REGEX_STR =        "(.*[[:space:]]+)+Configuration[[:blank:]]+([[:digit:]])[[:space:]]+";
static const char* PTB220_LINEAR_CORR_REGEX_STR =   "(.*[[:space:]]+)+Linear adjustments[[:blank:]]+(ON|OFF){1}[[:space:]]+";
static const char* PTB220_MULTI_PT_CORR_REGEX_STR = "(.*[[:space:]]+)+Multipoint adjustments[[:blank:]]+(ON|OFF)[[:space:]]+";
//static const char* PTB220_CAL_DATE_REGEX_STR =      "(.*[[:space:]]+)+Calibration date[[:blank:]]+([?[:digit:]]{4}(-[?[:digit:]]{2}){2})[[:space:]]+";
static const char* PTB220_SERIAL_CFG_REGEX_STR =    "(.*[[:space:]]+)+Baud Parity Data Stop Dpx[[:blank:]]+([[:digit:]]{4,5})[[:blank:]]+"
														"(N|E|O){1}[[:blank:]]+(7|8){1}[[:blank:]]+(1|2){1}[[:blank:]]+"
														"([[:upper:]])[[:space:]]+";
static const char* PTB220_ECHO_REGEX_STR =       	"(.*[[:space:]]+)+Echo[[:blank:]]+(ON|OFF){1}[[:space:]]+";
static const char* PTB220_SENDING_MODE_REGEX_STR =  "(.*[[:space:]]+)+Sending mode[[:blank:]]+(RUN|POLL){1}( / OPEN)*[[:space:]]+";
static const char* PTB220_PULSE_MODE_REGEX_STR =  	"(.*[[:space:]]+)+Pulse mode[[:blank:]]+(OFF){1}.*[[:space:]]+";
static const char* PTB220_MEAS_MODE_REGEX_STR =  	"(.*[[:space:]]+)+Measurement mode[[:blank:]]+(NORMAL|FAST){1}[[:space:]]+";
static const char* PTB220_ADDRESS_REGEX_STR =       "(.*[[:space:]]+)+Address[[:blank:]]+([[:digit:]]{1,3})[[:space:]]+";
static const char* PTB220_OUTPUT_INTERVAL_REGEX_STR =   "(.*[[:space:]]+)+Output interval[[:blank:]]+([[:digit:]]{1,3}) ((s|min|hr){1})[[:space:]]+";
static const char* PTB220_OUTPUT_RATE_XML_REGEX_STR = "(.*)([[:digit:]]{1,3}) ((s|min|hr){1})";
static const char* PTB220_OUTPUT_FMT_REGEX_STR =	"(.*[[:space:]]+)+Output format[[:blank:]]+([[:alnum:]\"#.])+[[:space:]]+";
static const char* PTB220_ERR_OUT_FMT_REGEX_STR =	"(.*[[:space:]]+)+Error output format[[:blank:]]+([[:alnum:]\"#.])*[[:space:]]+";
static const char* PTB220_USR_OUT_FMT_REGEX_STR =	"(.*[[:space:]]+)+SCOM format[[:blank:]]+([[:alnum:]\"#.])*[[:space:]]+";
static const char* PTB220_PRESS_UNIT_REGEX_STR =    "(.*[[:space:]]+)+Pressure unit[[:blank:]]+([[:alnum:]]{2,5})[[:space:]]+";
static const char* PTB220_TEMP_UNIT_REGEX_STR =		"(.*[[:space:]]+)+Temperature unit[[:blank:]]+\'(C|F){1}[[:space:]]+";
static const char* PTB220_AVG_TIME_REGEX_STR =		"(.*[[:space:]]+)+Averaging time[[:blank:]]+([[:digit:]]{1,3}).0 s[[:space:]]+";

static regex_t version;
static regex_t serNum;
static regex_t baroCfg;
static regex_t linearCorr;
static regex_t multiCorr;
//static regex_t calDate;
static regex_t serialCfg;
static regex_t echo;
static regex_t sendMode;
static regex_t pulseMode;
static regex_t measMode;
static regex_t addr;
static regex_t outInterval;
static regex_t xmlOutRate;
static regex_t outFormat;
static regex_t errOutFormat;
static regex_t usrOutFormat;
static regex_t pressUnit;
static regex_t tempUnit;
static regex_t avgTime;

static bool compileRegex() {
    static bool regexCompiled = false;
    int regStatus = 0;

    if (!regexCompiled) {
        regexCompiled = (regStatus = ::regcomp(&version, PTB220_VER_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus, &version, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("PTB220 version regular expression", string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&serNum, PTB220_SERIAL_NUMBER_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus, &serNum, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("PTB220 serial number regular expression", string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&baroCfg, PTB220_CONFIG_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus, &baroCfg, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("PTB220 barometer cfg regular expression", string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&linearCorr, PTB220_LINEAR_CORR_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus, &linearCorr, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("PTB220 linear correction regular expression", string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&multiCorr, PTB220_MULTI_PT_CORR_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus, &multiCorr, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("PTB220 linear correction regular expression", string(regerrbuf));
        }

//        regexCompiled = (regStatus = ::regcomp(&calDate, PTB220_CAL_DATE_REGEX_STR, REG_EXTENDED)) == 0;
//        if (regStatus) {
//            char regerrbuf[64];
//            regerror(regStatus, &calDate, regerrbuf, sizeof regerrbuf);
//            throw n_u::ParseException("PTB220 cal date regular expression", string(regerrbuf));
//        }

        regexCompiled = (regStatus = ::regcomp(&serialCfg, PTB220_SERIAL_CFG_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&serialCfg,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 serial config regular expression", string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&echo, PTB220_ECHO_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus, &echo, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("PTB220 serial number regular expression", string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&sendMode, PTB220_SENDING_MODE_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&sendMode,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 sending mode regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&pulseMode, PTB220_PULSE_MODE_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&pulseMode,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 pulse mode regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&measMode, PTB220_MEAS_MODE_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&measMode,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 measurement mode regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&addr, PTB220_ADDRESS_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&addr,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 address regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&outInterval, PTB220_OUTPUT_INTERVAL_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&outInterval,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 output rate regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&xmlOutRate, PTB220_OUTPUT_RATE_XML_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&xmlOutRate,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 output rate XML regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&outFormat, PTB220_OUTPUT_FMT_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&outFormat,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 output format regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&errOutFormat, PTB220_ERR_OUT_FMT_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&errOutFormat,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 error output format regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&usrOutFormat, PTB220_USR_OUT_FMT_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&usrOutFormat,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 error output format regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&pressUnit, PTB220_PRESS_UNIT_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&pressUnit,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 pressure unit regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&tempUnit, PTB220_TEMP_UNIT_REGEX_STR, REG_EXTENDED)) == 0;

        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&tempUnit,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 temp unit regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&avgTime, PTB220_AVG_TIME_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&avgTime,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB220 average time regular expression",
                string(regerrbuf));
        }
    }

    return regexCompiled;
}

static void freeRegex() {
    regfree(&version);
    regfree(&serNum);
    regfree(&baroCfg);
    regfree(&linearCorr);
    regfree(&multiCorr);
//    regfree(&calDate);
    regfree(&serialCfg);
    regfree(&echo);
    regfree(&sendMode);
    regfree(&pulseMode);
    regfree(&measMode);
    regfree(&addr);
    regfree(&outInterval);
    regfree(&outFormat);
    regfree(&errOutFormat);
    regfree(&usrOutFormat);
    regfree(&pressUnit);
    regfree(&tempUnit);
    regfree(&avgTime);
}

PTB220::PTB220()
    : SerialSensor(DEFAULT_PORT_CONFIG), testPortConfig(), desiredPortConfig(DEFAULT_PORT_CONFIG),
      defaultMessageConfig(DEFAULT_MESSAGE_LENGTH, DEFAULT_MSG_SEP_CHARS, DEFAULT_MSG_SEP_EOM),
      desiredScienceParameters(), sensorSWVersion(""), sensorSerialNumber("")
{
    // We set the defaults at construction, 
    // letting the base class modify according to fromDOMElement() 
    setMessageParameters(defaultMessageConfig);

    desiredScienceParameters = new PTB_CMD_ARG[NUM_DEFAULT_SCIENCE_PARAMETERS];
    for (int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        desiredScienceParameters[i] = DEFAULT_SCIENCE_PARAMETERS[i];
    }

    compileRegex();
}

PTB220::~PTB220()
{
    freeRegex();
    delete [] desiredScienceParameters;
}

void PTB220::fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException)
{
    // let the base classes have first shot at it, since we only care about an autoconfig child element
    // however, any duplicate items in autoconfig will override any items in the base classes
    SerialSensor::fromDOMElement(node);

    XDOMElement xnode(node);

    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) 
            continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();

        if (elname == "autoconfig") {
            // get all the attributes of the node
            xercesc::DOMNamedNodeMap *pAttributes = child->getAttributes();
            int nSize = pAttributes->getLength();
            
            for(int i=0; i<nSize; ++i) {
                XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
                // get attribute name
                const std::string& aname = attr.getName();
                const std::string& aval = attr.getValue();

                // xform everything to uppercase - this shouldn't affect numbers
                string upperAval = aval;
                std::transform(upperAval.begin(), upperAval.end(), upperAval.begin(), ::toupper);
                DLOG(("PTB220:fromDOMElement(): attribute: ") << aname << " : " << upperAval);

                // start with science parameters, assuming SerialSensor took care of any overrides to 
                // the default port config. May not have happened, so still need to take a look below.
                if (aname == "pressunits") {
                    updateDesiredScienceParameter(SENSOR_PRESS_UNIT_CMD, PTB_ARG(strToPressTempUnits(upperAval)));
                }
                if (aname == "tempunits") {
                    updateDesiredScienceParameter(SENSOR_TEMP_UNIT_CMD, PTB_ARG(strToPressTempUnits(upperAval)));
                }
                else if (aname == "outputformat") {
                	// Just assume they know what they're doin'
                    updateDesiredScienceParameter(SENSOR_DATA_OUTPUT_FORMAT_CMD, PTB_ARG(upperAval));
                }
                else if (aname == "averagetime") {
                    istringstream ist(aval);
                    int val = 0;
					ist >> val;
                    if (ist.fail() || val < SENSOR_AVG_TIME_MIN || val > SENSOR_AVG_TIME_MAX)
                        throw n_u::InvalidParameterException(
                            string("PTB220:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_AVG_TIME_CMD, PTB_ARG(val));
                }
                else if (aname == "outputintvl") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || val < SENSOR_OUTPUT_RATE_MIN || val > SENSOR_OUTPUT_RATE_MAX)
                        throw n_u::InvalidParameterException(
                            string("PTB220:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_OUTPUT_INTERVAL_VAL_CMD, PTB_ARG(val));
                }
                else if (aname == "outputintvlunit") {
                	std::string arg(upperAval);
                    if ( arg != "S" && arg != "MIN" && arg != "H")
                        throw n_u::InvalidParameterException(
                            string("PTB220:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_OUTPUT_INTERVAL_UNIT_CMD, PTB_ARG(arg));
                }
                else if (aname == "address") {
                    istringstream ist(aval);
                    int addr;
                    ist >> addr;
                    if (ist.fail() || addr< SENSOR_ADDR_MIN || addr > SENSOR_ADDR_MAX)
                        throw n_u::InvalidParameterException(
                            string("PTB220:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_ADDR_CMD, PTB_ARG(addr));
                }
                else if (aname == "porttype") {
                    if (upperAval == "RS232") 
                        desiredPortConfig.xcvrConfig.portType = RS232;
                    else if (upperAval == "RS422") 
                        desiredPortConfig.xcvrConfig.portType = RS422;
                    else if (upperAval == "RS485_HALF") 
                        desiredPortConfig.xcvrConfig.portType = RS485_HALF;
                    else if (upperAval == "RS485_FULL") 
                        desiredPortConfig.xcvrConfig.portType = RS485_FULL;
                    else
                        throw n_u::InvalidParameterException(
                            string("PTB220:") + getName(), aname, aval);
                }
                else if (aname == "termination") {
                    if (upperAval == "NO_TERM" || upperAval == "NO" || upperAval == "FALSE") 
                        desiredPortConfig.xcvrConfig.termination = NO_TERM;
                    else if (upperAval == "TERM_120_OHM" || upperAval == "YES" || upperAval == "TRUE") 
                        desiredPortConfig.xcvrConfig.termination = TERM_120_OHM;
                    else
                        throw n_u::InvalidParameterException(
                            string("PTB220:") + getName(), aname, aval);
                }
                else if (aname == "baud") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || !desiredPortConfig.termios.setBaudRate(val))
                        throw n_u::InvalidParameterException(
                            string("PTB220:") + getName(), aname,aval);
                }
                else if (aname == "parity") {
                    if (upperAval == "ODD") 
                        desiredPortConfig.termios.setParity(n_u::Termios::ODD);
                    else if (upperAval == "EVEN") 
                        desiredPortConfig.termios.setParity(n_u::Termios::EVEN);
                    else if (upperAval == "NONE") 
                        desiredPortConfig.termios.setParity(n_u::Termios::NONE);
                    else throw n_u::InvalidParameterException(
                        string("PTB220:") + getName(),
                        aname,aval);
                }
                else if (aname == "databits") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || val < 7 || val > 8)
                        throw n_u::InvalidParameterException(
                        string("PTB220:") + getName(),
                            aname, aval);
                    desiredPortConfig.termios.setDataBits(val);
                }
                else if (aname == "stopbits") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || val < 1 || val > 2)
                        throw n_u::InvalidParameterException(
                        string("PTB220:") + getName(),
                            aname, aval);
                    desiredPortConfig.termios.setStopBits(val);
                }
                else if (aname == "rts485") {
                    if (upperAval == "TRUE" || aval == "1") {
                        desiredPortConfig.rts485 = 1;
                    }
                    else if (upperAval == "TRUE" || aval == "0") {
                        desiredPortConfig.rts485 = 0;
                    }
                    else if (aval == "-1") {
                        desiredPortConfig.rts485 = -1;
                    }
                    else {
                        throw n_u::InvalidParameterException(
                        string("PTB220:") + getName(),
                            aname, aval);
                    }
                }
            }
        }
    }
}

void PTB220::open(int flags) throw (n_u::IOException, n_u::InvalidParameterException)
{
    // So open the device at the base class so we don't invoke any of the sampling functionality...
	// But we do want to invoke the creation of SerialPortIODevice and fromDOMElement()
    DSMSensor::open(flags);

    // Merge the current working with the desired config. We do this because
    // some things may change in the base class fromDOMElement(), affecting the
    // working port config, and some may change in this subclass's fromDOMElement() override,
    // affecting the desired port config.
    if (desiredPortConfig != DEFAULT_PORT_CONFIG) {
    	mergeDesiredWithWorkingConfig(desiredPortConfig, getPortConfig());
    	setPortConfig(desiredPortConfig);
    	applyPortConfig();
    }

    n_c::SerialPortIODevice* pSIODevice = dynamic_cast<n_c::SerialPortIODevice*>(getIODevice());
    // Make sure blocking is set properly
    pSIODevice->getBlocking();
    // Save off desiredConfig - base class should have modified it by now.
    // Do this after applying, as getPortConfig() only gets the items in the SerialPortIODevice object.
    desiredPortConfig = getPortConfig();

    // check the raw mode parameters
    VLOG(("Raw mode is ") << (desiredPortConfig.termios.getRaw() ? "ON" : "OFF"));

    NLOG(("First figure out whether we're talking to the sensor"));
    if (findWorkingSerialPortConfig()) {
        NLOG(("Found working sensor serial port configuration"));
        NLOG((""));
        NLOG(("Attempting to install the desired sensor serial parameter configuration"));
        if (installDesiredSensorConfig()) {
            NLOG(("Desired sensor serial port configuration successfully installed"));
            NLOG((""));
            NLOG(("Attempting to install the desired sensor science configuration"));
            if (configureScienceParameters()) {
                NLOG(("Desired sensor science configuration successfully installed"));
                NLOG(("Opening the NIDAS Way..."));
                SerialSensor::open(flags);
            }
            else {
                NLOG(("Failed to install sensor science configuration"));
            }
        }
        else {
            NLOG(("Failed to install desired config. Reverted back to what works. "
                    "Science configuration is not installed."));
        }
    }
    else
    {
        NLOG(("Couldn't find a serial port configuration that worked with this PTB220 sensor. "
              "May need to troubleshoot the sensor or cable. "
              "!!!NOTE: Sensor is not open for data collection!!!"));
    }
}

bool PTB220::findWorkingSerialPortConfig()
{
    bool foundIt = false;

    // first see if the current configuration is working. If so, all done!
    if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
        NLOG(("Testing initial config which may be custom "));
        printPortConfig();
    }

    if (!doubleCheckResponse()) {
        // initial config didn't work, so sweep through all parameters starting w/the default
        if (!isDefaultConfig(getPortConfig())) {
            // it's a custom config, so test default first
            NLOG(("Testing default config because SerialSensor applied a custom config which failed"));
            if (!testDefaultPortConfig()) {
                NLOG(("Default PortConfig failed. Now testing all the other serial parameter configurations..."));
                foundIt = sweepParameters(true);
            }
            else {
                // found it!! Tell someone!!
                foundIt = true;
                if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                    NLOG(("Default PortConfig was successfull!!!"));
                    printPortConfig();
                }
            }
        }
        else {
            NLOG(("Default PortConfig was not changed and failed. Now testing all the other serial "
                  "parameter configurations..."));
            foundIt = sweepParameters(true);
        }
    }
    else {
        // Found it! Tell someone!
        if (!isDefaultConfig(getPortConfig())) {
            NLOG(("SerialSensor customimized the default PortConfig and it succeeded!!"));
        }
        else {
            NLOG(("SerialSensor did not customimize the default PortConfig and it succeeded!!"));
        }

        foundIt = true;
        if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
            printPortConfig();
        }
    }

    return foundIt;
}

void PTB220::mergeDesiredWithWorkingConfig(PortConfig& rDesired, const PortConfig& rWorking)
{
	// Always want the desired port config to take precedence,
	// if it's been changed by the derived class via the autoconfig tag
	// Desired port config is initialized to the default port config. So if it
	// hasn't changed, but is not equal to the working port config, then assign the
	// working port config value to the desired port config
	if (rDesired.termios == DEFAULT_PORT_CONFIG.termios && rDesired.termios != rWorking.termios) {
		if (rDesired.termios.getBaudRate() != rWorking.termios.getBaudRate()) {
			rDesired.termios.setBaudRate(rWorking.termios.getBaudRate());
		}
		if (rDesired.termios.getParity() != rWorking.termios.getParity()) {
			rDesired.termios.setParity(rWorking.termios.getParity());
		}
		if (rDesired.termios.getDataBits() != rWorking.termios.getDataBits()) {
			rDesired.termios.setDataBits(rWorking.termios.getDataBits());
		}
		if (rDesired.termios.getStopBits() != rWorking.termios.getStopBits()) {
			rDesired.termios.setStopBits(rWorking.termios.getStopBits());
		}
	}

	if (rDesired.rts485 == DEFAULT_PORT_CONFIG.rts485 && rDesired.rts485 != rWorking.rts485) {
		rDesired.rts485 = rWorking.rts485;
	}

	if (rDesired.xcvrConfig == DEFAULT_PORT_CONFIG.xcvrConfig && rDesired.xcvrConfig != rWorking.xcvrConfig) {
		if (rDesired.xcvrConfig.port == DEFAULT_PORT_CONFIG.xcvrConfig.port && rDesired.xcvrConfig.port != rWorking.xcvrConfig.port) {
			rDesired.xcvrConfig.port = rWorking.xcvrConfig.port;
		}
		if (rDesired.xcvrConfig.portType == DEFAULT_PORT_CONFIG.xcvrConfig.portType && rDesired.xcvrConfig.portType != rWorking.xcvrConfig.portType) {
			rDesired.xcvrConfig.portType = rWorking.xcvrConfig.portType;
		}
		if (rDesired.xcvrConfig.sensorPower == DEFAULT_PORT_CONFIG.xcvrConfig.sensorPower && rDesired.xcvrConfig.sensorPower != rWorking.xcvrConfig.sensorPower) {
			rDesired.xcvrConfig.sensorPower = rWorking.xcvrConfig.sensorPower;
		}
		if (rDesired.xcvrConfig.termination == DEFAULT_PORT_CONFIG.xcvrConfig.termination && rDesired.xcvrConfig.termination != rWorking.xcvrConfig.termination) {
			rDesired.xcvrConfig.termination = rWorking.xcvrConfig.termination;
		}

		rDesired.applied = false;
	}
}

bool PTB220::installDesiredSensorConfig()
{
    bool installed = false;
    PortConfig sensorPortConfig = getPortConfig();

    // at this point we need to determine whether or not the current working config 
    // is the desired config, and adjust as necessary
    if (desiredPortConfig != sensorPortConfig) {
        // Gotta modify the PTB220 parameters first, and the modify our parameters to match and hope for the best.
        // We only do this for the serial and science parameters, as the sensor is physically configured to use  
        // the transceiver mode we discovered it works on. To change these parameters, the user would have to  
        // physically reconfigure the sensor and re-start the auto-config process.
        DLOG(("Attempting to set the serial configuration to the desired configuration."));

        serPortFlush(O_RDWR);
        
        sendSensorCmd(SENSOR_SERIAL_BAUD_CMD, PTB_ARG(desiredPortConfig.termios.getBaudRate()));
        sendSensorCmd(SENSOR_SERIAL_PARITY_CMD, PTB_ARG(desiredPortConfig.termios.getParityString(true)));
        sendSensorCmd(SENSOR_SERIAL_DATABITS_CMD, PTB_ARG(desiredPortConfig.termios.getDataBits()));
        sendSensorCmd(SENSOR_SERIAL_STOPBITS_CMD, PTB_ARG(desiredPortConfig.termios.getStopBits()), true); // send RESET w/this one...

        setPortConfig(desiredPortConfig);
        applyPortConfig();
        if (getPortConfig() == desiredPortConfig) {
            // wait for the sensor to reset - ~1 second
            usleep(SENSOR_RESET_WAIT_TIME);
            if (!doubleCheckResponse()) {
                if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                    NLOG(("PTB220::installDesiredSensorConfig() failed to achieve sensor communication "
                            "after setting desired serial port parameters. This is the current PortConfig"));
                    printPortConfig();
                }

                setPortConfig(sensorPortConfig);
                applyPortConfig();

                if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
                    DLOG(("Setting the port config back to something that works for a retry"));
                    printPortConfig();
                }
                
                if (!doubleCheckResponse()) {
                    DLOG(("The sensor port config which originally worked before attempting "
                          "to set the desired config no longer works. Really messed up now!"));
                }

                else if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
                    DLOG(("PTB220 reset to original!!!"));
                    printPortConfig();
                }
            }
            else {
                if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                    NLOG(("Success!! PTB220 set to desired configuration!!!"));
                    printPortConfig();
                }
                installed = true;
            }
        }

        else if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
            DLOG(("Attempt to set PortConfig to desiredPortConfig failed."));
            DLOG(("Desired PortConfig: "));
            printTargetConfig(desiredPortConfig);
            DLOG(("Actual set PortConfig: "));
            printPortConfig();
        }
    }

    else {
        if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
            NLOG(("Desired config is already set and tested."));
            printPortConfig();
        }
        installed = true;
    }

    DLOG(("Returning installed status: ") << (installed ? "SUCCESS!!" : "failed..."));
    return installed;
}

bool PTB220::configureScienceParameters()
{
    DLOG(("Sending sensor science parameters."));
    sendScienceParameters();
    DLOG(("First check of desired science parameters"));
    bool success = checkScienceParameters();
    if (!success) {
        DLOG(("First attempt to send science parameters failed - resending"));
        sendScienceParameters();
        success = checkScienceParameters();
        if (!success) {
            DLOG(("Second attempt to send science parameters failed. Giving up."));
        }
    }

    return success;
}

void PTB220::sendScienceParameters() {
    bool desiredIsDefault = true;

    DLOG(("Check for whether the desired science parameters are the same as the default"));
    for (int i=0; i< NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if ((desiredScienceParameters[i].cmd != DEFAULT_SCIENCE_PARAMETERS[i].cmd)
            || (desiredScienceParameters[i].arg != DEFAULT_SCIENCE_PARAMETERS[i].arg)) {
            desiredIsDefault = false;
            break;
        }
    }

    if (desiredIsDefault) NLOG(("Base class did not modify the default science parameters for this PB210"));
    else NLOG(("Base class modified the default science parameters for this PB210"));

    DLOG(("Sending science parameters"));
    for (int j=0; j<NUM_DEFAULT_SCIENCE_PARAMETERS; ++j) {
        sendSensorCmd(desiredScienceParameters[j].cmd, desiredScienceParameters[j].arg);
    }
    sendSensorCmd(SENSOR_RESET_CMD);
    usleep(SENSOR_RESET_WAIT_TIME);
}

bool PTB220::checkScienceParameters() {

    VLOG(("PTB220::checkScienceParameters() - Flush port and send query command"));
    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    sendSensorCmd(SENSOR_CONFIG_QRY_CMD);

    static const int BUF_SIZE = 2048;
    int bufRemaining = BUF_SIZE;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    VLOG(("PTB220::checkScienceParameters() - Read the entire response"));
    int numCharsRead = readResponse(&(respBuf[0]), bufRemaining, 2000);
    int totalCharsRead = numCharsRead;
    bufRemaining -= numCharsRead;

    if (LOG_LEVEL_IS_ACTIVE(LOGGER_VERBOSE)) {
        if (numCharsRead > 0) {
            VLOG(("Initial num chars read is: ") << numCharsRead << " comprised of: ");
            for (int i=0; i<5; ++i) {
                char hexBuf[60];
                memset(hexBuf, 0, 60);
                for (int j=0; j<10; ++j) {
                    snprintf(&(hexBuf[j*6]), 6, "%-#.2x     ", respBuf[(i*10)+j]);
                }
                VLOG((&(hexBuf[0])));
            }
        }
    }
    
    for (int i=0; (numCharsRead > 0 && bufRemaining > 0); ++i) {
        numCharsRead = readResponse(&(respBuf[totalCharsRead]), bufRemaining, 2000);
        totalCharsRead += numCharsRead;
        bufRemaining -= numCharsRead;

		if (numCharsRead == 0) {
			if (LOG_LEVEL_IS_ACTIVE(LOGGER_VERBOSE)) {
				VLOG(("Took ") << i+1 << " reads to get entire response");
            }
        }
    }

    if (totalCharsRead) {
        DLOG(("Response: "));
        DLOG((std::string(&respBuf[0], totalCharsRead).c_str()));
    }

    VLOG(("PTB220::checkScienceParameters() - Check the individual parameters available to us"));
    bool scienceParametersOK = false;
    int regexStatus = -1;
    regmatch_t matches[4];
    int nmatch = sizeof(matches) / sizeof(regmatch_t);
    
    // check for sample averaging
    if ((regexStatus = regexec(&avgTime, &(respBuf[0]), nmatch, matches, 0)) == 0) {
    	if (matches[2].rm_so >= 0) {
			string argStr = std::string(&(respBuf[matches[2].rm_so]), (matches[2].rm_eo - matches[2].rm_so));
			VLOG(("Checking sample averaging with argument: ") << argStr);
			scienceParametersOK = compareScienceParameter(SENSOR_AVG_TIME_CMD, argStr.c_str());
			if (!scienceParametersOK) {
				DLOG(("PTB220::checkScienceParameters(): Failed to find averaging time: ") << argStr );
			}
    	}
    }
    else {
        char regerrbuf[64];
        ::regerror(regexStatus, &avgTime, regerrbuf, sizeof regerrbuf);
        throw n_u::ParseException("regexec average time RE", string(regerrbuf));
    }    

    // check for output interval
    if (scienceParametersOK) {
        if ((regexStatus = regexec(&outInterval, respBuf, nmatch, matches, 0)) == 0) {
        	if (matches[2].rm_so >= 0) {
                string argStr = std::string(&(respBuf[matches[2].rm_so]), (matches[2].rm_eo - matches[2].rm_so));
                DLOG(("Checking output interval with argument: ") << argStr);
                scienceParametersOK = compareScienceParameter(SENSOR_OUTPUT_INTERVAL_VAL_CMD, argStr.c_str());
    			if (!scienceParametersOK) {
    				DLOG(("PTB220::checkScienceParameters(): Failed to find output interval: ") << argStr );
    			}
    			else if (matches[3].rm_so >= 0) {
                    string argStr = std::string(&(respBuf[matches[3].rm_so]), (matches[3].rm_eo - matches[3].rm_so));
                    DLOG(("Checking output interval units with argument: ") << argStr);
                    scienceParametersOK = compareScienceParameter(SENSOR_OUTPUT_INTERVAL_UNIT_CMD, argStr.c_str());
        			if (!scienceParametersOK) {
        				DLOG(("PTB220::checkScienceParameters(): Failed to find output interval units: ") << argStr );
        			}
    			}
        	}
        }
        else {
            char regerrbuf[64];
            ::regerror(regexStatus, &outInterval, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("regexec output interval RE", string(regerrbuf));
        }    
    }

    // check for pressure units
    if (scienceParametersOK) {
        if ((regexStatus = regexec(&pressUnit, respBuf, nmatch, matches, 0)) == 0 && matches[2].rm_so >= 0) {
            string argStr = std::string(&(respBuf[matches[2].rm_so]), (matches[2].rm_eo - matches[2].rm_so));
            VLOG(("Checking pressure units with argument: ") << argStr);
            scienceParametersOK = compareScienceParameter(SENSOR_PRESS_UNIT_CMD, argStr.c_str());
			if (!scienceParametersOK) {
				DLOG(("PTB220::checkScienceParameters(): Failed to find pressure units: ") << argStr );
			}
        }
        else {
            char regerrbuf[64];
            ::regerror(regexStatus, &pressUnit, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("regexec pressure unit RE", string(regerrbuf));
        }
    }

    // check for temperature units
    if (scienceParametersOK) {
        if ((regexStatus = regexec(&tempUnit, respBuf, nmatch, matches, 0)) == 0 && matches[2].rm_so >= 0) {
            string argStr = std::string(&(respBuf[matches[2].rm_so]), (matches[2].rm_eo - matches[2].rm_so));
            VLOG(("Checking temperature units with argument: ") << argStr);
            scienceParametersOK = compareScienceParameter(SENSOR_TEMP_UNIT_CMD, argStr.c_str());
			if (!scienceParametersOK) {
				DLOG(("PTB220::checkScienceParameters(): Failed to find temperature units: ") << argStr );
			}
        }
        else {
            char regerrbuf[64];
            ::regerror(regexStatus, &tempUnit, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("regexec temp unit RE", string(regerrbuf));
        }    
    }

    // check for multi-point correction
    if (scienceParametersOK) {
        if ((regexStatus = regexec(&multiCorr, respBuf, nmatch, matches, 0)) == 0 && matches[2].rm_so >= 0) {
            string argStr = std::string(&(respBuf[matches[2].rm_so]), (matches[2].rm_eo - matches[2].rm_so));
            VLOG(("Checking multi-point correction with argument: ") << argStr);
            scienceParametersOK = (argStr == "ON");
			if (!scienceParametersOK) {
				DLOG(("PTB220::checkScienceParameters(): Failed to determine multipoint correction status ON: ") << argStr );
			}
        }
        else {
            char regerrbuf[64];
            ::regerror(regexStatus, &multiCorr, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("regexec multi-point correction RE", string(regerrbuf));
        }    
    }

//    // check for calibration date correction
//    if (scienceParametersOK) {
//        if ((regexStatus = regexec(&calDate, respBuf, nmatch, matches, 0)) == 0 && matches[2].rm_so >= 0) {
//            string argStr = std::string(&(respBuf[matches[2].rm_so]), (matches[2].rm_eo - matches[2].rm_so));
//            VLOG(("Checking cal date with argument: ") << argStr);
//            scienceParametersOK = true; // TODO: for now. My unit has this set to ????
//            if (argStr != "\?\?\?\?-\?\?-\?\?") {
//            	WLOG(("PTB220::checkScienceParameters() - Cal Date is invalid: ") << argStr);
//            }
//        }
//        else {
//            char regerrbuf[64];
//            ::regerror(regexStatus, &calDate, regerrbuf, sizeof regerrbuf);
//            throw n_u::ParseException("regexec calDate RE", string(regerrbuf));
//        }
//    }
//
    return scienceParametersOK;
}

bool PTB220::compareScienceParameter(PTB220_COMMANDS cmd, const char* match)
{
    PTB_CMD_ARG desiredCmd = getDesiredCmd(cmd);
    VLOG(("Looking for command: ") << cmd);
    VLOG(("Found command: ") << desiredCmd.cmd);

    switch (cmd) {
        // These need to match warp str to int first
		case SENSOR_TEMP_UNIT_CMD:
		case SENSOR_PRESS_UNIT_CMD:
            VLOG(("Arguments match: ") << (desiredCmd.arg.intArg == pressUnitStr2PressUnit(match) ? "TRUE" : "FALSE"));
            return (desiredCmd.arg.intArg == pressUnitStr2PressUnit(match));
            break;

        // These need to match ints
		case SENSOR_AVG_TIME_CMD:
		case SENSOR_OUTPUT_INTERVAL_VAL_CMD:
		case SENSOR_SEND_MODE_CMD:
            {
                int arg;
                std::stringstream argStrm(match);
                argStrm >> arg;

                VLOG(("Arguments match: ") << (desiredCmd.arg.intArg == arg ? "TRUE" : "FALSE"));
                return (desiredCmd.arg.intArg == arg);
            }
            break;

        // These need to match strings
		case SENSOR_OUTPUT_INTERVAL_UNIT_CMD:
		case SENSOR_DATA_OUTPUT_FORMAT_CMD:
		default:
            VLOG(("Arguments match: ") << (desiredCmd.arg.strArg == std::string(match) ? "TRUE" : "FALSE"));
			return desiredCmd.arg.strArg.find(match) != std::string::npos;
			break;

    }

    // gotta shut the compiler up...
    return false;
}

PTB_CMD_ARG PTB220::getDesiredCmd(PTB220_COMMANDS cmd) {
    VLOG(("Looking in desiredScienceParameters[] for ") << cmd);
    for (int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if (desiredScienceParameters[i].cmd == cmd) {
            VLOG(("Found command: ") << cmd);
            return desiredScienceParameters[i];
        }
    }

    VLOG(("Requested cmd not found: ") << cmd);

    PTB_CMD_ARG nullRetVal(NULL_COMMAND, PTB_ARG(0));
    return(nullRetVal);
}


bool PTB220::testDefaultPortConfig()
{
    // get the existing PortConfig to preserve the port
    testPortConfig = getPortConfig();

    // copy in the defaults
    setTargetPortConfig(testPortConfig, DEFAULT_BAUD_RATE, DEFAULT_DATA_BITS, DEFAULT_PARITY, DEFAULT_STOP_BITS, 
                                        DEFAULT_RTS485, DEFAULT_PORT_TYPE, DEFAULT_SENSOR_TERMINATION, 
                                        DEFAULT_SENSOR_POWER);
    // send it back up the hierarchy
    setPortConfig(testPortConfig);

    // apply it to the hardware
    applyPortConfig();

    // test it
    return doubleCheckResponse();
}

bool PTB220::sweepParameters(bool defaultTested)
{
    bool foundIt = false;

    for (int i=0; i<NUM_PORT_TYPES; ++i) {
        int rts485 = 0;
        n_c::PORT_TYPES portType = SENSOR_PORT_TYPES[i];

        if (portType == n_c::RS485_HALF)
            rts485 = -1; // ??? TODO check this out: start low. Let write manage setting high
        else if (portType == n_c::RS422)
            rts485 = -1; // always high, since there are two drivers going both ways

        for (int j=0; j<NUM_SENSOR_BAUDS; ++j) {
            int baud = SENSOR_BAUDS[j];

            for (int k=0; k<NUM_SENSOR_WORD_SPECS; ++k) {
                WordSpec wordSpec = SENSOR_WORD_SPECS[k];

                // get the existing port config to preserve the port
                // which only gets set on construction
                testPortConfig = getPortConfig();

                // now set it to the new parameters
                setTargetPortConfig(testPortConfig, baud, wordSpec.dataBits, wordSpec.parity,
                                                    wordSpec.stopBits, rts485, portType, NO_TERM, 
                                                    DEFAULT_SENSOR_POWER);

                if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
                    DLOG(("Asking for PortConfig:"));
                    printTargetConfig(testPortConfig);
                }

                // don't test the default if already tested.
                if (defaultTested && isDefaultConfig(testPortConfig))
                {
                    // skip
                    NLOG((""));
                    NLOG(("Skipping default configuration since it's already tested..."));
                    continue;
                }

                setPortConfig(testPortConfig);
                applyPortConfig();

                if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                    NLOG((""));
                    NLOG(("Testing PortConfig: "));
                    printPortConfig();
                }

                DLOG(("Checking response once..."));
                if (doubleCheckResponse()) {
                    foundIt = true;
                    return foundIt;
                } 
                else {
                    if (portType == n_c::RS485_HALF || portType == n_c::RS422) {
                        DLOG(("If 422/485, one more try - test the connection w/termination turned on."));
                        setTargetPortConfig(testPortConfig, baud, wordSpec.dataBits, wordSpec.parity,
                                                            wordSpec.stopBits, rts485, portType, TERM_120_OHM, 
                                                            DEFAULT_SENSOR_POWER);
                        if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
                            DLOG(("Asking for PortConfig:"));
                            printTargetConfig(testPortConfig);
                        }

                        setPortConfig(testPortConfig);
                        applyPortConfig();

                        if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                            NLOG(("Testing PortConfig on RS422/RS485 with termination: "));
                            printPortConfig();
                        }

                        if (doubleCheckResponse()) {
                            foundIt = true;
                            return foundIt;
                        }
                    }
                }
            }
        }
    }

    return foundIt;
}

void PTB220::setTargetPortConfig(PortConfig& target, int baud, int dataBits, Termios::parity parity, int stopBits,
                                                     int rts485, n_c::PORT_TYPES portType, n_c::TERM termination, 
                                                     n_c::SENSOR_POWER_STATE power)
{
    target.termios.setBaudRate(baud);
    target.termios.setDataBits(dataBits);
    target.termios.setParity(parity);
    target.termios.setStopBits(stopBits);
    target.rts485 = (rts485);
    target.xcvrConfig.portType = portType;
    target.xcvrConfig.termination = termination;
    target.xcvrConfig.sensorPower = power;

    target.applied =false;
}

bool PTB220::isDefaultConfig(const n_c::PortConfig& target)
{
    return ((target.termios.getBaudRate() == DEFAULT_BAUD_RATE)
            && (target.termios.getParity() == DEFAULT_PARITY)
            && (target.termios.getDataBits() == DEFAULT_DATA_BITS)
            && (target.termios.getStopBits() == DEFAULT_STOP_BITS)
            && (target.rts485 == DEFAULT_RTS485)
            && (target.xcvrConfig.portType == DEFAULT_PORT_TYPE)
            && (target.xcvrConfig.termination == DEFAULT_SENSOR_TERMINATION)
            && (target.xcvrConfig.sensorPower == DEFAULT_SENSOR_POWER));
}

bool PTB220::doubleCheckResponse()
{
    bool foundIt = false;

    DLOG(("Checking response once..."));
    if (checkResponse()) {
        if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
            // tell everyone
            DLOG(("Found working port config: "));
            printPortConfig();
        }

        foundIt = true;
    } 
    else {
        DLOG(("Checking response twice..."));
        if (checkResponse()) {
            // tell everyone
            DLOG(("Response checks out on second try..."));
            if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
                DLOG(("Found working port config: "));
                printPortConfig();
            }

            foundIt = true;
        }
        else {
            DLOG(("Checked response twice, and failed twice."));
        }
    }

    return foundIt;
}

bool PTB220::checkResponse()
{
    static const char* PTB220_VER_STR =           "Software version";
    static const char* PTB220_SERIAL_NUMBER_STR = "Serial number";
    static const char* PTB220_XDUCER_CFG_STR = 	  "Configuration";
    static const char* PTB220_LINEAR_CORR_STR =   "Linear adjustments";
    static const char* PTB220_MULTI_PT_CORR_STR = "Multipoint adjustments";
    static const char* PTB220_CAL_DATE_STR =      "Calibration date";
    static const char* PTB220_SERIAL_PARAM_STR =  "Baud Parity Data Stop Dpx";
    static const char* PTB220_SERIAL_ECHO_STR =   "Echo";
    static const char* PTB220_SENDING_MODE_STR =  "Sending mode";
    static const char* PTB220_MEAS_MODE_STR =  	  "Measurement mode";
    static const char* PTB220_PULSE_MODE_STR =    "Pulse mode";
    static const char* PTB220_ADDRESS_STR =       "Address";
    static const char* PTB220_OUTPUT_INTVL_STR =  "Output interval";
    static const char* PTB220_OUTPUT_FMT_STR =    "Output format";
    static const char* PTB220_ERR_OUTPUT_FMT_STR = "Error output format";
    static const char* PTB220_USR_SEND_STR =  	  "SCOM format";
    static const char* PTB220_PRESS_UNIT_STR =    "Pressure unit";
    static const char* PTB220_TEMP_UNIT_STR =     "Temperature unit";
    static const char* PTB220_AVG_TIME_STR = 	  "Averaging time";

    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    sendSensorCmd(SENSOR_CONFIG_QRY_CMD);

    static const int BUF_SIZE = 2048;
    int bufRemaining = BUF_SIZE;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    int numCharsRead = readResponse(&(respBuf[0]), bufRemaining, 2000);
    int totalCharsRead = numCharsRead;
    bufRemaining -= numCharsRead;

    if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
        if (numCharsRead > 0) {
            DLOG(("Initial num chars read is: ") << numCharsRead << " comprised of: ");
            for (int i=0; i<5; ++i) {
                char hexBuf[60];
                memset(hexBuf, 0, 60);
                for (int j=0; j<10; ++j) {
                    snprintf(&(hexBuf[j*6]), 6, "%-#.2x     ", respBuf[(i*10)+j]);
                }
                DLOG((&(hexBuf[0])));
            }
        }
    }
    
    for (int i=0; (numCharsRead > 0 && bufRemaining > 0); ++i) {
        numCharsRead = readResponse(&(respBuf[totalCharsRead]), bufRemaining, 2000);
        totalCharsRead += numCharsRead;
        bufRemaining -= numCharsRead;

        if (LOG_LEVEL_IS_ACTIVE(LOGGER_DEBUG)) {
            if (numCharsRead == 0) {
                DLOG(("Took ") << i+1 << " reads to get entire response");
            }
        }
    }

    if (totalCharsRead) {
        std::string respStr;
        respStr.append(&respBuf[0], totalCharsRead);

        DLOG(("Response: "));
        DLOG((respStr.c_str()));

        // This is where the response is checked for signature elements
        unsigned int foundPos = 0;
        if ((foundPos = respStr.find(PTB220_VER_STR, foundPos)) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_VER_STR << "\"");
			return false;
        }
        else {
        	// scoop up the software version
        	size_t versIdx = respStr.find_first_of("/", foundPos) + 2;
        	size_t crIdx = respStr.find_first_of("\r", versIdx);
        	sensorSWVersion.append(respStr.substr(versIdx, crIdx-versIdx));
        	DLOG(("Found sensor SW version: ") << sensorSWVersion);
        }

        if ((foundPos = respStr.find(PTB220_SERIAL_NUMBER_STR, foundPos+strlen(PTB220_VER_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_SERIAL_NUMBER_STR << "\"");
			return false;
        }
        else {
        	size_t sernumIdx = respStr.find_first_of("0,1,2,3,4,5,6,7,8,9", foundPos)-1;
        	size_t crIdx = respStr.find_first_of("\r", sernumIdx);
        	sensorSerialNumber.append(respStr.substr(sernumIdx, crIdx-sernumIdx));
        	DLOG(("Found sensor serial number: ") << sensorSerialNumber);
        }

		if ((foundPos = respStr.find(PTB220_XDUCER_CFG_STR, foundPos+strlen(PTB220_SERIAL_NUMBER_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_XDUCER_CFG_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_LINEAR_CORR_STR, foundPos+strlen(PTB220_XDUCER_CFG_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_LINEAR_CORR_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_MULTI_PT_CORR_STR, foundPos+strlen(PTB220_LINEAR_CORR_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_MULTI_PT_CORR_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_CAL_DATE_STR, foundPos+strlen(PTB220_MULTI_PT_CORR_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_CAL_DATE_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_SERIAL_PARAM_STR, foundPos+strlen(PTB220_CAL_DATE_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_SERIAL_PARAM_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_SERIAL_ECHO_STR, foundPos+strlen(PTB220_SERIAL_PARAM_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_SERIAL_ECHO_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_SENDING_MODE_STR, foundPos+strlen(PTB220_SERIAL_ECHO_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_SENDING_MODE_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_MEAS_MODE_STR, foundPos+strlen(PTB220_SENDING_MODE_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_MEAS_MODE_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_PULSE_MODE_STR, foundPos+strlen(PTB220_MEAS_MODE_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_PULSE_MODE_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_ADDRESS_STR, foundPos+strlen(PTB220_MEAS_MODE_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_ADDRESS_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_OUTPUT_INTVL_STR, foundPos+strlen(PTB220_ADDRESS_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_OUTPUT_INTVL_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_OUTPUT_FMT_STR, foundPos+strlen(PTB220_OUTPUT_INTVL_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_OUTPUT_FMT_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_ERR_OUTPUT_FMT_STR, foundPos+strlen(PTB220_OUTPUT_FMT_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_ERR_OUTPUT_FMT_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_USR_SEND_STR, foundPos+strlen(PTB220_ERR_OUTPUT_FMT_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_USR_SEND_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_PRESS_UNIT_STR, foundPos+strlen(PTB220_USR_SEND_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_PRESS_UNIT_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_TEMP_UNIT_STR, foundPos+strlen(PTB220_PRESS_UNIT_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_TEMP_UNIT_STR << "\"");
			return false;
        }

		if ((foundPos = respStr.find(PTB220_AVG_TIME_STR, foundPos+strlen(PTB220_TEMP_UNIT_STR))) == string::npos) {
			DLOG(("Coundn't find ") << "\"" << PTB220_AVG_TIME_STR << "\"");
			return false;
        }

        return true;
    }

    else {
        DLOG(("Didn't get any chars from serial port"));
        return false;
    }
}


void PTB220::sendSensorCmd(PTB220_COMMANDS cmd, PTB_ARG arg, bool resetNow)
{
    assert(cmd < NUM_SENSOR_CMDS);
    std::string snsrCmd(cmdTable[cmd]);

    // ignoring these commands for now

    //	Require SW4 set to WRITE ENABLE
	// case	SENSOR_MEAS_MODE_CMD:
	// case	SENSOR_LINEAR_CORR_CMD,
	// case	SENSOR_MULTPT_CORR_CMD,
	// case	SENSOR_SET_LINEAR_CORR_CMD,
	// case	SENSOR_SET_MULTPT_CORR_CMD,
	// case	SENSOR_SET_CAL_DATE_CMD,

    //	Require SW3 set to ON
    // case SENSOR_PULSE_MODE_TEST_CMD:

    // ignore external display
	// case SENSOR_DISPLAY_OUTPUT_FORMAT_CMD:
	// case	SENSOR_KEYBD_LCK_CMD,

    // Not an expected use case
	// case SENSOR_USR_DEFINED_SEND_CMD,

    // expect only single xducer devices
	// case	SENSOR_PRESS_DIFF_LIMIT_CMD,

    int insertIdx = snsrCmd.find_first_of('\r');
    DLOG(("PTB220::sendSensorCmd(): insert idx: ") << insertIdx);
    std::ostringstream argStr;

    switch (cmd) {
        // these commands all take an argument...
    	// these take integers...
		case SENSOR_SERIAL_BAUD_CMD:
		case SENSOR_SERIAL_PARITY_CMD:
		case SENSOR_SERIAL_DATABITS_CMD:
		case SENSOR_SERIAL_STOPBITS_CMD:
		case SENSOR_OUTPUT_INTERVAL_VAL_CMD:
		case SENSOR_HEIGHT_CORR_CMD:
		case SENSOR_AVG_TIME_CMD:
		case SENSOR_ADDR_CMD:
		case SENSOR_PRESS_STABILITY_CMD:
		case SENSOR_PRESS_LIMIT_ALARMS_CMD:
		case SENSOR_POLL_SEND_CMD:
		case SENSOR_POLL_OPEN_CMD:
		case SENSOR_TEST_CMD:
            argStr << arg.intArg;
            break;

		// these take strings
        case SENSOR_ECHO_CMD:
		case SENSOR_PRESS_UNIT_CMD:
		case SENSOR_TEMP_UNIT_CMD:
        case SENSOR_DATA_OUTPUT_FORMAT_CMD:
		case SENSOR_ERROR_OUTPUT_FORMAT_CMD:
		case SENSOR_OUTPUT_INTERVAL_UNIT_CMD:
		case SENSOR_PRESS_TRACK_CMD:
        case SENSOR_SEND_MODE_CMD:
                argStr << arg.strArg;
            break;

        // these do not...
		case SENSOR_POLL_CLOSE_CMD:
		case SENSOR_RUN_CMD:
		case SENSOR_STOP_CMD:
		case SENSOR_STOP_SEND_CMD:
        case SENSOR_RESET_CMD:
        case SENSOR_VERIF_CMD:
        case SENSOR_SELF_DIAG_CMD:
        case SENSOR_CONFIG_QRY_CMD:
		case SENSOR_SW_VER_CMD:
		case SENSOR_SER_NUM_CMD:
		case SENSOR_ERR_LIST_CMD:
		case SENSOR_XDUCER_COEFF_LIST_CMD:
		case SENSOR_PRESS_LIMIT_LIST_CMD:
		case SENSOR_CORR_STATUS_CMD:
        default:
            break;
    }

    DLOG(("PTB220::sendSensorCmd(): argStr: ") << argStr.str());
    // Fill in the blank w/argStr, which may be blank....
    snsrCmd.insert(insertIdx, argStr.str());
    DLOG(("PTB220::sendSensorCmd(): snsrCmd: ") << snsrCmd);

    // Write the command - assume the port is already open
    // The PTB220 seems to not be able to keep up with a burst of data, so
    // give it some time between chars - i.e. ~80 words/min rate
    DLOG(("Sending command: "));
    DLOG((snsrCmd.c_str()));
    for (unsigned int i=0; i<snsrCmd.length(); ++i) {
        write(&(snsrCmd.c_str()[i]), 1);
        usleep(CHAR_WRITE_DELAY);
    }
    DLOG(("write() sent ") << snsrCmd.length());;

    // Check whether the client wants to send a reset command for those that require it to take effect
    switch (cmd) {
		case SENSOR_SERIAL_BAUD_CMD:
		case SENSOR_SERIAL_PARITY_CMD:
		case SENSOR_SERIAL_DATABITS_CMD:
		case SENSOR_SERIAL_STOPBITS_CMD:
    	case SENSOR_SEND_MODE_CMD:
    	// Requires SW4 set to write enable
    	// case SENSOR_MEAS_MODE_CMD;
            if (resetNow)
            {
                // only two level deep recursion is OK
                sendSensorCmd(SENSOR_RESET_CMD);
            }
            break;

        // do not reset for these commands to take effect
        case SENSOR_ECHO_CMD:
        case SENSOR_DATA_OUTPUT_FORMAT_CMD:
		case SENSOR_ERROR_OUTPUT_FORMAT_CMD:
		case SENSOR_PRESS_UNIT_CMD:
		case SENSOR_TEMP_UNIT_CMD:
		case SENSOR_HEIGHT_CORR_CMD:
		case SENSOR_AVG_TIME_CMD:
		case SENSOR_OUTPUT_INTERVAL_VAL_CMD:
		case SENSOR_OUTPUT_INTERVAL_UNIT_CMD:
		case SENSOR_ADDR_CMD:
		case SENSOR_PRESS_STABILITY_CMD:
		case SENSOR_PRESS_LIMIT_ALARMS_CMD:
		case SENSOR_RUN_CMD:
		case SENSOR_STOP_CMD:
		case SENSOR_STOP_SEND_CMD:
		case SENSOR_POLL_SEND_CMD:
		case SENSOR_PRESS_TRACK_CMD:
		case SENSOR_POLL_OPEN_CMD:
		case SENSOR_TEST_CMD:
		case SENSOR_RESET_CMD:
		case SENSOR_VERIF_CMD:
		case SENSOR_SELF_DIAG_CMD:
		case SENSOR_CONFIG_QRY_CMD:
		case SENSOR_SW_VER_CMD:
		case SENSOR_SER_NUM_CMD:
		case SENSOR_ERR_LIST_CMD:
		case SENSOR_XDUCER_COEFF_LIST_CMD:
		case SENSOR_PRESS_LIMIT_LIST_CMD:
		case SENSOR_CORR_STATUS_CMD:
		case SENSOR_POLL_CLOSE_CMD:
        default:
            break;
    }
}

size_t PTB220::readResponse(void *buf, size_t len, int msecTimeout)
{
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(getReadFd(), &fdset);

    struct timeval tmpto = { msecTimeout / MSECS_PER_SEC,
        (msecTimeout % MSECS_PER_SEC) * USECS_PER_MSEC };

    int res = ::select(getReadFd()+1,&fdset,0,0,&tmpto);

    if (res < 0) {
        DLOG(("General select error on: ") << getDeviceName() << ": error: " << errno);
        return -1;
    }

    if (res == 0) {
        DLOG(("Select timeout on: ") << getDeviceName() << ": " << msecTimeout << " msec");
        return 0;
    }

    // no select timeout or error, so get the goodies out of the buffer...
    return read(buf,len);
}

void PTB220::updateDesiredScienceParameter(PTB220_COMMANDS cmd, PTB_ARG arg) {
    for(int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if (cmd == desiredScienceParameters[i].cmd) {
            desiredScienceParameters[i].arg = arg;
            break;
        }
    }
}



}}} //namespace nidas { namespace dynld { namespace isff {

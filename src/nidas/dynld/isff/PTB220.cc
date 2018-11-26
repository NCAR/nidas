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
	{8,Termios::NONE,2}
};

const n_c::PORT_TYPES PTB220::SENSOR_PORT_TYPES[PTB220::NUM_PORT_TYPES] = {n_c::RS232, n_c::RS422, n_c::RS485_HALF };

// static default configuration to send to base class...
const PortConfig PTB220::DEFAULT_PORT_CONFIG(PTB220::DEFAULT_BAUD_RATE, PTB220::DEFAULT_DATA_BITS,
                                             PTB220::DEFAULT_PARITY, PTB220::DEFAULT_STOP_BITS,
                                             PTB220::DEFAULT_PORT_TYPE, PTB220::DEFAULT_SENSOR_TERMINATION,
                                             PTB220::DEFAULT_SENSOR_POWER, PTB220::DEFAULT_RTS485,
                                             PTB220::DEFAULT_CONFIG_APPLIED);

const n_c::SensorCmdData PTB220::DEFAULT_SCIENCE_PARAMETERS[] = {
	n_c::SensorCmdData(DEFAULT_PRESS_UNITS_CMD, n_c::SensorCmdArg(pressTempUnitsArgsToStr(DEFAULT_PRESS_UNITS))),
	n_c::SensorCmdData(DEFAULT_TEMP_UNITS_CMD, n_c::SensorCmdArg(pressTempUnitsArgsToStr(DEFAULT_TEMP_UNITS))),
	n_c::SensorCmdData(DEFAULT_OUTPUT_RATE_CMD, n_c::SensorCmdArg(DEFAULT_OUTPUT_RATE)),
	n_c::SensorCmdData(DEFAULT_OUTPUT_RATE_UNITS_CMD, n_c::SensorCmdArg(DEFAULT_OUTPUT_RATE_UNIT)),
	n_c::SensorCmdData(DEFAULT_SAMPLE_AVERAGING_CMD, n_c::SensorCmdArg(DEFAULT_AVG_TIME)),
	n_c::SensorCmdData(DEFAULT_OUTPUT_FORMAT_CMD, n_c::SensorCmdArg(DEFAULT_SENSOR_OUTPUT_FORMAT)),
	n_c::SensorCmdData(DEFAULT_SENSOR_SEND_MODE_CMD, n_c::SensorCmdArg(DEFAULT_SENSOR_SEND_MODE))
};

const int PTB220::NUM_DEFAULT_SCIENCE_PARAMETERS = sizeof(DEFAULT_SCIENCE_PARAMETERS)/sizeof(n_c::SensorCmdData);

const char* PTB220::DEFAULT_SENSOR_OUTPUT_FORMAT = "\"B\" ADDR \" \" 4.3 P1 3.1 T1 #r #n";
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

    // Let the base class know about PTB210 RS232 limitations
    for (int i=0; i<NUM_PORT_TYPES; ++i) {
    	_portTypeList.push_back(SENSOR_PORT_TYPES[i]);
    }

    for (int i=0; i<NUM_SENSOR_BAUDS; ++i) {
    	_baudRateList.push_back(SENSOR_BAUDS[i]);
    }

    for (int i=0; i<NUM_SENSOR_WORD_SPECS; ++i) {
    	_serialWordSpecList.push_back(SENSOR_WORD_SPECS[i]);
    }

    desiredScienceParameters = new n_c::SensorCmdData[NUM_DEFAULT_SCIENCE_PARAMETERS];
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
                    updateDesiredScienceParameter(SENSOR_PRESS_UNIT_CMD, n_c::SensorCmdArg(strToPressTempUnits(upperAval)));
                }
                if (aname == "tempunits") {
                    updateDesiredScienceParameter(SENSOR_TEMP_UNIT_CMD, n_c::SensorCmdArg(strToPressTempUnits(upperAval)));
                }
                else if (aname == "outputformat") {
                	// Just assume they know what they're doin'
                    updateDesiredScienceParameter(SENSOR_DATA_OUTPUT_FORMAT_CMD, n_c::SensorCmdArg(upperAval));
                }
                else if (aname == "averagetime") {
                    istringstream ist(aval);
                    int val = 0;
					ist >> val;
                    if (ist.fail() || val < SENSOR_AVG_TIME_MIN || val > SENSOR_AVG_TIME_MAX)
                        throw n_u::InvalidParameterException(
                            string("PTB220:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_AVG_TIME_CMD, n_c::SensorCmdArg(val));
                }
                else if (aname == "outputintvl") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || val < SENSOR_OUTPUT_RATE_MIN || val > SENSOR_OUTPUT_RATE_MAX)
                        throw n_u::InvalidParameterException(
                            string("PTB220:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_OUTPUT_INTERVAL_VAL_CMD, n_c::SensorCmdArg(val));
                }
                else if (aname == "outputintvlunit") {
                	std::string arg(upperAval);
                    if ( arg != "S" && arg != "MIN" && arg != "H")
                        throw n_u::InvalidParameterException(
                            string("PTB220:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_OUTPUT_INTERVAL_UNIT_CMD, n_c::SensorCmdArg(arg));
                }
                else if (aname == "address") {
                    istringstream ist(aval);
                    int addr;
                    ist >> addr;
                    if (ist.fail() || addr< SENSOR_ADDR_MIN || addr > SENSOR_ADDR_MAX)
                        throw n_u::InvalidParameterException(
                            string("PTB220:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_ADDR_CMD, n_c::SensorCmdArg(addr));
                }
            }
        }
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
        
        sendSensorCmd(SENSOR_SERIAL_BAUD_CMD, n_c::SensorCmdArg(desiredPortConfig.termios.getBaudRate()));
        sendSensorCmd(SENSOR_SERIAL_PARITY_CMD, n_c::SensorCmdArg(desiredPortConfig.termios.getParityString(true)));
        sendSensorCmd(SENSOR_SERIAL_DATABITS_CMD, n_c::SensorCmdArg(desiredPortConfig.termios.getDataBits()));
        sendSensorCmd(SENSOR_SERIAL_STOPBITS_CMD, n_c::SensorCmdArg(desiredPortConfig.termios.getStopBits()), true); // send RESET w/this one...

        setPortConfig(desiredPortConfig);
        applyPortConfig();
        if (getPortConfig() == desiredPortConfig) {
            // wait for the sensor to reset - ~1 second
            usleep(SENSOR_RESET_WAIT_TIME);
            if (!doubleCheckResponse()) {
				NLOG(("PTB220::installDesiredSensorConfig() failed to achieve sensor communication "
						"after setting desired serial port parameters. This is the current PortConfig") << getPortConfig());

                setPortConfig(sensorPortConfig);
                applyPortConfig();

				DLOG(("Setting the port config back to something that works for a retry") << getPortConfig());
                
                if (!doubleCheckResponse()) {
                    DLOG(("The sensor port config which originally worked before attempting "
                          "to set the desired config no longer works. Really messed up now!"));
                }

                else {
                    DLOG(("PTB220 reset to original!!!") << getPortConfig());
                }
            }
            else {
				NLOG(("Success!! PTB220 set to desired configuration!!!") << getPortConfig());
                installed = true;
            }
        }

        else {
            DLOG(("Attempt to set PortConfig to desiredPortConfig failed."));
            DLOG(("Desired PortConfig: ") << desiredPortConfig);
            DLOG(("Actual set PortConfig: ") << getPortConfig());
        }
    }

    else {
		NLOG(("Desired config is already set and tested.") << getPortConfig());
        installed = true;
    }

    DLOG(("Returning installed status: ") << (installed ? "SUCCESS!!" : "failed..."));
    return installed;
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

    if (desiredIsDefault) NLOG(("Base class did not modify the default science parameters for this PB220"));
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
    int numCharsRead = readEntireResponse(&(respBuf[0]), bufRemaining, 100);
    int totalCharsRead = numCharsRead;
    bufRemaining -= numCharsRead;

    static LogContext lp(LOG_VERBOSE);
    if (lp.active())
        if (numCharsRead > 0) {
        	printResponseHex(numCharsRead, respBuf);
    }
    
//    for (int i=0; (numCharsRead > 0 && bufRemaining > 0); ++i) {
//        numCharsRead = readResponse(&(respBuf[totalCharsRead]), bufRemaining, 2000);
//        totalCharsRead += numCharsRead;
//        bufRemaining -= numCharsRead;
//
//		if (numCharsRead == 0) {
//			VLOG(("Took ") << i+1 << " reads to get entire response");
//        }
//    }

    if (numCharsRead) {
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

    // PTB220 entries in sensor_catalog.xml don't tend to have an init_string, so need to start it up here.
    if (scienceParametersOK) {
        sendSensorCmd(SENSOR_RUN_CMD);
    }

    return scienceParametersOK;
}

bool PTB220::compareScienceParameter(int cmd, const char* match)
{
    n_c::SensorCmdData desiredCmd = getDesiredCmd(cmd);
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

n_c::SensorCmdData PTB220::getDesiredCmd(int cmd) {
    VLOG(("Looking in desiredScienceParameters[] for ") << cmd);
    for (int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if (desiredScienceParameters[i].cmd == cmd) {
            VLOG(("Found command: ") << cmd);
            return desiredScienceParameters[i];
        }
    }

    VLOG(("Requested cmd not found: ") << cmd);

    n_c::SensorCmdData nullRetVal(NULL_COMMAND, n_c::SensorCmdArg(0));
    return(nullRetVal);
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

    int numCharsRead = readEntireResponse(&(respBuf[0]), bufRemaining, 100);
//    int totalCharsRead = numCharsRead;
//    bufRemaining -= numCharsRead;

    static LogContext lp(LOG_DEBUG);
    if (lp.active()) {
        if (numCharsRead > 0) {
        	printResponseHex(numCharsRead, respBuf);
        }
    }
    
//    for (int i=0; (numCharsRead > 0 && bufRemaining > 0); ++i) {
//        numCharsRead = readResponse(&(respBuf[totalCharsRead]), bufRemaining, 2000);
//        totalCharsRead += numCharsRead;
//        bufRemaining -= numCharsRead;
//
//		if (numCharsRead == 0) {
//			DLOG(("Took ") << i+1 << " reads to get entire response");
//		}
//    }
//
    if (numCharsRead) {
        std::string respStr;
        respStr.append(&respBuf[0], numCharsRead);

        DLOG(("Response: "));
        DLOG((respStr.c_str()));

        // This is where the response is checked for signature elements
        size_t foundPos = 0;
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


void PTB220::sendSensorCmd(int cmd, n_c::SensorCmdArg arg, bool resetNow)
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

//size_t PTB220::readResponse(void *buf, size_t len, int msecTimeout)
//{
//    fd_set fdset;
//    FD_ZERO(&fdset);
//    FD_SET(getReadFd(), &fdset);
//
//    struct timeval tmpto = { msecTimeout / MSECS_PER_SEC,
//        (msecTimeout % MSECS_PER_SEC) * USECS_PER_MSEC };
//
//    int res = ::select(getReadFd()+1,&fdset,0,0,&tmpto);
//
//    if (res < 0) {
//        DLOG(("General select error on: ") << getDeviceName() << ": error: " << errno);
//        return -1;
//    }
//
//    if (res == 0) {
//        DLOG(("Select timeout on: ") << getDeviceName() << ": " << msecTimeout << " msec");
//        return 0;
//    }
//
//    // no select timeout or error, so get the goodies out of the buffer...
//    return read(buf,len);
//}

void PTB220::updateDesiredScienceParameter(int cmd, n_c::SensorCmdArg arg) {
    for(int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if (cmd == desiredScienceParameters[i].cmd) {
            desiredScienceParameters[i].arg = arg;
            break;
        }
    }
}



}}} //namespace nidas { namespace dynld { namespace isff {

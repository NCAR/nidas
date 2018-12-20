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

#include "PTB210.h"
#include <nidas/core/SerialPortIODevice.h>
#include <nidas/util/ParseException.h>

#include <sstream>
#include <limits>
#include <regex.h>

using namespace nidas::core;
using namespace std;

NIDAS_CREATOR_FUNCTION_NS(isff,PTB210)

namespace nidas { namespace dynld { namespace isff {

const char* PTB210::DEFAULT_MSG_SEP_CHARS = "\r\n";

const char* PTB210::SENSOR_RESET_CMD_STR = ".RESET\r";
const char* PTB210::SENSOR_SERIAL_BAUD_CMD_STR = ".BAUD.\r";
const char* PTB210::SENSOR_SERIAL_EVENP_WORD_CMD_STR = ".E71\r";
const char* PTB210::SENSOR_SERIAL_ODDP_WORD_CMD_STR = ".O71\r";
const char* PTB210::SENSOR_SERIAL_NOP_WORD_CMD_STR = ".N81\r";
const char* PTB210::SENSOR_PRESS_MIN_CMD_STR = ".PMIN.\r";
const char* PTB210::SENSOR_PRESS_MAX_CMD_STR = ".PMAX.\r";
const char* PTB210::SENSOR_MEAS_RATE_CMD_STR = ".MPM.\r";
const char* PTB210::SENSOR_NUM_SAMP_AVG_CMD_STR = ".AVRG.\r";
const char* PTB210::SENSOR_POWER_DOWN_CMD_STR = ".PD\r";
const char* PTB210::SENSOR_POWER_UP_CMD_STR = "\r";
const char* PTB210::SENSOR_SINGLE_SAMP_CMD_STR = ".P\r";
const char* PTB210::SENSOR_START_CONT_SAMP_CMD_STR = ".BP\r";
const char* PTB210::SENSOR_STOP_CONT_SAMP_CMD_STR = "\r";
const char* PTB210::SENSOR_SAMP_UNIT_CMD_STR = ".UNIT.\r";
const char* PTB210::SENSOR_EXC_UNIT_CMD_STR = ".FORM.0";
const char* PTB210::SENSOR_INC_UNIT_CMD_STR = ".FORM.1";
const char* PTB210::SENSOR_CORRECTION_ON_CMD_STR = ".MPCON\r";
const char* PTB210::SENSOR_CORRECTION_OFF_CMD_STR = ".MPCOFF\r";
const char* PTB210::SENSOR_TERM_ON_CMD_STR = ".RON\r";
const char* PTB210::SENSOR_TERM_OFF_CMD_STR = ".ROFF\r";
const char* PTB210::SENSOR_CONFIG_QRY_CMD_STR = "\r.?\r";

const char* PTB210::cmdTable[NUM_SENSOR_CMDS] =
{
    SENSOR_RESET_CMD_STR,
    SENSOR_SERIAL_BAUD_CMD_STR,
    SENSOR_SERIAL_EVENP_WORD_CMD_STR,
    SENSOR_SERIAL_ODDP_WORD_CMD_STR,
    SENSOR_SERIAL_NOP_WORD_CMD_STR,
    SENSOR_MEAS_RATE_CMD_STR,
    SENSOR_NUM_SAMP_AVG_CMD_STR,
    SENSOR_PRESS_MIN_CMD_STR,
    SENSOR_PRESS_MAX_CMD_STR,
    SENSOR_SINGLE_SAMP_CMD_STR,
    SENSOR_START_CONT_SAMP_CMD_STR,
    SENSOR_STOP_CONT_SAMP_CMD_STR,
    SENSOR_POWER_DOWN_CMD_STR,
    SENSOR_POWER_UP_CMD_STR,
    SENSOR_SAMP_UNIT_CMD_STR,
    SENSOR_INC_UNIT_CMD_STR,
    SENSOR_EXC_UNIT_CMD_STR,
    SENSOR_CORRECTION_ON_CMD_STR,
    SENSOR_CORRECTION_OFF_CMD_STR,
    // No way to set the calibration points, so no need to set the Cal date.
    // SENSOR_SET_CAL_DATE_CMD_STR,
    SENSOR_TERM_ON_CMD_STR,
    SENSOR_TERM_OFF_CMD_STR,
    SENSOR_CONFIG_QRY_CMD_STR
};

// NOTE: list sensor bauds from highest to lowest as the higher 
//       ones are the most likely
const int PTB210::SENSOR_BAUDS[NUM_SENSOR_BAUDS] = {19200, 9600, 4800, 2400, 1200};
const WordSpec PTB210::SENSOR_WORD_SPECS[PTB210::NUM_SENSOR_WORD_SPECS] = {
    {7,Termios::EVEN,1}, 
    {7,Termios::ODD,1}, 
    {8,Termios::NONE,1}
};
const n_c::PORT_TYPES PTB210::SENSOR_PORT_TYPES[PTB210::NUM_PORT_TYPES] = {n_c::RS232, n_c::RS422, n_c::RS485_HALF };


// static default configuration to send to base class...
const PortConfig PTB210::DEFAULT_PORT_CONFIG(PTB210::DEFAULT_BAUD_RATE, PTB210::DEFAULT_DATA_BITS,
                                             PTB210::DEFAULT_PARITY, PTB210::DEFAULT_STOP_BITS,
                                             PTB210::DEFAULT_PORT_TYPE, PTB210::DEFAULT_SENSOR_TERMINATION, 
                                             PTB210::DEFAULT_RTS485,
                                             PTB210::DEFAULT_CONFIG_APPLIED);

const n_c::SensorCmdData PTB210::DEFAULT_SCIENCE_PARAMETERS[] = {
    n_c::SensorCmdData(DEFAULT_PRESSURE_UNITS_CMD, n_c::SensorCmdArg(DEFAULT_PRESSURE_UNITS)),
	n_c::SensorCmdData(DEFAULT_SAMPLE_RATE_CMD, n_c::SensorCmdArg(DEFAULT_SAMPLE_RATE)),
	n_c::SensorCmdData(DEFAULT_SAMPLE_AVERAGING_CMD, n_c::SensorCmdArg(DEFAULT_NUM_SAMPLES_AVERAGED)),
	n_c::SensorCmdData(DEFAULT_OUTPUT_UNITS_CMD, n_c::SensorCmdArg(0)),
	n_c::SensorCmdData(DEFAULT_USE_CORRECTION_CMD, n_c::SensorCmdArg(0))
};

const int PTB210::NUM_DEFAULT_SCIENCE_PARAMETERS = sizeof(DEFAULT_SCIENCE_PARAMETERS)/sizeof(n_c::SensorCmdData);

/* Typical PTB210 .? query response. All line endings are \r\n
 * 
 * PTB210 Ver 2.0                                              
 * CAL DATE       :2017-08-15                                  
 * ID CODE        :0                                           
 * SERIAL NUMBER  :N3310370                                    
 * MULTIPOINT CORR:ON                                          
 * MEAS PER MINUTE:    60                                      
 * AVERAGING      :     0                                      
 * PRESSURE UNIT  : mBar                                       
 * Pressure Min...Max:   490  1110                             
 * LOW CURRENT MODE
 * RS485 RESISTOR OFF
 *
 */


// regular expression strings, contexts, compilation
// NOTE: the regular expressions need to search a buffer w/multiple lines separated by \r\n
static const char* PTB210_VER_REGEX_STR =           "PTB210 Ver ([[:digit:]].[[:digit:]])[[:space:]]+";
static const char* PTB210_CAL_DATE_REGEX_STR =      "(.*[[:space:]]+)+CAL DATE[[:blank:]]+:([[digit:]]{4}(-[[:digit:]]{2}){2}))[[:space:]]+";
static const char* PTB210_ID_CODE_REGEX_STR =       "(.*[[:space:]]+)+ID CODE[[:blank:]]+:([[:digit:]]{1,3})[[:space:]]+";
static const char* PTB210_SERIAL_NUMBER_REGEX_STR = "(.*[[:space:]]+)+SERIAL NUMBER[[:blank:]]+:([[:upper:]][[:digit:]]+)[[:space:]]+";
static const char* PTB210_MULTI_PT_CORR_REGEX_STR = "(.*[[:space:]]+)+MULTIPOINT CORR:(ON|OFF)[[:space:]]+";
static const char* PTB210_MEAS_PER_MIN_REGEX_STR =  "(.*[[:space:]]+)+MEAS PER MINUTE:[[:blank:]]+([[:digit:]]{1,4})[[:space:]]+";
static const char* PTB210_NUM_SMPLS_AVG_REGEX_STR = "(.*[[:space:]]+)+AVERAGING[[:blank:]]+:[[:blank:]]+([[:digit:]]{1,3})[[:space:]]+";
static const char* PTB210_PRESS_UNIT_REGEX_STR =    "(.*[[:space:]]+)+PRESSURE UNIT[[:blank:]]+:[[:blank:]]+([[:alnum:]]{2,5})[[:space:]]+";
static const char* PTB210_PRESS_MINMAX_REGEX_STR =  "(.*[[:space:]]+)+Pressure Min...Max:[[:blank:]]+([[:digit:]]{1,5})[[:blank:]]+([[:digit:]]{1,5})[[:space:]]+";
static const char* PTB210_CURR_MODE_REGEX_STR =     "(.*[[:space:]]+)+CURRENT MODE";
static const char* PTB210_RS485_RES_REGEX_STR =     "(.*[[:space:]]+)+RS485 RESISTOR (ON|OFF)[[:space:]]+";

static regex_t version;
static regex_t calDate;
static regex_t idCode;
static regex_t serNum;
static regex_t multiCorr;
static regex_t measRate;
static regex_t averageSamp;
static regex_t pressUnit;
static regex_t pressMinMax;
static regex_t currentMode;
static regex_t termination;

static bool compileRegex() {
    static bool regexCompiled = false;
    int regStatus = 0;

    if (!regexCompiled) {
        regexCompiled = (regStatus = ::regcomp(&version, PTB210_VER_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus, &version, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("PTB210 version regular expression", string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&calDate, PTB210_CAL_DATE_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus, &calDate, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("PTB210 cal date regular expression", string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&idCode, PTB210_ID_CODE_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&idCode,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB210 id code regular expression", string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&serNum, PTB210_SERIAL_NUMBER_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus, &serNum, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("PTB210 serial number regular expression", string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&multiCorr, PTB210_MULTI_PT_CORR_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&multiCorr,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB210 multipoint correction regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&measRate, PTB210_MEAS_PER_MIN_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&measRate,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB210 measurement rate regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&averageSamp, PTB210_NUM_SMPLS_AVG_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&averageSamp,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB210 samples averaged regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&pressUnit, PTB210_PRESS_UNIT_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&pressUnit,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB210 pressure unit regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&pressMinMax, PTB210_PRESS_MINMAX_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&pressMinMax,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB210 min/max pressure range regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&currentMode, PTB210_CURR_MODE_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&currentMode,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB210 current mode regular expression",
                string(regerrbuf));
        }

        regexCompiled = (regStatus = ::regcomp(&termination, PTB210_RS485_RES_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus,&termination,regerrbuf,sizeof regerrbuf);
            throw n_u::ParseException("PTB210 termination resistor regular expression",
                string(regerrbuf));
        }
    }

    return regexCompiled;
}

static void freeRegex() {
    regfree(&version);
    regfree(&calDate);
    regfree(&idCode);
    regfree(&serNum);
    regfree(&multiCorr);
    regfree(&measRate);
    regfree(&averageSamp);
    regfree(&pressUnit);
    regfree(&pressMinMax);
    regfree(&currentMode);
    regfree(&termination);
}

PTB210::PTB210()
    : SerialSensor(DEFAULT_PORT_CONFIG),
      defaultMessageConfig(DEFAULT_MESSAGE_LENGTH, DEFAULT_MSG_SEP_CHARS, DEFAULT_MSG_SEP_EOM),
      desiredScienceParameters()
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

PTB210::~PTB210()
{
    freeRegex();
    delete [] desiredScienceParameters;
}

void PTB210::fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException)
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
                DLOG(("PTB210:fromDOMElement(): attribute: ") << aname << " : " << upperAval);

                // start with science parameters, assuming SerialSensor took care of any overrides to 
                // the default port config.
                if (aname == "pressunits") {
                    PTB_PRESSURE_UNITS units = mbar;
                    if (upperAval == "HPA") {
                        units = hPa;
                    }
                    else if (upperAval == "MBAR") {
                        units = mbar;
                    }
                    else if (upperAval == "INHG") {
                        units = inHg;
                    }
                    else if (upperAval == "PSIA") {
                        units = psia;
                    }
                    else if (upperAval == "TORR") {
                        units = torr;
                    }
                    else if (upperAval == "MMHG") {
                        units = mmHg;
                    }
                    else if (upperAval == "KPA") {
                        units = kPa;
                    }
                    else if (upperAval == "PA") {
                        units = Pa;
                    }
                    else if (upperAval == "MMH2O") {
                        units = mmH2O;
                    }
                    else if (upperAval == "INH2O") {
                        units = inH2O;
                    }
                    else if (upperAval == "BAR") {
                        units = bar;
                    }
                    else
                        throw n_u::InvalidParameterException(
                            string("PTB210:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_SAMP_UNIT_CMD, units);
                }
                else if (aname == "reportunits") {
                    if (upperAval == "NO" || upperAval == "FALSE" || upperAval == "OFF") {
                        updateDesiredScienceParameter(SENSOR_EXC_UNIT_CMD);
                    }
                    else if (upperAval == "YES" || upperAval == "TRUE" || upperAval == "ON") {
                        updateDesiredScienceParameter(SENSOR_INC_UNIT_CMD);
                    }
                    else
                        throw n_u::InvalidParameterException(
                            string("PTB210:") + getName(), aname, aval);
                }
                else if (aname == "samplesaveraged") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || val < SENSOR_SAMPLE_AVG_MIN || val > SENSOR_SAMPLE_AVG_MAX)
                        throw n_u::InvalidParameterException(
                            string("PTB210:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_NUM_SAMP_AVG_CMD, val);
                }
                else if (aname == "samplerate") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || val < SENSOR_MEAS_RATE_MIN || val > SENSOR_MEAS_RATE_MAX)
                        throw n_u::InvalidParameterException(
                            string("PTB210:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_MEAS_RATE_CMD, val);
                }
                else if (aname == "correction") {
                    if (upperAval == "NO" || upperAval == "FALSE" || upperAval == "OFF") {
                        updateDesiredScienceParameter(SENSOR_CORRECTION_OFF_CMD);
                    }
                    else if (upperAval == "YES" || upperAval == "TRUE" || upperAval == "ON") {
                        updateDesiredScienceParameter(SENSOR_CORRECTION_ON_CMD);
                    }
                    else
                        throw n_u::InvalidParameterException(
                            string("PTB210:") + getName(), aname, aval);
                }
            }
        }
    }
}

bool PTB210::installDesiredSensorConfig(const PortConfig& rDesiredConfig)
{
    bool installed = false;
    const PortConfig sensorPortConfig = getPortConfig();

    // at this point we need to determine whether or not the current working config
    // is the desired config, and adjust as necessary
    if (rDesiredConfig != sensorPortConfig) {
        // Gotta modify the PTB210 parameters first, and the modify our parameters to match and hope for the best.
        // We only do this for the serial and science parameters, as the sensor is physically configured to use
        // the transceiver mode we discovered it works on. To change these parameters, the user would have to
        // physically reconfigure the sensor and re-start the auto-config process.
        DLOG(("Attempting to set the serial configuration to the desired configuration."));

        serPortFlush(O_RDWR);

        sendSensorCmd(SENSOR_SERIAL_BAUD_CMD, n_c::SensorCmdArg(rDesiredConfig.termios.getBaudRate()));

        // PTB210 only supports three combinations of word format - all based on parity
        // So just force it based on parity. Go ahead and reset now, so we can see if we're
        // still talking to each other...
        switch (rDesiredConfig.termios.getParityString(true).c_str()[0]) {
            case 'O':
                sendSensorCmd(SENSOR_SERIAL_ODD_WORD_CMD, n_c::SensorCmdArg(0), true);
                break;

            case 'E':
                sendSensorCmd(SENSOR_SERIAL_EVEN_WORD_CMD, n_c::SensorCmdArg(0), true);
                break;

            case 'N':
                sendSensorCmd(SENSOR_SERIAL_NO_WORD_CMD, n_c::SensorCmdArg(0), true);
                break;

            default:
                break;
        }

        setPortConfig(rDesiredConfig);
        applyPortConfig();
        if (getPortConfig() == rDesiredConfig) {
            // wait for the sensor to reset - ~1 second
            usleep(SENSOR_RESET_WAIT_TIME);
            if (!doubleCheckResponse()) {
				NLOG(("PTB210::installDesiredSensorConfig() failed to achieve sensor communication "
						"after setting desired serial port parameters. This is the current PortConfig") << getPortConfig());

                setPortConfig(sensorPortConfig);
                applyPortConfig();

				DLOG(("Setting the port config back to something that works for a retry") << getPortConfig());

                if (!doubleCheckResponse()) {
                    DLOG(("The sensor port config which originally worked before attempting "
                          "to set the desired config no longer works. Really messed up now!"));
                }

				DLOG(("PTB210 reset to original!!!") << getPortConfig());
            }
            else {
				NLOG(("Success!! PTB210 set to desired configuration!!!") << getPortConfig());
                installed = true;
            }
        }

        else {
            DLOG(("Attempt to set PortConfig to the desired PortConfig failed."));
            DLOG(("Desired PortConfig: ") << rDesiredConfig);
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

void PTB210::sendScienceParameters() {
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
    else
    	NLOG(("Base class modified the default science parameters for this PB210"));

    DLOG(("Sending science parameters"));
    for (int j=0; j<NUM_DEFAULT_SCIENCE_PARAMETERS; ++j) {
        sendSensorCmd(desiredScienceParameters[j].cmd, desiredScienceParameters[j].arg);
    }
    sendSensorCmd(SENSOR_RESET_CMD);
    usleep(SENSOR_RESET_WAIT_TIME);
}

bool PTB210::checkScienceParameters() {

    VLOG(("PTB210::checkScienceParameters() - Flush port and send query command"));
    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    sendSensorCmd(SENSOR_CONFIG_QRY_CMD);

    static const int BUF_SIZE = 512;
    int bufRemaining = BUF_SIZE;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    VLOG(("PTB210::checkScienceParameters() - Read the entire response"));
    int numCharsRead = readResponse(&(respBuf[0]), bufRemaining, 2000);
    int totalCharsRead = numCharsRead;
    bufRemaining -= numCharsRead;

    static n_u::LogContext lp(LOG_VERBOSE);
    if (lp.active()) {
		if (numCharsRead > 0) {
			printResponseHex(numCharsRead, respBuf);
		}
    }
    
    for (int i=0; (numCharsRead > 0 && bufRemaining > 0); ++i) {
        numCharsRead = readResponse(&(respBuf[totalCharsRead]), bufRemaining, 2000);
        totalCharsRead += numCharsRead;
        bufRemaining -= numCharsRead;

		if (numCharsRead == 0) {
			VLOG(("Took ") << i+1 << " reads to get entire response");
		}
    }

    if (totalCharsRead && lp.active()) {
        std::string respStr;
        respStr.append(&respBuf[0], totalCharsRead);

        VLOG(("Response: "));
        VLOG((respStr.c_str()));

    }

    VLOG(("PTB210::checkScienceParameters() - Check the individual parameters available to us"));
    bool scienceParametersOK = false;
    int regexStatus = -1;
    regmatch_t matches[4];
    int nmatch = sizeof(matches) / sizeof(regmatch_t);
    
    // check for sample averaging
    if ((regexStatus = regexec(&averageSamp, &(respBuf[0]), nmatch, matches, 0)) == 0 && matches[2].rm_so >= 0) {
        string argStr = std::string(&(respBuf[matches[2].rm_so]), (matches[2].rm_eo - matches[2].rm_so));
        VLOG(("Checking sample averaging with argument: ") << argStr);
        scienceParametersOK = compareScienceParameter(SENSOR_NUM_SAMP_AVG_CMD, argStr.c_str());
    }
    else {
        char regerrbuf[64];
        ::regerror(regexStatus, &averageSamp, regerrbuf, sizeof regerrbuf);
        throw n_u::ParseException("regexec average samples RE", string(regerrbuf)); 
    }    

    // check for measurement rate
    if (scienceParametersOK) {
        if ((regexStatus = regexec(&measRate, respBuf, nmatch, matches, 0)) == 0 && matches[2].rm_so >= 0) {
            string argStr = std::string(&(respBuf[matches[2].rm_so]), (matches[2].rm_eo - matches[2].rm_so));
            VLOG(("Checking measurement rate with argument: ") << argStr);
            scienceParametersOK = compareScienceParameter(SENSOR_MEAS_RATE_CMD, argStr.c_str());
        }
        else {
            char regerrbuf[64];
            ::regerror(regexStatus, &measRate, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("regexec measurement rate RE", string(regerrbuf));
        }    
    }

    // check for pressure units
    if (scienceParametersOK) {
        if ((regexStatus = regexec(&pressUnit, respBuf, nmatch, matches, 0)) == 0 && matches[2].rm_so >= 0) {
            string argStr = std::string(&(respBuf[matches[2].rm_so]), (matches[2].rm_eo - matches[2].rm_so));
            VLOG(("Checking pressure units with argument: ") << argStr);
            scienceParametersOK = compareScienceParameter(SENSOR_SAMP_UNIT_CMD, argStr.c_str());
        }
        else {
            char regerrbuf[64];
            ::regerror(regexStatus, &pressUnit, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("regexec pressure unit RE", string(regerrbuf));
        }    
    }

    // check for multi-point correction
    if (scienceParametersOK) {
        if ((regexStatus = regexec(&multiCorr, respBuf, nmatch, matches, 0)) == 0 && matches[2].rm_so >= 0) {
            string argStr = std::string(&(respBuf[matches[2].rm_so]), (matches[2].rm_eo - matches[2].rm_so));
            VLOG(("Checking multi-point correction with argument: ") << argStr);
            scienceParametersOK = (argStr == "ON" ? compareScienceParameter(SENSOR_CORRECTION_ON_CMD, argStr.c_str())
                                                  : compareScienceParameter(SENSOR_CORRECTION_OFF_CMD, argStr.c_str()));
        }
        else {
            char regerrbuf[64];
            ::regerror(regexStatus, &multiCorr, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("regexec multi-point correction RE", string(regerrbuf));
        }    
    }

    // // check for termination???
    // if (scienceParametersOK) {
    //     if ((regexStatus = regexec(&measRate, respBuf, nmatch, matches, 0)) == 0 && matches[2].rm_so >= 0) {
    //         string argStr = std::string(&(respBuf[matches[2].rm_so]), (matches[2].rm_eo - matches[2].rm_so));
    //         scienceParametersOK = compareScienceParameter(SENSOR_MEAS_RATE_CMD, argStr.c_str());
    //     }
    //     else {
    //         char regerrbuf[64];
    //         ::regerror(regexStatus, &measRate, regerrbuf, sizeof regerrbuf);
    //         throw n_u::ParseException("regexec measurement rate RE", string(regerrbuf));
    //     }    
    // }

    return scienceParametersOK;
}

bool PTB210::compareScienceParameter(PTB_COMMANDS cmd, const char* match)
{
    n_c::SensorCmdData desiredCmd = getDesiredCmd(cmd);
    VLOG(("Desired command: ") << desiredCmd.cmd);
    VLOG(("Searched command: ") << cmd);

    // Does the command take a parameter? If not, just search for the command
    switch (cmd) {
        // These commands do not take a parameter. 
        // If we found one of these just return true.
        case SENSOR_RESET_CMD:
        case SENSOR_SERIAL_EVEN_WORD_CMD:
        case SENSOR_SERIAL_ODD_WORD_CMD:
        case SENSOR_SERIAL_NO_WORD_CMD:
        case SENSOR_SINGLE_SAMP_CMD:
        case SENSOR_START_CONT_SAMP_CMD:
        case SENSOR_STOP_CONT_SAMP_CMD:
        case SENSOR_POWER_DOWN_CMD:
        case SENSOR_POWER_UP_CMD:
        case SENSOR_INC_UNIT_CMD:
        case SENSOR_CORRECTION_ON_CMD:
        case SENSOR_CORRECTION_OFF_CMD:
        case SENSOR_TERM_ON_CMD:
        case SENSOR_TERM_OFF_CMD:
        case SENSOR_CONFIG_QRY_CMD:
            VLOG(("Returned arg matches command sent: ") << (desiredCmd.cmd == cmd ? "TRUE" : "FALSE"));
            return (desiredCmd.cmd == cmd);
            break;

        // Need to match the command argument, which are all ints
        case SENSOR_SAMP_UNIT_CMD:
            VLOG(("Arguments match: ") << (desiredCmd.arg.intArg == pressUnitStr2PressUnit(match) ? "TRUE" : "FALSE"));
            return (desiredCmd.arg.intArg == pressUnitStr2PressUnit(match));
            break;

        case SENSOR_SERIAL_BAUD_CMD:
        case SENSOR_MEAS_RATE_CMD:
        case SENSOR_NUM_SAMP_AVG_CMD:
        case SENSOR_PRESS_MIN_CMD:
        case SENSOR_PRESS_MAX_CMD:
        default:
            {
                int arg;
                std::stringstream argStrm(match);
                argStrm >> arg;

                VLOG(("Arguments match: ") << (desiredCmd.arg.intArg == arg ? "TRUE" : "FALSE"));
                return (desiredCmd.arg.intArg == arg);
            }
            break;
    }

    // gotta shut the compiler up...
    return false;
}

n_c::SensorCmdData PTB210::getDesiredCmd(PTB_COMMANDS cmd) {
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

bool PTB210::checkResponse()
{
    static const char* PTB210_VER_STR =           "PTB210 Ver";
    static const char* PTB210_CAL_DATE_STR =      "CAL DATE";
    static const char* PTB210_ID_CODE_STR =       "ID CODE";
    static const char* PTB210_SERIAL_NUMBER_STR = "SERIAL NUMBER";
    static const char* PTB210_MULTI_PT_CORR_STR = "MULTIPOINT CORR";
    static const char* PTB210_MEAS_PER_MIN_STR =  "MEAS PER MINUTE";
    static const char* PTB210_NUM_SMPLS_AVG_STR = "AVERAGING";
    static const char* PTB210_PRESS_UNIT_STR =    "PRESSURE UNIT";
    static const char* PTB210_PRESS_MINMAX_STR =  "Pressure Min...Max";
    static const char* PTB210_CURR_MODE_STR =     "CURRENT MODE";
    static const char* PTB210_RS485_RES_STR =     "RS485 RESISTOR";

    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    sendSensorCmd(SENSOR_CONFIG_QRY_CMD);

    static const int BUF_SIZE = 512;
    int bufRemaining = BUF_SIZE;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    int numCharsRead = readResponse(&(respBuf[0]), bufRemaining, 2000);
    int totalCharsRead = numCharsRead;
    bufRemaining -= numCharsRead;

    static LogContext lp(LOG_VERBOSE);
    if (lp.active()) {
    	if (numCharsRead > 0) {
    		printResponseHex(numCharsRead, respBuf);
    	}
    }
    
    for (int i=0; (numCharsRead > 0 && bufRemaining > 0); ++i) {
        numCharsRead = readResponse(&(respBuf[totalCharsRead]), bufRemaining, 2000);
        totalCharsRead += numCharsRead;
        bufRemaining -= numCharsRead;

		if (numCharsRead == 0) {
			DLOG(("Took ") << i+1 << " reads to get entire response");
        }
    }

    if (totalCharsRead) {
        std::string respStr;
        respStr.append(&respBuf[0], totalCharsRead);

        DLOG(("Response: "));
        DLOG((respStr.c_str()));

        // This is where the response is checked for signature elements
        int foundPos = 0;
        bool retVal = (foundPos = respStr.find(PTB210_VER_STR, foundPos) != string::npos);
        if (retVal) {
            retVal = (foundPos = respStr.find(PTB210_CAL_DATE_STR, foundPos+strlen(PTB210_VER_STR)) != string::npos);
            if (retVal) {
                retVal = (foundPos = respStr.find(PTB210_ID_CODE_STR, foundPos+strlen(PTB210_CAL_DATE_STR)) != string::npos);
                if (retVal) {
                    retVal = (foundPos = respStr.find(PTB210_SERIAL_NUMBER_STR, foundPos+strlen(PTB210_ID_CODE_STR)) != string::npos);
                    if (retVal) {
                        retVal = (foundPos = respStr.find(PTB210_MULTI_PT_CORR_STR, foundPos+strlen(PTB210_SERIAL_NUMBER_STR)) != string::npos);
                        if (retVal) {
                            retVal = (foundPos = respStr.find(PTB210_MEAS_PER_MIN_STR, foundPos+strlen(PTB210_MULTI_PT_CORR_STR)) != string::npos);
                            if (retVal) {
                                retVal = (foundPos = respStr.find(PTB210_NUM_SMPLS_AVG_STR, foundPos+strlen(PTB210_MEAS_PER_MIN_STR)) != string::npos);
                                if (retVal) {
                                    retVal = (foundPos = respStr.find(PTB210_PRESS_UNIT_STR, foundPos+strlen(PTB210_NUM_SMPLS_AVG_STR)) != string::npos);
                                    if (retVal) {
                                        retVal = (foundPos = respStr.find(PTB210_PRESS_MINMAX_STR, foundPos+strlen(PTB210_PRESS_UNIT_STR)) != string::npos);
                                        if (retVal) {
                                            retVal = (foundPos = respStr.find(PTB210_CURR_MODE_STR, foundPos+strlen(PTB210_PRESS_MINMAX_STR)) != string::npos);
                                            if (retVal) {
                                                retVal = (foundPos = respStr.find(PTB210_RS485_RES_STR, foundPos+strlen(PTB210_CURR_MODE_STR)) != string::npos);
                                                if (!retVal)
                                                    DLOG(("Coundn't find ") << "\"" << PTB210_RS485_RES_STR << "\"");
                                            }
                                            else
                                                DLOG(("Coundn't find ") << "\"" << PTB210_CURR_MODE_STR << "\"");
                                        }
                                        else
                                            DLOG(("Coundn't find ") << "\"" << PTB210_PRESS_MINMAX_STR << "\"");
                                    }
                                    else
                                        DLOG(("Coundn't find ") << "\"" << PTB210_PRESS_UNIT_STR << "\"");
                                }
                                else
                                    DLOG(("Coundn't find ") << "\"" << PTB210_NUM_SMPLS_AVG_STR << "\"");
                            }
                            else
                                DLOG(("Coundn't find ") << "\"" << PTB210_MEAS_PER_MIN_STR << "\"");
                        }
                        else
                            DLOG(("Coundn't find ") << "\"" << PTB210_MULTI_PT_CORR_STR << "\"");
                    }
                    else
                        DLOG(("Coundn't find ") << "\"" << PTB210_SERIAL_NUMBER_STR << "\"");
                }
                else
                    DLOG(("Coundn't find ") << "\"" << PTB210_ID_CODE_STR << "\"");
            }
            else
                DLOG(("Coundn't find ") << "\"" << PTB210_CAL_DATE_STR << "\"");
        }
        else
            DLOG(("Coundn't find ") << "\"" << PTB210_VER_STR << "\"");

        return retVal;
    }

    else {
        DLOG(("Didn't get any chars from serial port"));
        return false;
    }
}


void PTB210::sendSensorCmd(int cmd, n_c::SensorCmdArg arg, bool resetNow)
{
    assert(cmd < NUM_SENSOR_CMDS);
    std::string snsrCmd(cmdTable[cmd]);

    switch (cmd) {
        // these commands all take an argument...
        case SENSOR_SERIAL_BAUD_CMD:
        case SENSOR_MEAS_RATE_CMD:
        case SENSOR_NUM_SAMP_AVG_CMD:
        case SENSOR_PRESS_MIN_CMD:
        case SENSOR_PRESS_MAX_CMD:
        case SENSOR_SAMP_UNIT_CMD:
            {
                int insertIdx = snsrCmd.find_last_of('.');
                std::ostringstream argStr; 
                argStr << arg.intArg;
                snsrCmd.insert(insertIdx+1, argStr.str());
            }
            break;

        case SENSOR_RESET_CMD:
        case SENSOR_SERIAL_EVEN_WORD_CMD:
        case SENSOR_SERIAL_ODD_WORD_CMD:
        case SENSOR_SERIAL_NO_WORD_CMD:
        case SENSOR_SINGLE_SAMP_CMD:
        case SENSOR_START_CONT_SAMP_CMD:
        case SENSOR_STOP_CONT_SAMP_CMD:
        case SENSOR_POWER_DOWN_CMD:
        case SENSOR_POWER_UP_CMD:
        case SENSOR_INC_UNIT_CMD:
        case SENSOR_CORRECTION_ON_CMD:
        case SENSOR_CORRECTION_OFF_CMD:
        case SENSOR_TERM_ON_CMD:
        case SENSOR_TERM_OFF_CMD:
        case SENSOR_CONFIG_QRY_CMD:
        default:
            break;
    }

    // Write the command - assume the port is already open
    // The PTB210 seems to not be able to keep up with a burst of data, so 
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
        case SENSOR_SERIAL_EVEN_WORD_CMD:
        case SENSOR_SERIAL_ODD_WORD_CMD:
        case SENSOR_SERIAL_NO_WORD_CMD:
        case SENSOR_PRESS_MIN_CMD:
        case SENSOR_PRESS_MAX_CMD:
        case SENSOR_MEAS_RATE_CMD:
        case SENSOR_NUM_SAMP_AVG_CMD:
        case SENSOR_SAMP_UNIT_CMD:
        case SENSOR_EXC_UNIT_CMD:
        case SENSOR_INC_UNIT_CMD:
        case SENSOR_CORRECTION_ON_CMD:
        case SENSOR_CORRECTION_OFF_CMD:
            if (resetNow)
            {
                // only two level deep recursion is OK
                sendSensorCmd(SENSOR_RESET_CMD);
            }
            break;

        case SENSOR_RESET_CMD:
        case SENSOR_SINGLE_SAMP_CMD:
        case SENSOR_START_CONT_SAMP_CMD:
        case SENSOR_STOP_CONT_SAMP_CMD:
        case SENSOR_POWER_DOWN_CMD:
        case SENSOR_POWER_UP_CMD:
        case SENSOR_TERM_ON_CMD:
        case SENSOR_TERM_OFF_CMD:
        case SENSOR_CONFIG_QRY_CMD:
        default:
            break;
    }
}

void PTB210::updateDesiredScienceParameter(PTB_COMMANDS cmd, int arg) {
    PTB_COMMANDS srchCmd = cmd;

    // some commands are paired, so look for cmd's opposite pair instead;
    switch (cmd) {
        case SENSOR_EXC_UNIT_CMD:
            srchCmd = SENSOR_INC_UNIT_CMD;
            break;
        case SENSOR_INC_UNIT_CMD:
            srchCmd = SENSOR_EXC_UNIT_CMD;
            break;
        case SENSOR_CORRECTION_OFF_CMD:
            srchCmd = SENSOR_CORRECTION_ON_CMD;
            break;
        case SENSOR_CORRECTION_ON_CMD:
            srchCmd = SENSOR_CORRECTION_OFF_CMD;
            break;
        default:
            break;
    }

    for(int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if (srchCmd == desiredScienceParameters[i].cmd) {
            desiredScienceParameters[i].cmd = cmd;  // always replace w/cmd, as srchCmd by be different
            desiredScienceParameters[i].arg.intArg = arg;
            break;
        }
    }
}



}}} //namespace nidas { namespace dynld { namespace isff {

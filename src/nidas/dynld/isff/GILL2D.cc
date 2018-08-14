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

#include "GILL2D.h"
#include <nidas/core/SerialPortIODevice.h>
#include <nidas/util/ParseException.h>
#include <nidas/util/IosFlagSaver.h>

#include <sstream>
#include <iomanip>
#include <limits>
#include <regex.h>
#include <time.h>
#include <sys/select.h>

using namespace nidas::core;
using namespace std;

NIDAS_CREATOR_FUNCTION_NS(isff,GILL2D)

namespace nidas { namespace dynld { namespace isff {

const char* GILL2D::DEFAULT_MSG_SEP_CHAR = "\n";

const char* GILL2D::SENSOR_CONFIG_MODE_CMD_STR = "*";
const char* GILL2D::SENSOR_ENABLE_POLLED_MODE_CMD_STR = "?";
const char* GILL2D::SENSOR_POLL_MEAS_CMD_STR = "?";
const char* GILL2D::SENSOR_QRY_ID_CMD_STR = "&";
const char* GILL2D::SENSOR_DISABLE_POLLED_MODE_CMD_STR = "!\r";
const char* GILL2D::SENSOR_SERIAL_BAUD_CMD_STR = "B\r";
const char* GILL2D::SENSOR_DIAG_QRY_CMD_STR = "D\r";
const char* GILL2D::SENSOR_DUPLEX_COMM_CMD_STR = "E\r";
const char* GILL2D::SENSOR_SERIAL_DATA_WORD_CMD_STR = "F\r";
const char* GILL2D::SENSOR_AVG_PERIOD_CMD_STR = "G\r";
const char* GILL2D::SENSOR_HEATING_CMD_STR = "H\r";
const char* GILL2D::SENSOR_NMEA_ID_STR_CMD_STR = "K\r";
const char* GILL2D::SENSOR_MSG_TERM_CMD_STR = "L\r";
const char* GILL2D::SENSOR_MSG_STREAM_CMD_STR = "M\r";
const char* GILL2D::SENSOR_NODE_ADDR_CMD_STR = "N\r";
const char* GILL2D::SENSOR_OUTPUT_FIELD_FMT_CMD_STR = "O\r";
const char* GILL2D::SENSOR_OUTPUT_RATE_CMD_STR = "P\r";
const char* GILL2D::SENSOR_START_MEAS_CMD_STR = "Q\r";
const char* GILL2D::SENSOR_MEAS_UNITS_CMD_STR = "U\r";
const char* GILL2D::SENSOR_VERT_MEAS_PADDING_CMD_STR = "V\r";
const char* GILL2D::SENSOR_ALIGNMENT_CMD_STR = "X\r";

const char* GILL2D::cmdTable[NUM_SENSOR_CMDS] =
{
	SENSOR_CONFIG_MODE_CMD_STR,
	SENSOR_ENABLE_POLLED_MODE_CMD_STR,
	SENSOR_POLL_MEAS_CMD_STR,
	SENSOR_QRY_ID_CMD_STR,
	SENSOR_DISABLE_POLLED_MODE_CMD_STR,
	SENSOR_SERIAL_BAUD_CMD_STR,
	SENSOR_DIAG_QRY_CMD_STR,
	SENSOR_DUPLEX_COMM_CMD_STR,
	SENSOR_SERIAL_DATA_WORD_CMD_STR,
	SENSOR_AVG_PERIOD_CMD_STR,
	SENSOR_HEATING_CMD_STR,
	SENSOR_NMEA_ID_STR_CMD_STR,
	SENSOR_MSG_TERM_CMD_STR,
	SENSOR_MSG_STREAM_CMD_STR,
	SENSOR_NODE_ADDR_CMD_STR,
	SENSOR_OUTPUT_FIELD_FMT_CMD_STR,
	SENSOR_OUTPUT_RATE_CMD_STR,
	SENSOR_START_MEAS_CMD_STR,
	SENSOR_MEAS_UNITS_CMD_STR,
	SENSOR_VERT_MEAS_PADDING_CMD_STR,
	SENSOR_ALIGNMENT_CMD_STR,
};

// NOTE: list sensor bauds from highest to lowest as the higher 
//       ones are the most likely
const int GILL2D::SENSOR_BAUDS[NUM_BAUD_ARGS] = {9600, 19200, 4800, 38400, 2400, 1200, 300};
const GILL2D_DATA_WORD_ARGS GILL2D::SENSOR_WORD_SPECS[NUM_DATA_WORD_ARGS] = { N81, E81, O81};

// GIL instruments do not use RS232
const n_c::PORT_TYPES GILL2D::SENSOR_PORT_TYPES[GILL2D::NUM_PORT_TYPES] = {n_c::RS422, n_c::RS485_HALF };


// static default configuration to send to base class...
const PortConfig GILL2D::DEFAULT_PORT_CONFIG(GILL2D::DEFAULT_BAUD_RATE, GILL2D::DEFAULT_DATA_BITS,
                                             GILL2D::DEFAULT_PARITY, GILL2D::DEFAULT_STOP_BITS,
                                             GILL2D::DEFAULT_PORT_TYPE, GILL2D::DEFAULT_SENSOR_TERMINATION, 
											 GILL2D::DEFAULT_SENSOR_POWER, GILL2D::DEFAULT_RTS485, GILL2D::DEFAULT_CONFIG_APPLIED);

const GILL2D_CMD_ARG GILL2D::DEFAULT_SCIENCE_PARAMETERS[] =
{
    {SENSOR_AVG_PERIOD_CMD, 0},
    {SENSOR_HEATING_CMD, DISABLED},
    {SENSOR_NMEA_ID_STR_CMD, IIMWV},
    {SENSOR_MSG_TERM_CMD, CRLF},
    {SENSOR_MSG_STREAM_CMD, ASC_PLR_CONT},
	{SENSOR_OUTPUT_FIELD_FMT_CMD, CSV},
	{SENSOR_OUTPUT_RATE_CMD, ONE_PER_SEC},
	{SENSOR_MEAS_UNITS_CMD, MPS},
	{SENSOR_VERT_MEAS_PADDING_CMD, DISABLE_VERT_PAD},
	{SENSOR_ALIGNMENT_CMD, U_EQ_NS}
};

const int GILL2D::NUM_DEFAULT_SCIENCE_PARAMETERS = sizeof(DEFAULT_SCIENCE_PARAMETERS)/sizeof(GILL2D_CMD_ARG);

/* Typical GILL2D D3 query response. L1 means all line endings are \r\n
 * 
 * This is actually the factory default settings. For most WindObserver models, 
 * wihch this class implements, A, C, T, Y and Z are not settable.
 * 
 * For non-heated models, H is also not settable.
 * 
 * A0 B3 C1 E1 F1 G0000 H1 J1 K1 L1 M2 NA O1 P1 T1 U1 V1 X1 Y1 Z1
 *
 */


// regular expression strings, contexts, compilation
static const char* GILL2D_RESPONSE_REGEX_STR =   "(.*[[:space:]]+)+"
												"(A[[:digit:]]){0,1} B[[:digit:]] (C[[:digit:]]){0,1} E[[:digit:]] F[[:digit:]] "
		                                         "G[[:digit:]]{4} H[[:digit:]] (J[[:digit:]]){0,1} K[[:digit:]] L[[:digit:]] "
		                                         "M[[:digit:]] N[[:upper:]] O[[:digit:]] P[[:digit:]] (T[[:digit:]]){0,1} "
		                                         "U[[:digit:]] V[[:digit:]] X[[:digit:]] (Y[[:digit:]]){0,1} (Z[[:digit:]]){0,1}";
static const char* GILL2D_COMPARE_REGEX_STR =    "(.*[[:space:]]+)+"
												"(A[[:digit:]]){0,1} B([[:digit:]]) (C[[:digit:]]){0,1} E([[:digit:]]) F([[:digit:]]) "
		                                         "G([[:digit:]]{4}) H([[:digit:]]) (J[[:digit:]]){0,1} K([[:digit:]]) L([[:digit:]]) "
		                                         "M([[:digit:]]) N([[:upper:]]) O([[:digit:]]) P([[:digit:]]) (T[[:digit:]]){0,1} "
		                                         "U([[:digit:]]) V([[:digit:]]) X([[:digit:]]) (Y[[:digit:]]){0,1} (Z[[:digit:]]){0,1}";
static const char* GILL2D_CONFIG_MODE_REGEX_STR = "(.*[[:space:]]+)+CONFIGURATION MODE";

static regex_t sensorCfgResponse;
static regex_t compareScienceResponse;
static regex_t configModeResponse;

static bool compileRegex()
{
    static bool regexCompiled = false;
    int regStatus = 0;

    if (!regexCompiled) {
        regexCompiled = (regStatus = ::regcomp(&sensorCfgResponse, GILL2D_RESPONSE_REGEX_STR, REG_NOSUB|REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus, &sensorCfgResponse, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("GILL2D current config regular expression", string(regerrbuf));
        }
    }

    if (regexCompiled) {
        regexCompiled = (regStatus = ::regcomp(&compareScienceResponse, GILL2D_COMPARE_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus, &compareScienceResponse, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("GILL2D science params compare regular expression", string(regerrbuf));
        }
    }

    if (regexCompiled) {
        regexCompiled = (regStatus = ::regcomp(&configModeResponse, GILL2D_CONFIG_MODE_REGEX_STR, REG_EXTENDED)) == 0;
        if (regStatus) {
            char regerrbuf[64];
            regerror(regStatus, &configModeResponse, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("GILL2D config mode regular expression", string(regerrbuf));
        }
    }

    return regexCompiled;
}

static void freeRegex() {
    regfree(&sensorCfgResponse);
    regfree(&compareScienceResponse);
    regfree(&configModeResponse);

}

GILL2D::GILL2D()
    : Wind2D(DEFAULT_PORT_CONFIG),
	  testPortConfig(),
	  desiredPortConfig(),
      defaultMessageConfig(DEFAULT_MESSAGE_LENGTH, DEFAULT_MSG_SEP_CHAR, DEFAULT_MSG_SEP_EOM),
      desiredScienceParameters(), unitId('\0'), polling(false)
{
    // We set the defaults at construction, 
    // letting the base class modify according to fromDOMElement() 
    setMessageParameters(defaultMessageConfig);

    desiredScienceParameters = new GILL2D_CMD_ARG[NUM_DEFAULT_SCIENCE_PARAMETERS];
    for (int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        desiredScienceParameters[i] = DEFAULT_SCIENCE_PARAMETERS[i];
    }

    compileRegex();
}

GILL2D::~GILL2D()
{
    freeRegex();
    delete [] desiredScienceParameters;
}

void GILL2D::fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException)
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
                // DLOG(("GILL2D:fromDOMElement(): aname: ") << aname << " upperAval: " << upperAval);

                // start with science parameters, assuming SerialSensor took care of any overrides to 
                // the default port config.
                if (aname == "units") {
                    GILL2D_UNITS_ARGS units = MPS;
                    if (upperAval == "MPS") {
                        units = MPS;
                    }
                    else if (upperAval == "KPH") {
                        units = KPH;
                    }
                    else if (upperAval == "KNOTS") {
                        units = KNOTS;
                    }
                    else if (upperAval == "MPH") {
                        units = MPH;
                    }
                    else if (upperAval == "FPM") {
                        units = FPM;
                    }
                    else
                        throw n_u::InvalidParameterException(
                            string("GILL2D:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_MEAS_UNITS_CMD, units);
                }
                else if (aname == "averagetime") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || val < MIN_AVERAGING_TIME || val > MAX_AVERAGING_TIME)
                        throw n_u::InvalidParameterException(
                            string("GILL2D:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_AVG_PERIOD_CMD, val);
                }
                else if (aname == "heating") {
                    if (upperAval == "TRUE" || upperAval == "YES" || upperAval == "ENABLE" || aval == "1") {
                    	updateDesiredScienceParameter(SENSOR_HEATING_CMD, ACTIVE_H3);
                    }
                    else if (upperAval == "FALSE" || upperAval == "NO" || upperAval == "NONE"
                    		 || upperAval == "NA" || upperAval == "DISABLE" || aval == "0") {
                    	updateDesiredScienceParameter(SENSOR_HEATING_CMD, DISABLED);
                    }
                    else
                        throw n_u::InvalidParameterException(
                            string("GILL2D:") + getName(), aname, aval);
                }
                else if (aname == "nmeastring") {
                    if (upperAval.find("IIMWV")) {
                    	updateDesiredScienceParameter(SENSOR_NMEA_ID_STR_CMD, IIMWV);
                    }
                    else if (upperAval.find("WIMWV")) {
                    	updateDesiredScienceParameter(SENSOR_NMEA_ID_STR_CMD, WIMWV);
                    }
                    else
                        throw n_u::InvalidParameterException(
                            string("GILL2D:") + getName(), aname, aval);
                }
                else if (aname == "msgterm") {
                	if (upperAval.find("CR") || upperAval.find("CRLF") || upperAval.find("CR LF")) {
						updateDesiredScienceParameter(SENSOR_MSG_TERM_CMD, CRLF);
                	}
                	else if (!upperAval.find("CR") && upperAval.find("LF")) {
                		updateDesiredScienceParameter(SENSOR_MSG_TERM_CMD, LF);
                	}
                	else
                        throw n_u::InvalidParameterException(
                            string("GILL2D:") + getName(), aname, aval);
                }
                else if (aname == "stream") {
                	if (upperAval.find("ASC")) {
                		if (upperAval.find("UV")) {
                			if (upperAval.find("CONT")) {
                        		updateDesiredScienceParameter(SENSOR_MSG_STREAM_CMD, ASC_UV_CONT);
                			}
                			else if (upperAval.find("POLL")) {
                        		updateDesiredScienceParameter(SENSOR_MSG_STREAM_CMD, ASC_PLR_CONT);
                			}
                			else
                                throw n_u::InvalidParameterException(
                                    string("GILL2D:") + getName(), aname, aval);
                		}
                		if (upperAval.find("POLAR") || upperAval.find("PLR")) {
                			if (upperAval.find("CONT")) {
                        		updateDesiredScienceParameter(SENSOR_MSG_STREAM_CMD, ASC_UV_CONT);
                			}
                			else if (upperAval.find("POLL")) {
                        		updateDesiredScienceParameter(SENSOR_MSG_STREAM_CMD, ASC_PLR_CONT);
                			}
                			else
                                throw n_u::InvalidParameterException(
                                    string("GILL2D:") + getName(), aname, aval);
                		}
                	}
                	else if (upperAval.find("NMEA")) {
                		updateDesiredScienceParameter(SENSOR_MSG_STREAM_CMD, NMEA_CONT);
                	}
                	else
                        throw n_u::InvalidParameterException(
                            string("GILL2D:") + getName(), aname, aval);
                }
                else if (aname == "outputformat") {
                	if (upperAval == "CSV") {
                		updateDesiredScienceParameter(SENSOR_OUTPUT_FIELD_FMT_CMD, CSV);
                	}
                	else if (upperAval == "FIX" || upperAval == "FIXED") {
                		updateDesiredScienceParameter(SENSOR_OUTPUT_FIELD_FMT_CMD, FIXED_FIELD);
                	}
                	else
                        throw n_u::InvalidParameterException(
                            string("GILL2D:") + getName(), aname, aval);
                }
                else if (aname == "outputrate") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || val < ONE_PER_SEC || val > FOUR_PER_SEC)
                        throw n_u::InvalidParameterException(
                            string("GILL2D:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_OUTPUT_RATE_CMD, val);
                }
                else if (aname == "vertpad") {
                    if (upperAval == "TRUE" || upperAval == "YES" || aval == "1") {
                    	updateDesiredScienceParameter(SENSOR_VERT_MEAS_PADDING_CMD, ENABLE_VERT_PAD);
                    }
                    else if (upperAval == "FALSE" || upperAval == "NO" || upperAval == "NONE"
                    		 || upperAval == "NA" || aval == "0") {
                    	updateDesiredScienceParameter(SENSOR_VERT_MEAS_PADDING_CMD, DISABLE_VERT_PAD);
                    }
                    else
                        throw n_u::InvalidParameterException(
                            string("GILL2D:") + getName(), aname, aval);
                }
                else if (aname == "porttype") {
                    if (upperAval == "RS422")
                        desiredPortConfig.xcvrConfig.portType = RS422;
                    else if (upperAval == "RS485_HALF") 
                        desiredPortConfig.xcvrConfig.portType = RS485_HALF;
                    else if (upperAval == "RS485_FULL") 
                        desiredPortConfig.xcvrConfig.portType = RS485_FULL;
                    else
                        throw n_u::InvalidParameterException(
                            string("GILL2D:") + getName(), aname, aval);
                }
                else if (aname == "baud") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || !desiredPortConfig.termios.setBaudRate(val))
                        throw n_u::InvalidParameterException(
                            string("GILL2D:") + getName(), aname,aval);
                }
                else if (aname == "parity") {
                    if (upperAval == "ODD") 
                        desiredPortConfig.termios.setParity(n_u::Termios::ODD);
                    else if (upperAval == "EVEN") 
                        desiredPortConfig.termios.setParity(n_u::Termios::EVEN);
                    else if (upperAval == "NONE") 
                        desiredPortConfig.termios.setParity(n_u::Termios::NONE);
                    else throw n_u::InvalidParameterException(
                        string("GILL2D:") + getName(),
                        aname,aval);

                    // These are the only legal values for databits and stopbits,  
                    // so force them.
                    desiredPortConfig.termios.setDataBits(8);
                    desiredPortConfig.termios.setStopBits(1);

                }
                // TODO: May want to remove this for the new boards???
                else if (aname == "rts485") {
                    if (upperAval == "TRUE" || aval == "1") {
                        desiredPortConfig.rts485 = 1;
                    }
                    else if (upperAval == "NONE" || upperAval == "NA" || aval == "0") {
                        desiredPortConfig.rts485 = 0;
                    }
                    else if (upperAval == "FALSE" || aval == "-1") {
                        desiredPortConfig.rts485 = -1;
                    }
                    else {
                        throw n_u::InvalidParameterException(
                        string("GILL2D:") + getName(),
                            aname, aval);
                    }
                }
            }
        }
    }
}

void GILL2D::open(int flags) throw (n_u::IOException, n_u::InvalidParameterException)
{
    NLOG(("First figure out whether we're talking to the sensor"));
    if (findWorkingSerialPortConfig(flags)) {
        NLOG(("Found working sensor serial port configuration"));
        NLOG((""));
        NLOG(("Attempting to install the desired sensor serial parameter configuration"));
        if (installDesiredSensorConfig()) {
            NLOG(("Desired sensor serial port configuration successfully installed"));
            NLOG((""));
            NLOG(("Attempting to install the desired sensor science configuration"));
            if (configureScienceParameters()) {
                NLOG(("Desired sensor science configuration successfully installed"));
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
        NLOG(("Couldn't find a serial port configuration that worked with this GILL2D sensor. "
              "May need to troubleshoot the sensor or cable. "
              "!!!NOTE: Sensor is not open for data collection!!!"));
    }
}

bool GILL2D::findWorkingSerialPortConfig(int flags)
{
    bool foundIt = false;

    // first see if the current configuration is working. If so, all done!
    // So open the device at a the base class so we don't recurse ourselves to death...
    DSMSensor::open(flags);
    n_c::SerialPortIODevice* pSIODevice = dynamic_cast<n_c::SerialPortIODevice*>(getIODevice());
    applyPortConfig();
    // Make sure blocking is set properly
    pSIODevice->getBlocking();
    // Save off desiredConfig - base class should have modified it by now.
    // Do this after applying, as getPortConfig() only gets the items in the SerialPortIODevice object.
    desiredPortConfig = getPortConfig();

    // check the raw mode parameters
    VLOG(("Raw mode is ") << (desiredPortConfig.termios.getRaw() ? "ON" : "OFF"));

    if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
        NLOG(("Testing initial config which may be custom "));
        printPortConfig();
    }

    GILL2D_CFG_MODE_STATUS cfgModeStatus = enterConfigMode();
    if (cfgModeStatus == NOT_ENTERED || (cfgModeStatus == ENTERED && !doubleCheckResponse())) {
        // initial config didn't work, so sweep through all parameters starting w/the default
        if (!isDefaultConfig(getPortConfig())) {
            // it's a custom config, so test default first
            if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                NLOG(("This is the failed port config: "));
                printPortConfig();
            }
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
            NLOG(("SerialSensor customized the default PortConfig and it succeeded!!"));
        }
        else {
            NLOG(("SerialSensor did not customize the default PortConfig and it succeeded!!"));
        }

        foundIt = true;
        if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
            printPortConfig();
        }
    }

    return foundIt;
}

bool GILL2D::installDesiredSensorConfig()
{
    bool installed = false;
    PortConfig sensorPortConfig = getPortConfig();

    // at this point we need to determine whether or not the current working config 
    // is the desired config, and adjust as necessary
    if (desiredPortConfig != sensorPortConfig) {
        // Gotta modify the GILL2D parameters first, and the modify our parameters to match and hope for the best.
        // We only do this for the serial and science parameters, as the sensor is physically configured to use  
        // the transceiver mode we discovered it works on. To change these parameters, the user would have to  
        // physically reconfigure the sensor and re-start the auto-config process.
        DLOG(("Attempting to set the serial configuration to the desired configuration."));

        serPortFlush(O_RDWR);

        if (desiredPortConfig.termios.getBaudRate() != sensorPortConfig.termios.getBaudRate()) {
        	DLOG(("Changing baud rate to: ") << desiredPortConfig.termios.getBaudRate());
        	GILL2D_BAUD_ARGS newBaudArg = G38400;
        	switch (desiredPortConfig.termios.getBaudRate()) {
				case 38400:
					break;
				case 19200:
					newBaudArg = G19200;
					break;
				case 9600:
					newBaudArg = G9600;
					break;
				case 4800:
					newBaudArg = G4800;
					break;
				case 2400:
					newBaudArg = G2400;
					break;
				case 1200:
					newBaudArg = G1200;
					break;
				case 300:
					newBaudArg = G300;
					break;
        	}

        	sendSensorCmd(SENSOR_SERIAL_BAUD_CMD, newBaudArg);
        }

        if (desiredPortConfig.termios.getParity() | sensorPortConfig.termios.getParity()) {
        	DLOG(("Changing parity to: ") << desiredPortConfig.termios.getParityString());
			// GILL2D only supports three combinations of word format - all based on parity
			// So just force it based on parity.
			switch (desiredPortConfig.termios.getParityString(true).c_str()[0]) {
				case 'O':
					sendSensorCmd(SENSOR_SERIAL_DATA_WORD_CMD, O81);
					break;

				case 'E':
					sendSensorCmd(SENSOR_SERIAL_DATA_WORD_CMD, E81);
					break;

				case 'N':
					sendSensorCmd(SENSOR_SERIAL_DATA_WORD_CMD, N81);
					break;

				default:
					break;
			}
        }

        setPortConfig(desiredPortConfig);
        applyPortConfig();
        if (getPortConfig() == desiredPortConfig) {
            if (!doubleCheckResponse()) {
                if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                    NLOG(("GILL2D::installDesiredSensorConfig() failed to achieve sensor communication "
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
                    DLOG(("GILL2D reset to original!!!"));
                    printPortConfig();
                }
            }
            else {
                if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                    NLOG(("Success!! GILL2D set to desired configuration!!!"));
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

bool GILL2D::configureScienceParameters()
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

void GILL2D::sendScienceParameters() {
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
}

bool GILL2D::checkScienceParameters()
{
	bool scienceParametersOK = false;

    VLOG(("GILL2D::checkScienceParameters() - Flush port and send query command"));
    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    sendSensorCmd(SENSOR_DIAG_QRY_CMD, OPER_CONFIG);

    static const int BUF_SIZE = 512;
    int bufRemaining = BUF_SIZE;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    VLOG(("GILL2D::checkScienceParameters() - Read the entire response"));
    int numCharsRead = readEntireResponse(&(respBuf[0]), bufRemaining, 2000);


    if (numCharsRead ) {
    	if (LOG_LEVEL_IS_ACTIVE(LOGGER_VERBOSE)) {
			std::string respStr;
			respStr.append(&respBuf[0], numCharsRead);

			VLOG(("Response: "));
			VLOG((respStr.c_str()));
    	}

		VLOG(("GILL2D::checkScienceParameters() - Check the individual parameters available to us"));
		int regexStatus = -1;
		int nmatch = compareScienceResponse.re_nsub+1;
		regmatch_t matches[nmatch];

		VLOG(("Expecting nmatch matches: ") << nmatch);

		// check for sample averaging
		if ((regexStatus = regexec(&compareScienceResponse, &(respBuf[0]), nmatch, matches, 0)) == 0) {
			if (matches[7].rm_so >= 0) {
				string argStr = std::string(&(respBuf[matches[7].rm_so]), (matches[7].rm_eo - matches[7].rm_so));
				VLOG(("Checking sample averaging time(G) with argument: ") << argStr);
				scienceParametersOK = compareScienceParameter(SENSOR_AVG_PERIOD_CMD, argStr.c_str());
			}

			if (scienceParametersOK && matches[8].rm_so >= 0) {
				string argStr = std::string(&(respBuf[matches[8].rm_so]), (matches[8].rm_eo - matches[8].rm_so));
				VLOG(("Checking heater status(H) with argument: ") << argStr);
				scienceParametersOK = compareScienceParameter(SENSOR_HEATING_CMD, argStr.c_str());
			}

			if (scienceParametersOK && matches[10].rm_so >= 0) {
				string argStr = std::string(&(respBuf[matches[10].rm_so]), (matches[10].rm_eo - matches[10].rm_so));
				VLOG(("Checking NMEA string(K) with argument: ") << argStr);
				scienceParametersOK = compareScienceParameter(SENSOR_NMEA_ID_STR_CMD, argStr.c_str());
			}

			if (scienceParametersOK && matches[11].rm_so >= 0) {
				string argStr = std::string(&(respBuf[matches[11].rm_so]), (matches[11].rm_eo - matches[11].rm_so));
				VLOG(("Checking message termination(L) with argument: ") << argStr);
				scienceParametersOK = compareScienceParameter(SENSOR_MSG_TERM_CMD, argStr.c_str());
			}

			if (scienceParametersOK && matches[12].rm_so >= 0) {
				string argStr = std::string(&(respBuf[matches[12].rm_so]), (matches[12].rm_eo - matches[12].rm_so));
				VLOG(("Checking message stream format(M) with argument: ") << argStr);
				scienceParametersOK = compareScienceParameter(SENSOR_MSG_STREAM_CMD, argStr.c_str());
			}

			if (scienceParametersOK && matches[13].rm_so >= 0) {
				string argStr = std::string(&(respBuf[matches[13].rm_so]), (matches[13].rm_eo - matches[13].rm_so));
				VLOG(("Checking node address(N) with argument: ") << argStr);
				scienceParametersOK = compareScienceParameter(SENSOR_NODE_ADDR_CMD, argStr.c_str());
			}

			if (scienceParametersOK && matches[14].rm_so >= 0) {
				string argStr = std::string(&(respBuf[matches[14].rm_so]), (matches[14].rm_eo - matches[14].rm_so));
				VLOG(("Checking output field format(O) with argument: ") << argStr);
				scienceParametersOK = compareScienceParameter(SENSOR_OUTPUT_FIELD_FMT_CMD, argStr.c_str());
			}

			if (scienceParametersOK && matches[15].rm_so >= 0) {
				string argStr = std::string(&(respBuf[matches[15].rm_so]), (matches[15].rm_eo - matches[15].rm_so));
				VLOG(("Checking output rate(P) with argument: ") << argStr);
				scienceParametersOK = compareScienceParameter(SENSOR_OUTPUT_RATE_CMD, argStr.c_str());
			}

			if (scienceParametersOK && matches[17].rm_so >= 0) {
				string argStr = std::string(&(respBuf[matches[17].rm_so]), (matches[17].rm_eo - matches[17].rm_so));
				VLOG(("Checking wind speed units(U) with argument: ") << argStr);
				scienceParametersOK = compareScienceParameter(SENSOR_MEAS_UNITS_CMD, argStr.c_str());
			}

			if (scienceParametersOK && matches[18].rm_so >= 0) {
				string argStr = std::string(&(respBuf[matches[18].rm_so]), (matches[18].rm_eo - matches[18].rm_so));
				VLOG(("Checking vertical output pad(V) with argument: ") << argStr);
				scienceParametersOK = compareScienceParameter(SENSOR_MEAS_UNITS_CMD, argStr.c_str());
			}

			if (scienceParametersOK && matches[19].rm_so >= 0) {
				string argStr = std::string(&(respBuf[matches[19].rm_so]), (matches[19].rm_eo - matches[19].rm_so));
				VLOG(("Checking sensor alignment(X) with argument: ") << argStr);
				scienceParametersOK = compareScienceParameter(SENSOR_ALIGNMENT_CMD, argStr.c_str());
			}
		}
	    else {
	        char regerrbuf[64];
	        ::regerror(regexStatus, &compareScienceResponse, regerrbuf, sizeof regerrbuf);
	        throw n_u::ParseException("regexec average samples RE", string(regerrbuf));
	    }
    }
    else
    	DLOG(("No characters returned from serial port"));

    return scienceParametersOK;
}

bool GILL2D::compareScienceParameter(GILL2D_COMMANDS cmd, const char* match)
{
    GILL2D_CMD_ARG desiredCmd = getDesiredCmd(cmd);
    VLOG(("Desired command: ") << desiredCmd.cmd);
    VLOG(("Searched command: ") << cmd);

	int arg;
	std::stringstream argStrm(match);
	argStrm >> arg;

	VLOG(("Arguments match: ") << (desiredCmd.arg == arg ? "TRUE" : "FALSE"));
	return (desiredCmd.arg == arg);
}

GILL2D_CMD_ARG GILL2D::getDesiredCmd(GILL2D_COMMANDS cmd) {
    VLOG(("Looking in desiredScienceParameters[] for ") << cmd);
    for (int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if (desiredScienceParameters[i].cmd == cmd) {
            VLOG(("Found command: ") << cmd);
            return desiredScienceParameters[i];
        }
    }

    VLOG(("Requested cmd not found: ") << cmd);

    GILL2D_CMD_ARG nullRetVal = {NULL_COMMAND, 0};
    return(nullRetVal);
}


bool GILL2D::testDefaultPortConfig()
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

bool GILL2D::sweepParameters(bool defaultTested)
{
    bool foundIt = false;

    for (int i=0; i<NUM_PORT_TYPES; ++i) {
        int rts485 = 0;
        n_c::PORT_TYPES portType = SENSOR_PORT_TYPES[i];

        if (portType == n_c::RS485_HALF)
            rts485 = -1; // ??? TODO check this out: start low. Let write manage setting high
        else if (portType == n_c::RS422)
            rts485 = -1; // always high, since there are two drivers going both ways

        for (int j=0; j<NUM_BAUD_ARGS; ++j) {
            int baud = SENSOR_BAUDS[j];

            for (int k=0; k<NUM_DATA_WORD_ARGS; ++k) {
                Termios::parity parity = (SENSOR_WORD_SPECS[k] == N81) ? Termios::NONE : (SENSOR_WORD_SPECS[k] == E81) ? Termios::EVEN : Termios::ODD;

                // get the existing port config to preserve the port
                // which only gets set on construction
                testPortConfig = getPortConfig();

                // now set it to the new parameters
                setTargetPortConfig(testPortConfig, baud, 8, parity, 1,
                                                    rts485, portType, NO_TERM,
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

                GILL2D_CFG_MODE_STATUS cfgModeStatus = enterConfigMode();
                if (cfgModeStatus == ENTERED_RESP_CHECKED || (cfgModeStatus == ENTERED && doubleCheckResponse())) {
                    foundIt = true;
                    return foundIt;
                } 
                else {
                    if (portType == n_c::RS485_HALF || portType == n_c::RS422) {
                        DLOG(("If 422/485, one more try - test the connection w/termination turned on."));
                        setTargetPortConfig(testPortConfig, baud, 8, parity, 1,
															rts485, portType, TERM_120_OHM,
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

                        if (cfgModeStatus == ENTERED_RESP_CHECKED || (cfgModeStatus == ENTERED && doubleCheckResponse())) {
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

void GILL2D::setTargetPortConfig(PortConfig& target, int baud, int dataBits, Termios::parity parity, int stopBits, 
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

bool GILL2D::isDefaultConfig(const n_c::PortConfig& target)
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

bool GILL2D::doubleCheckResponse()
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

bool GILL2D::checkResponse()
{
	DLOG(("GILL2D::checkResponse(): enter..."));
	bool retVal = false;

    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    // Just getting into config mode elicits a usable response...
    NLOG(("Sending GILL command to get current config..."));
    sendSensorCmd(SENSOR_DIAG_QRY_CMD, OPER_CONFIG);

    static const int BUF_SIZE = 512;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    int numCharsRead = readEntireResponse(&(respBuf[0]), BUF_SIZE, 2000);

    if (numCharsRead) {
        std::string respStr(&respBuf[0], numCharsRead);

        DLOG(("Response: "));
        DLOG((respStr.c_str()));

        // This is where the response is checked for signature elements
        VLOG(("GILL2D::checkResponse() - Check the general format of the config mode response"));
        int regexStatus = -1;
        
        // check for sample averaging
        retVal = ((regexStatus = regexec(&sensorCfgResponse, &(respBuf[0]), 0, 0, 0)) == 0);

        if (regexStatus > 0) {
            char regerrbuf[64];
            ::regerror(regexStatus, &sensorCfgResponse, regerrbuf, sizeof regerrbuf);
            DLOG(("GILL2D::checkResponse() regex failed: ") << std::string(regerrbuf));
        }
    }

    else {
        DLOG(("Didn't get any chars from serial port"));
    }

	DLOG(("GILL2D::checkResponse(): exit..."));
    return retVal;
}


void GILL2D::sendSensorCmd(GILL2D_COMMANDS cmd, int arg)
{
    assert(cmd < NUM_SENSOR_CMDS);
    std::string snsrCmd(cmdTable[cmd]);
	std::ostringstream argStr;

    // Most GIL commands take character args starting at '1'.
    // Some do not, so if a value < 0 is passed in, skip this part.
    if (arg >= 0) {
    	if (cmd == SENSOR_AVG_PERIOD_CMD) {
        	// requires at least 4 numeric characters, 0 padded.
        	char buf[5];
        	snprintf(buf, 5, "%04d", arg);
        	argStr << std::string(buf, 5);
        }
    	else if (arg >= 1) {
    		if (cmd == SENSOR_CONFIG_MODE_CMD) {
				argStr << (char)arg;
			}
			else {
				argStr << arg;
			}
    	}

        DLOG(("Attempting to insert: ") << arg);
    	int insertIdx = snsrCmd.find_last_of('\r');
    	DLOG(("Found \\r at position: ") << insertIdx);
    	snsrCmd.insert(insertIdx, argStr.str());
    	// add the \r at the beginning to help some GILL states get unstuck...
    	// For instance, when changing serial port settings while in configuration mode,
    	// a carriage return is necessary before it will respond to commands.
    	snsrCmd.insert(0, "\r");
    }

    // Write the command - assume the port is already open
    // The  seems to not be able to keep up with a burst of data, so
    // give it some time between chars - i.e. ~80 words/min rate
    DLOG(("Sending command: "));
    DLOG((snsrCmd.c_str()));
    struct timespec writeWait = {0, CHAR_WRITE_DELAY};
    for (unsigned int i=0; i<snsrCmd.length(); ++i) {
        write(&(snsrCmd.c_str()[i]), 1);
        nanosleep(&writeWait, 0);
    }
    DLOG(("write() sent ") << snsrCmd.length());;

    if (isConfigCmd(cmd) || cmd == SENSOR_QRY_ID_CMD) {
    	const int CMD_RESP_BUF_SIZE = 20;
		char cmdRespBuf[CMD_RESP_BUF_SIZE];
		memset(cmdRespBuf, 0, CMD_RESP_BUF_SIZE);
		int numCharsRead = readEntireResponse(cmdRespBuf, CMD_RESP_BUF_SIZE, 2000);
		std::string respStr(cmdRespBuf, numCharsRead);
		std::ostringstream oss;
		oss << "Sent: " << snsrCmd << std::endl << " Received: " << std::hex << respStr;
		DLOG((oss.str().c_str()));
		if (cmd == SENSOR_QRY_ID_CMD) {
			unsigned stxPos = respStr.find_first_of('\x02');
			unsigned etxPos = respStr.find_first_of('\x03', stxPos);
			if (etxPos != string::npos && (etxPos - stxPos) == 2) {
				// we can probably believe that we have captured the unit address response
				unitId = respStr[stxPos+1];
			}
		}
		else if (cmd == SENSOR_SERIAL_BAUD_CMD || cmd == SENSOR_SERIAL_DATA_WORD_CMD) {
			DLOG(("Serial port setting change... Looking for confirm request response..."));
			if (respStr.find("confirm") != std::string::npos) {
				DLOG(("Serial port setting change... Found confirm request response, changing termios to new parameters and sending confirmation..."));
				if (confirmGillSerialPortChange(cmd, arg)) {
					DLOG(("Serial port setting confirmed: ") << cmdTable[cmd]);
				}
				else
					DLOG(("Serial port setting NOT confirmed: ") << cmdTable[cmd]);
			}
			else {
				DLOG(("Serial port setting change... Didn't find confirm request response - retrying read..."));
				memset(cmdRespBuf, 0, CMD_RESP_BUF_SIZE);
				numCharsRead = readEntireResponse(cmdRespBuf, CMD_RESP_BUF_SIZE, 2000);
				if (numCharsRead) {
					if (respStr.find("confirm") != std::string::npos) {
						DLOG(("Serial port setting change... Found confirm request response, changing termios to new parameters and sending confirmation..."));
						if (confirmGillSerialPortChange(cmd, arg)) {
							DLOG(("Serial port setting confirmed: ") << cmdTable[cmd]);
						}
						else
							DLOG(("Serial port setting NOT confirmed: ") << cmdTable[cmd]);
					}
					else
						DLOG(("Serial port setting change... Didn't find confirm request response - failed twice"));
				}
				else
					DLOG(("Serial port setting change... Didn't find confirm request response - failed twice"));
			}
		}
    }
}

bool GILL2D::confirmGillSerialPortChange(GILL2D_COMMANDS cmd, int arg)
{
	DLOG(("confirmGillSerialPortChange(): enter"));
	ostringstream entireCmd(cmdTable[cmd]);
	entireCmd << arg;

	std::string confirmCmd(cmdTable[cmd]);
	confirmCmd.append("\r");
	confirmCmd.insert(0, "\r");
	DLOG(("Serial port confirmation command: ") << confirmCmd);

	DLOG(("Getting the existing PortConfig and adjusting Termios..."));
	PortConfig currentPortConfig = getPortConfig();

	// Adjust PortConfig/Termios and apply
	if (cmd == SENSOR_SERIAL_BAUD_CMD) {
		DLOG(("Setting the new serial baud rate command..."));
		switch (static_cast<GILL2D_BAUD_ARGS>(arg)) {
			case G300:
				currentPortConfig.termios.setBaudRate(300);
				break;
			case G1200:
				currentPortConfig.termios.setBaudRate(1200);
				break;
			case G2400:
				currentPortConfig.termios.setBaudRate(2400);
				break;
			case G4800:
				currentPortConfig.termios.setBaudRate(4800);
				break;
			case G9600:
				currentPortConfig.termios.setBaudRate(9600);
				break;
			case G19200:
				currentPortConfig.termios.setBaudRate(19200);
				break;
			case G38400:
				currentPortConfig.termios.setBaudRate(38400);
				break;
		}
	}
	else if(cmd == SENSOR_SERIAL_DATA_WORD_CMD) {
		DLOG(("Setting the new serial word command..."));
		switch (static_cast<GILL2D_DATA_WORD_ARGS>(arg)) {
			case N81:
				currentPortConfig.termios.setParity(Termios::NONE);
				break;
			case E81:
				currentPortConfig.termios.setParity(Termios::EVEN);
				break;
			case O81:
				currentPortConfig.termios.setParity(Termios::ODD);
				break;
		}
	}

	DLOG(("Applying the new PortConfig settings..."));
	setPortConfig(currentPortConfig);
	applyPortConfig();

	DLOG(("Writing the confirmation command..."));
	write(confirmCmd.c_str(), 3);

	DLOG(("Reading the confirmation response..."));
	char confirmRespBuf[10];
	memset(confirmRespBuf, 0, 10);
	size_t numRespChars = readEntireResponse(confirmRespBuf, 10, 2000);
	std::string respStr(confirmRespBuf, numRespChars);
	DLOG(("Confirmation Response: ") << respStr);
	DLOG(("confirmGillSerialPortChange(): exit"));

	return 	(respStr.find(entireCmd.str()) != std::string::npos);
}

size_t GILL2D::readResponse(void *buf, size_t len, int msecTimeout)
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

size_t GILL2D::readEntireResponse(void *buf, size_t len, int msecTimeout)
{
	char* cbuf = (char*)buf;
	int bufRemaining = len;
    int numCharsRead = readResponse(cbuf, len, msecTimeout);
    int totalCharsRead = numCharsRead;
    bufRemaining -= numCharsRead;

    if (LOG_LEVEL_IS_ACTIVE(LOGGER_VERBOSE)) {
        if (numCharsRead > 0) {
        	IosFlagSaver iosFlagRestorer(std::cout);
            VLOG(("Initial num chars read is: ") << numCharsRead << " comprised of: ");
            for (size_t i=0; i<5; ++i) {
                for (int j=0; j<10; ++j) {
                	if ((i*10 + j) > len)
						break;
                    std::cout << std::setw(5) << std::hex << (unsigned char)cbuf[(i*10)+j];
                }
                std::cout << endl << std::flush;
//                VLOG((&hexBuf[0]));
            }
        }
    }

    for (int i=0; (numCharsRead > 0 && bufRemaining > 0); ++i) {
        numCharsRead = readResponse(&cbuf[totalCharsRead], bufRemaining, msecTimeout);
        totalCharsRead += numCharsRead;
        bufRemaining -= numCharsRead;

        if (LOG_LEVEL_IS_ACTIVE(LOGGER_VERBOSE)) {
            if (numCharsRead == 0) {
                VLOG(("Took ") << i+1 << " reads to get entire response");
            }
        }
    }

    return totalCharsRead;
}

void GILL2D::updateDesiredScienceParameter(GILL2D_COMMANDS cmd, int arg) {
    for(int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if (cmd == desiredScienceParameters[i].cmd) {
            desiredScienceParameters[i].arg = arg;
            break;
        }
    }
}

bool GILL2D::checkConfigMode(bool continuous)
{
	bool retVal = false;
    static const int BUF_SIZE = 75;
    int bufRemaining = BUF_SIZE;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);
    int numCharsRead = 0;

    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    // Just getting into config mode elicits a usable response...
    if (continuous) {
		DLOG(("Sending GILL command to enter configuration mode in continuous operation..."));
		sendSensorCmd(SENSOR_CONFIG_MODE_CMD, -1);
    }
    else {
    	if (!unitId) {
    		DLOG(("Attempting to get unit ID"));
    		sendSensorCmd(SENSOR_QRY_ID_CMD, -1);
    	}

    	if (unitId) {
			DLOG(("Sending polled mode command to enter config mode"));
			sendSensorCmd(SENSOR_CONFIG_MODE_CMD, unitId);
    	}
    	else
    		DLOG(("Didn't get the unit ID. Must not be in polled mode, or bad serial port config."));
    }

    bufRemaining = BUF_SIZE;
    memset(respBuf, 0, BUF_SIZE);

    numCharsRead = readEntireResponse(&(respBuf[0]), bufRemaining, 2000);

    if (numCharsRead) {
        std::string respStr;
        respStr.append(&respBuf[0], numCharsRead);

        DLOG(("Response: "));
        DLOG((respStr.c_str()));

        // This is where the response is checked for signature elements
        VLOG(("GILL2D::checkConfigMode() - Check the general format of the config mode response"));
        int regexStatus = -1;

        // check for sample averaging
        retVal = ((regexStatus = regexec(&configModeResponse, &(respBuf[0]), 0, 0, 0)) == 0);

        if (regexStatus > 0) {
            char regerrbuf[64];
            ::regerror(regexStatus, &configModeResponse, regerrbuf, sizeof regerrbuf);
            DLOG(("GILL2D::checkConfigMode() regex failed: ") << std::string(regerrbuf));
        }
    }

    else {
        DLOG(("Didn't get any chars from serial port"));
    }

    return retVal;
}

GILL2D_CFG_MODE_STATUS GILL2D::enterConfigMode()
{
	DLOG(("GILL2D::enterConfigMode(): enter..."));
	GILL2D_CFG_MODE_STATUS retVal = NOT_ENTERED;

	DLOG(("Trying to get into Configuration Mode"));
	DLOG(("First check to see if sensor is in continous output..."));
	if (!checkConfigMode()) {
		DLOG(("Must not be in continuous mode, or serial port not set up right"));
		if (!checkConfigMode(POLLED)) {
			DLOG(("Must not be in polled mode, or serial port not set up right"));
			if (checkResponse()) {
				retVal = ENTERED_RESP_CHECKED;
				DLOG(("Sensor is already in config mode, response has been checked and succeeded"));
			}
			else
				DLOG(("Must not be in config mode, or serial port not set up right"));
		}
		else {
			DLOG(("Entered config mode while in polled mode; diag response not checked."));
			retVal = ENTERED;
		}
	}
	else {
		DLOG(("Entered config mode while in continous mode; diag response not checked."));
		retVal = ENTERED;
	}

	DLOG(("GILL2D::enterConfigMode(): exit..."));
	return retVal;
}

}}} //namespace nidas { namespace dynld { namespace isff {

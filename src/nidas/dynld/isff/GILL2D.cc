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
const n_c::WordSpec GILL2D::SENSOR_WORD_SPECS[NUM_DATA_WORD_ARGS] =
{
	{8, Termios::NONE, 1},
	{8, Termios::EVEN, 1},
	{8, Termios::ODD, 1},
};

// GIL instruments do not use RS232
const n_c::PORT_TYPES GILL2D::SENSOR_PORT_TYPES[GILL2D::NUM_PORT_TYPES] = {n_c::RS422, n_c::RS485_HALF };


// static default configuration to send to base class...
const PortConfig GILL2D::DEFAULT_PORT_CONFIG(GILL2D::DEFAULT_BAUD_RATE, GILL2D::DEFAULT_DATA_BITS,
                                             GILL2D::DEFAULT_PARITY, GILL2D::DEFAULT_STOP_BITS,
                                             GILL2D::DEFAULT_PORT_TYPE, GILL2D::DEFAULT_SENSOR_TERMINATION, 
											 GILL2D::DEFAULT_RTS485, GILL2D::DEFAULT_CONFIG_APPLIED);

const n_c::SensorCmdData GILL2D::DEFAULT_SCIENCE_PARAMETERS[] =
{
    n_c::SensorCmdData(SENSOR_AVG_PERIOD_CMD, n_c::SensorCmdArg(0)),
    n_c::SensorCmdData(SENSOR_HEATING_CMD, n_c::SensorCmdArg(DISABLED)),
    n_c::SensorCmdData(SENSOR_NMEA_ID_STR_CMD, n_c::SensorCmdArg(IIMWV)),
    n_c::SensorCmdData(SENSOR_MSG_TERM_CMD, n_c::SensorCmdArg(CRLF)),
    n_c::SensorCmdData(SENSOR_MSG_STREAM_CMD, n_c::SensorCmdArg(ASC_PLR_CONT)),
	n_c::SensorCmdData(SENSOR_OUTPUT_FIELD_FMT_CMD, n_c::SensorCmdArg(CSV)),
	n_c::SensorCmdData(SENSOR_OUTPUT_RATE_CMD, n_c::SensorCmdArg(ONE_PER_SEC)),
	n_c::SensorCmdData(SENSOR_MEAS_UNITS_CMD, n_c::SensorCmdArg(MPS)),
	n_c::SensorCmdData(SENSOR_VERT_MEAS_PADDING_CMD, n_c::SensorCmdArg(DISABLE_VERT_PAD)),
	n_c::SensorCmdData(SENSOR_ALIGNMENT_CMD, n_c::SensorCmdArg(U_EQ_NS))
};

const int GILL2D::NUM_DEFAULT_SCIENCE_PARAMETERS = sizeof(DEFAULT_SCIENCE_PARAMETERS)/sizeof(n_c::SensorCmdData);

/* Typical GILL2D D3 query response. L1 means all line endings are \r\n
 * 
 * This is actually the factory default settings. For most WindObserver models, 
 * which this class implements, A, C, T, Y and Z are not settable.
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
	  _desiredPortConfig(DEFAULT_PORT_CONFIG),
      _defaultMessageConfig(DEFAULT_MESSAGE_LENGTH, DEFAULT_MSG_SEP_CHAR, DEFAULT_MSG_SEP_EOM),
      _desiredScienceParameters(), _unitId('\0'), _polling(false)
{
    // We set the defaults at construction, 
    // letting the base class modify according to fromDOMElement() 
    setMessageParameters(_defaultMessageConfig);

    // Let the base class know about PTB210 RS232 limitations
    for (int i=0; i<NUM_PORT_TYPES; ++i) {
    	_portTypeList.push_back(SENSOR_PORT_TYPES[i]);
    }

    for (int i=0; i<NUM_BAUD_ARGS; ++i) {
    	_baudRateList.push_back(SENSOR_BAUDS[i]);
    }

    for (int i=0; i<NUM_DATA_WORD_ARGS; ++i) {
    	_serialWordSpecList.push_back(SENSOR_WORD_SPECS[i]);
    }

    _desiredScienceParameters = new n_c::SensorCmdData[NUM_DEFAULT_SCIENCE_PARAMETERS];
    for (int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        _desiredScienceParameters[i] = DEFAULT_SCIENCE_PARAMETERS[i];
    }

    compileRegex();
}

GILL2D::~GILL2D()
{
    freeRegex();
    delete [] _desiredScienceParameters;
}

void GILL2D::fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException)
{
	NLOG(("GILL2D - checking for sensor customizations in the DSM/Sensor Catalog XML..."));
	DLOG(("GILL2D::fromDOMElement() - entry"));
    // let the base classes have first shot at it, since we only care about an autoconfig child element
    // however, any duplicate items in autoconfig will override any items in the base classes
    Wind2D::fromDOMElement(node);

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
        	DLOG(("Found the <autoconfig /> tag..."));
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
                 DLOG(("GILL2D:fromDOMElement(): attribute: ") << aname << " : " << upperAval);

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
            }
        }
    }

	DLOG(("GILL2D::fromDOMElement() - exit"));
}

bool GILL2D::installDesiredSensorConfig()
{
    bool installed = false;
    PortConfig sensorPortConfig = getPortConfig();

    // at this point we need to determine whether or not the current working config 
    // is the desired config, and adjust as necessary
    if (_desiredPortConfig != sensorPortConfig) {
        // Gotta modify the GILL2D parameters first, and the modify our parameters to match and hope for the best.
        // We only do this for the serial and science parameters, as the sensor is physically configured to use  
        // the transceiver mode we discovered it works on. To change these parameters, the user would have to  
        // physically reconfigure the sensor and re-start the auto-config process.
        DLOG(("Attempting to set the serial configuration to the desired configuration."));

        serPortFlush(O_RDWR);

        if (_desiredPortConfig.termios.getBaudRate() != sensorPortConfig.termios.getBaudRate()) {
        	DLOG(("Changing baud rate to: ") << _desiredPortConfig.termios.getBaudRate());
        	GILL2D_BAUD_ARGS newBaudArg = G38400;
        	switch (_desiredPortConfig.termios.getBaudRate()) {
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

        	sendSensorCmd(SENSOR_SERIAL_BAUD_CMD, n_c::SensorCmdArg(newBaudArg));
        }

        if (_desiredPortConfig.termios.getParity() | sensorPortConfig.termios.getParity()) {
        	DLOG(("Changing parity to: ") << _desiredPortConfig.termios.getParityString());
			// GILL2D only supports three combinations of word format - all based on parity
			// So just force it based on parity.
			switch (_desiredPortConfig.termios.getParityString(true).c_str()[0]) {
				case 'O':
					sendSensorCmd(SENSOR_SERIAL_DATA_WORD_CMD, n_c::SensorCmdArg(O81));
					break;

				case 'E':
					sendSensorCmd(SENSOR_SERIAL_DATA_WORD_CMD, n_c::SensorCmdArg(E81));
					break;

				case 'N':
					sendSensorCmd(SENSOR_SERIAL_DATA_WORD_CMD, n_c::SensorCmdArg(N81));
					break;

				default:
					break;
			}
        }

        setPortConfig(_desiredPortConfig);
        applyPortConfig();
        if (getPortConfig() == _desiredPortConfig) {
            if (!doubleCheckResponse()) {
				NLOG(("GILL2D::installDesiredSensorConfig() failed to achieve sensor communication "
						"after setting desired serial port parameters. This is the current PortConfig") << getPortConfig());

                setPortConfig(sensorPortConfig);
                applyPortConfig();

				DLOG(("Setting the port config back to something that works for a retry") << getPortConfig());
                
                if (!doubleCheckResponse()) {
                    DLOG(("The sensor port config which originally worked before attempting "
                          "to set the desired config no longer works. Really messed up now!"));
                }

                else {
                    DLOG(("GILL2D reset to original!!!") << getPortConfig());
                }
            }
            else {
				NLOG(("Success!! GILL2D set to desired configuration!!!") << getPortConfig());
                installed = true;
            }
        }

        else {
            DLOG(("Attempt to set PortConfig to desiredPortConfig failed."));
            DLOG(("Desired PortConfig: ") << _desiredPortConfig);
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

void GILL2D::sendScienceParameters() {
    bool desiredIsDefault = true;

    DLOG(("Check for whether the desired science parameters are the same as the default"));
    for (int i=0; i< NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if ((_desiredScienceParameters[i].cmd != DEFAULT_SCIENCE_PARAMETERS[i].cmd)
            || (_desiredScienceParameters[i].arg != DEFAULT_SCIENCE_PARAMETERS[i].arg)) {
            desiredIsDefault = false;
            break;
        }
    }

    if (desiredIsDefault) NLOG(("Base class did not modify the default science parameters for this Gill sensor"));
    else NLOG(("Base class modified the default science parameters for this PB210"));

    DLOG(("Sending science parameters"));
    for (int j=0; j<NUM_DEFAULT_SCIENCE_PARAMETERS; ++j) {
        sendSensorCmd(_desiredScienceParameters[j].cmd, _desiredScienceParameters[j].arg);
    }
}

bool GILL2D::checkScienceParameters()
{
	bool scienceParametersOK = false;

    VLOG(("GILL2D::checkScienceParameters() - Flush port and send query command"));
    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    sendSensorCmd(SENSOR_DIAG_QRY_CMD, n_c::SensorCmdArg(OPER_CONFIG));

    static const int BUF_SIZE = 512;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    VLOG(("GILL2D::checkScienceParameters() - Read the entire response"));
    int numCharsRead = readEntireResponse(&(respBuf[0]), BUF_SIZE, 2000);


    if (numCharsRead ) {
		std::string respStr;
		respStr.append(&respBuf[0], numCharsRead);

		VLOG(("Response: "));
		VLOG((respStr.c_str()));

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
    n_c::SensorCmdData desiredCmd = getDesiredCmd(cmd);
    VLOG(("Desired command: ") << desiredCmd.cmd);
    VLOG(("Searched command: ") << cmd);

	int arg;
	std::stringstream argStrm(match);
	argStrm >> arg;

	VLOG(("Arguments match: ") << (desiredCmd.arg.intArg == arg ? "TRUE" : "FALSE"));
	return (desiredCmd.arg.intArg == arg);
}

n_c::SensorCmdData GILL2D::getDesiredCmd(GILL2D_COMMANDS cmd) {
    VLOG(("Looking in desiredScienceParameters[] for ") << cmd);
    for (int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if (_desiredScienceParameters[i].cmd == cmd) {
            VLOG(("Found command: ") << cmd);
            return _desiredScienceParameters[i];
        }
    }

    VLOG(("Requested cmd not found: ") << cmd);

    n_c::SensorCmdData nullRetVal(NULL_COMMAND, n_c::SensorCmdArg(0));
    return(nullRetVal);
}

bool GILL2D::checkResponse()
{
	DLOG(("GILL2D::checkResponse(): enter..."));
	bool retVal = false;

    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    // Just getting into config mode elicits a usable response...
    NLOG(("Sending GILL command to get current config..."));
    sendSensorCmd(SENSOR_DIAG_QRY_CMD, n_c::SensorCmdArg(OPER_CONFIG));

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


void GILL2D::sendSensorCmd(int cmd, n_c::SensorCmdArg arg)
{
    assert(cmd < NUM_SENSOR_CMDS);
    std::string snsrCmd(cmdTable[cmd]);
	std::ostringstream argStr;

    // Most GIL commands take character args starting at '1'.
    // Some do not, so if a value < 0 is passed in, skip this part.
    if (arg.intArg >= 0) {
    	if (cmd == SENSOR_AVG_PERIOD_CMD) {
        	// requires at least 4 numeric characters, 0 padded.
        	char buf[5];
        	snprintf(buf, 5, "%04d", arg.intArg);
        	argStr << std::string(buf, 5);
        }
    	else if (arg.intArg >= 1) {
    		if (cmd == SENSOR_CONFIG_MODE_CMD) {
				argStr << arg.intArg;
			}
			else {
				argStr << arg.intArg;
			}
    	}

        DLOG(("Attempting to insert: ") << arg.intArg);
    	int insertIdx = snsrCmd.find_last_of('\r');
    	DLOG(("Found \\r at position: ") << insertIdx);
    	snsrCmd.insert(insertIdx, argStr.str());
    	// add the \r at the beginning to help some GILL states get unstuck...
    	// For instance, when changing serial port settings while in configuration mode,
    	// a carriage return is necessary before it will respond to commands.
    	snsrCmd.insert(0, "\r\n");
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
			size_t stxPos = respStr.find_first_of('\x02');
			size_t etxPos = respStr.find_first_of('\x03', stxPos);
			if (stxPos != string::npos && etxPos != string::npos) {
				if ((etxPos - stxPos) == 2) {
					// we can probably believe that we have captured the unit address response
					_unitId = respStr[stxPos+1];
				}
			}
		}
		else if (cmd == SENSOR_SERIAL_BAUD_CMD || cmd == SENSOR_SERIAL_DATA_WORD_CMD) {
			// used to look for the "confirm >" response here, but it doesn't always come back in programmatic use cases
			// like it does in minicom. So the strategy is to just assume that it's there and send the confirmation.
			// Seems to be working.
			DLOG(("Serial port setting change... changing termios to new parameters and sending confirmation..."));
			if (confirmGillSerialPortChange(cmd, arg.intArg)) {
				DLOG(("Serial port setting confirmed: ") << cmdTable[cmd]);
			}
			else
				DLOG(("Serial port setting NOT confirmed: ") << cmdTable[cmd]);
		}
    }
}

bool GILL2D::confirmGillSerialPortChange(int cmd, int arg)
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
	std::size_t numRespChars = readEntireResponse(confirmRespBuf, 10, 2000);
	std::string respStr(confirmRespBuf, numRespChars);
	DLOG(("Confirmation Response: ") << respStr);
	DLOG(("confirmGillSerialPortChange(): exit"));

	return 	(respStr.find(entireCmd.str()) != std::string::npos);
}

void GILL2D::updateDesiredScienceParameter(GILL2D_COMMANDS cmd, int arg) {
    for(int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if (cmd == _desiredScienceParameters[i].cmd) {
            _desiredScienceParameters[i].arg.intArg = arg;
            break;
        }
    }
}

bool GILL2D::checkConfigMode(bool continuous)
{
	bool retVal = false;
    static const int BUF_SIZE = 75;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);
    int numCharsRead = 0;

    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    // Just getting into config mode elicits a usable response...
    if (continuous) {
		DLOG(("Sending GILL command to enter configuration mode in continuous operation..."));
		sendSensorCmd(SENSOR_CONFIG_MODE_CMD, n_c::SensorCmdArg(-1));
    }
    else {
    	if (!_unitId) {
    		DLOG(("Attempting to get unit ID"));
    		sendSensorCmd(SENSOR_QRY_ID_CMD, n_c::SensorCmdArg(-1));
    	}

    	if (_unitId) {
			DLOG(("Sending polled mode command to enter config mode"));
			sendSensorCmd(SENSOR_CONFIG_MODE_CMD, n_c::SensorCmdArg(_unitId));
    	}
    	else
    		DLOG(("Didn't get the unit ID. Must not be in polled mode, or bad serial port config."));
    }

    memset(respBuf, 0, BUF_SIZE);

    numCharsRead = readEntireResponse(&(respBuf[0]), BUF_SIZE, 2000);

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

n_c::CFG_MODE_STATUS GILL2D::enterConfigMode()
{
	DLOG(("GILL2D::enterConfigMode(): enter..."));
	n_c::CFG_MODE_STATUS retVal = NOT_ENTERED;

	DLOG(("Trying to get into Configuration Mode"));
	DLOG(("First check to see if sensor is in continuous output..."));
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

void GILL2D::exitConfigMode()
{
    sendSensorCmd(SENSOR_START_MEAS_CMD);
}


}}} //namespace nidas { namespace dynld { namespace isff {

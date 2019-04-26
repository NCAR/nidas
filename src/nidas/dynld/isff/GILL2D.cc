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
#include <boost/regex.hpp>
#include <time.h>
#include <string.h>
#include <sys/select.h>

using namespace nidas::core;
using namespace std;
using namespace boost;

NIDAS_CREATOR_FUNCTION_NS(isff,GILL2D)

namespace nidas { namespace dynld { namespace isff {

const char* GILL2D::DEFAULT_MSG_SEP_CHAR = "\n";

const char* GILL2D::SENSOR_CONFIG_MODE_CMD_STR = "*";
const char* GILL2D::SENSOR_ENABLE_POLLED_MODE_CMD_STR = "?";
const char* GILL2D::SENSOR_POLL_MEAS_CMD_STR = "?";
const char* GILL2D::SENSOR_QRY_ID_CMD_STR = "&";
const char* GILL2D::SENSOR_DISABLE_POLLED_MODE_CMD_STR = "!\r";
const char* GILL2D::SENSOR_SOS_TEMP_CMD_STR = "A\r";
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
    SENSOR_SOS_TEMP_CMD_STR,
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
    WordSpec(8, Termios::NONE, 1),
    WordSpec(8, Termios::EVEN, 1),
    WordSpec(8, Termios::ODD, 1),
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
    n_c::SensorCmdData(SENSOR_SOS_TEMP_CMD, n_c::SensorCmdArg(REPORT_TEMP)),
    n_c::SensorCmdData(SENSOR_AVG_PERIOD_CMD, n_c::SensorCmdArg(0)),
    n_c::SensorCmdData(SENSOR_HEATING_CMD, n_c::SensorCmdArg(HTG_DISABLED)),
    n_c::SensorCmdData(SENSOR_NMEA_ID_STR_CMD, n_c::SensorCmdArg(IIMWV)),
    n_c::SensorCmdData(SENSOR_MSG_TERM_CMD, n_c::SensorCmdArg(LF)),
    n_c::SensorCmdData(SENSOR_MSG_STREAM_CMD, n_c::SensorCmdArg(ASC_PLR_CONT)),
	n_c::SensorCmdData(SENSOR_OUTPUT_FIELD_FMT_CMD, n_c::SensorCmdArg(FIXED_FIELD)),
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
static const regex GILL2D_RESPONSE_REGEX_STR("[[:space:]]+"
											 "A([[:digit:]]) B[[:digit:]] C[[:digit:]] E[[:digit:]] F[[:digit:]] "
		                                     "G[[:digit:]]{4} H[[:digit:]] J[[:digit:]] K[[:digit:]] L[[:digit:]] "
		                                     "M[[:digit:]] N[[:upper:]] O[[:digit:]] P[[:digit:]] T[[:digit:]] "
		                                     "U[[:digit:]] V[[:digit:]] X[[:digit:]] Y[[:digit:]] Z[[:digit:]]");
static const regex GILL2D_COMPARE_REGEX_STR("[[:space:]]+"
											"A([[:digit:]]) B([[:digit:]]) C([[:digit:]]) E([[:digit:]]) F([[:digit:]]) "
		                                    "G([[:digit:]]{4}) H([[:digit:]]) J([[:digit:]]) K([[:digit:]]) L([[:digit:]]) "
		                                    "M([[:digit:]]) N([[:upper:]]) O([[:digit:]]) P([[:digit:]]) T([[:digit:]]) "
		                                    "U([[:digit:]]) V([[:digit:]]) X([[:digit:]]) Y([[:digit:]]) Z([[:digit:]])");
static const regex GILL2D_CONFIG_MODE_REGEX_STR("[[:space:]]+CONFIGURATION MODE");
static const regex GILL2D_SERNO_REGEX_STR("D1[[:space:]]+([[:alnum:]]+)[[:space:]]+D1");
static const regex GILL2D_FW_VER_REGEX_STR("D2[[:space:]]+([[:digit:]]+\\.[[:digit:]]+)");

static const std::string AVERAGING_CFG_DESC("Avg secs");
static const std::string SOS_TEMP_CFG_DESC("SpdOfSnd/Temp Rprt");
static const std::string HEATING_CFG_DESC("Heater");
static const std::string NMEA_ID_STR_CFG_DESC("NMEA");
static const std::string MSG_TERM_CFG_DESC("Msg Term");
static const std::string MSG_STREAM_CFG_DESC("Msg Stream");
static const std::string FIELD_FMT_CFG_DESC("Field Fmt");
static const std::string OUTPUT_RATE_CFG_DESC("Output Rate");
static const std::string MEAS_UNITS_CFG_DESC("Meas Units");
static const std::string NODE_ADDR_CFG_DESC("Node Addr");
static const std::string VERT_MEAS_PAD_CFG_DESC("Vert Pad");
static const std::string ALIGN_45_DEG_CFG_DESC("Align/45 Deg");

GILL2D::GILL2D()
    : Wind2D(DEFAULT_PORT_CONFIG),
      testPortConfig(),
      _desiredPortConfig(DEFAULT_PORT_CONFIG),
      _defaultMessageConfig(DEFAULT_MESSAGE_LENGTH, DEFAULT_MSG_SEP_CHAR, DEFAULT_MSG_SEP_EOM),
      _desiredScienceParameters(), _sosEnabled(false), _unitId('\0'), _polling(false)
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

    initCustomMetadata();
}

GILL2D::~GILL2D()
{
    delete [] _desiredScienceParameters;
}

void GILL2D::fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException)
{
    NLOG(("GILL2D - checking for sensor customizations in the DSM/Sensor Catalog XML..."));
    DLOG(("GILL2D::fromDOMElement() - entry"));
    // let the base classes have first shot at it, since we only care about an autoconfig child element
    // however, any duplicate items in autoconfig will override any items in the base classes
    Wind2D::fromDOMElement(node);

    // Handle common autoconfig attributes first...
    fromDOMElementAutoConfig(node);

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
                else if (aname == "sostemp") {
                    if (upperAval == "SOS") {
                        updateDesiredScienceParameter(SENSOR_SOS_TEMP_CMD, REPORT_SOS);
                    }
                    else if (upperAval == "TEMP") {
                        updateDesiredScienceParameter(SENSOR_SOS_TEMP_CMD, REPORT_TEMP);
                    }
                    else if (upperAval == "BOTH") {
                        updateDesiredScienceParameter(SENSOR_SOS_TEMP_CMD, REPORT_BOTH);
                    }
                    else if (upperAval == "FALSE" || upperAval == "NO" || upperAval == "NONE"
                             || upperAval == "NA" || upperAval == "DISABLE" || aval == "0") {
                        updateDesiredScienceParameter(SENSOR_SOS_TEMP_CMD, REPORT_DISABLED);
                    }
                    else
                        throw n_u::InvalidParameterException(
                            string("GILL2D:") + getName(), aname, aval);
                }
                else if (aname == "heating") {
                    if (upperAval == "TRUE" || upperAval == "YES" || upperAval == "ENABLE" || aval == "1") {
                        updateDesiredScienceParameter(SENSOR_HEATING_CMD, HTG_ACTIVE);
                    }
                    else if (upperAval == "FALSE" || upperAval == "NO" || upperAval == "NONE"
                             || upperAval == "NA" || upperAval == "DISABLE" || aval == "0") {
                        updateDesiredScienceParameter(SENSOR_HEATING_CMD, HTG_DISABLED);
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
                        		updateDesiredScienceParameter(SENSOR_MSG_STREAM_CMD, ASC_PLR_POLLED);
                			}
                			else
                                throw n_u::InvalidParameterException(
                                    string("GILL2D:") + getName(), aname, aval);
                		}
                		if (upperAval.find("POLAR") || upperAval.find("PLR")) {
                			if (upperAval.find("CONT")) {
                        		updateDesiredScienceParameter(SENSOR_MSG_STREAM_CMD, ASC_PLR_CONT);
                			}
                			else if (upperAval.find("POLL")) {
                        		updateDesiredScienceParameter(SENSOR_MSG_STREAM_CMD, ASC_PLR_POLLED);
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

bool GILL2D::installDesiredSensorConfig(const PortConfig& rDesiredConfig)
{
    bool installed = false;
    PortConfig sensorPortConfig = getPortConfig();

    // at this point we need to determine whether or not the current working config
    // is the desired config, and adjust as necessary
    if (rDesiredConfig != sensorPortConfig) {
        // Gotta modify the GILL2D parameters first, and the modify our parameters to match and hope for the best.
        // We only do this for the serial and science parameters, as the sensor is physically configured to use
        // the transceiver mode we discovered it works on. To change these parameters, the user would have to
        // physically reconfigure the sensor and re-start the auto-config process.
        DLOG(("Attempting to set the serial configuration to the desired configuration."));

        serPortFlush(O_RDWR);

        if (rDesiredConfig.termios.getBaudRate() != sensorPortConfig.termios.getBaudRate()) {
            DLOG(("Changing baud rate to: ") << rDesiredConfig.termios.getBaudRate());
            GILL2D_BAUD_ARGS newBaudArg = G38400;
            switch (rDesiredConfig.termios.getBaudRate()) {
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

        if (rDesiredConfig.termios.getParity() | sensorPortConfig.termios.getParity()) {
            DLOG(("Changing parity to: ") << rDesiredConfig.termios.getParityString());
            // GILL2D only supports three combinations of word format - all based on parity
            // So just force it based on parity.
            switch (rDesiredConfig.termios.getParityString(true).c_str()[0]) {
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

    static const int BUF_SIZE = 240;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    VLOG(("GILL2D::checkScienceParameters() - Read the entire response"));
    int numCharsRead = readEntireResponse(&(respBuf[0]), BUF_SIZE-1, MSECS_PER_SEC);

    if (numCharsRead ) {
        std::string respStr;
        respStr.append(&respBuf[0], numCharsRead);
        transformEmbeddedNulls(respStr);

        VLOG(("Response: "));
        VLOG((respStr.c_str()));

        VLOG(("GILL2D::checkScienceParameters() - Check the individual parameters available to us"));
        cmatch results;
        bool regexFound = regex_search(respStr.c_str(), results, GILL2D_COMPARE_REGEX_STR);
        bool responseOK = regexFound && results[0].matched;
        if (!responseOK) {
            DLOG(("GILL2D::checkScienceParameters(): regex failed"));
            return scienceParametersOK;
        }

        else {
            if (results[1].matched) {
                VLOG(("Checking SOS/Temp status(A) with argument: ") << results.str(1));
                scienceParametersOK = compareScienceParameter(SENSOR_SOS_TEMP_CMD, results.str(1).c_str());
                updateMetaDataItem(MetaDataItem(SOS_TEMP_CFG_DESC, results.str(1)));
            }

			if (scienceParametersOK && results[6].matched) {
				VLOG(("Checking sample averaging time(G) with argument: ") << results.str(6));
				scienceParametersOK = compareScienceParameter(SENSOR_AVG_PERIOD_CMD, results.str(6).c_str());
				updateMetaDataItem(MetaDataItem(AVERAGING_CFG_DESC, results.str(6)));
			}

			if (scienceParametersOK && results[7].matched) {
				VLOG(("Checking heater status(H) with argument: ") << results.str(7));
				scienceParametersOK = compareScienceParameter(SENSOR_HEATING_CMD, results.str(7).c_str());
                updateMetaDataItem(MetaDataItem(HEATING_CFG_DESC, results.str(7)));
			}

			if (scienceParametersOK && results[9].matched) {
				VLOG(("Checking NMEA string(K) with argument: ") << results.str(9));
				scienceParametersOK = compareScienceParameter(SENSOR_NMEA_ID_STR_CMD, results.str(9).c_str());
                updateMetaDataItem(MetaDataItem(NMEA_ID_STR_CFG_DESC, results.str(9)));
			}

			if (scienceParametersOK && results[10].matched) {
				VLOG(("Checking message termination(L) with argument: ") << results.str(10));
				scienceParametersOK = compareScienceParameter(SENSOR_MSG_TERM_CMD, results.str(10).c_str());
                updateMetaDataItem(MetaDataItem(MSG_TERM_CFG_DESC, results.str(10)));
			}

			if (scienceParametersOK && results[11].matched) {
				VLOG(("Checking message stream format(M) with argument: ") << results.str(11));
				scienceParametersOK = compareScienceParameter(SENSOR_MSG_STREAM_CMD, results.str(11).c_str());
                updateMetaDataItem(MetaDataItem(MSG_STREAM_CFG_DESC, results.str(11)));
			}

			if (scienceParametersOK && results[12].matched) {
				VLOG(("Checking node address(N) with argument: ") << results.str(12));
				scienceParametersOK = compareScienceParameter(SENSOR_NODE_ADDR_CMD, results.str(12).c_str());
                updateMetaDataItem(MetaDataItem(NODE_ADDR_CFG_DESC, results.str(12)));
			}

			if (scienceParametersOK && results[13].matched) {
				VLOG(("Checking output field format(O) with argument: ") << results.str(13));
				scienceParametersOK = compareScienceParameter(SENSOR_OUTPUT_FIELD_FMT_CMD, results.str(13).c_str());
                updateMetaDataItem(MetaDataItem(FIELD_FMT_CFG_DESC, results.str(13)));
			}

			if (scienceParametersOK && results[14].matched) {
				VLOG(("Checking output rate(P) with argument: ") << results.str(14));
				scienceParametersOK = compareScienceParameter(SENSOR_OUTPUT_RATE_CMD, results.str(14).c_str());
                updateMetaDataItem(MetaDataItem(OUTPUT_RATE_CFG_DESC, results.str(14)));
		}

			if (scienceParametersOK && results[16].matched) {
				VLOG(("Checking wind speed units(U) with argument: ") << results.str(16));
				scienceParametersOK = compareScienceParameter(SENSOR_MEAS_UNITS_CMD, results.str(16).c_str());
                updateMetaDataItem(MetaDataItem(MEAS_UNITS_CFG_DESC, results.str(16)));
			}

			if (scienceParametersOK && results[17].matched) {
				VLOG(("Checking vertical output pad(V) with argument: ") << results.str(17));
				scienceParametersOK = compareScienceParameter(SENSOR_VERT_MEAS_PADDING_CMD, results.str(17).c_str());
                updateMetaDataItem(MetaDataItem(VERT_MEAS_PAD_CFG_DESC, results.str(17)));
			}

			if (scienceParametersOK && results[18].matched) {
				VLOG(("Checking sensor alignment(X) with argument: ") << results.str(18));
				scienceParametersOK = compareScienceParameter(SENSOR_ALIGNMENT_CMD, results.str(18).c_str());
                updateMetaDataItem(MetaDataItem(ALIGN_45_DEG_CFG_DESC, results.str(18)));
			}
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


// TODO: Gotta clear out the string NULLs when Gill responds to command:
// K1
// K\0+\r*K1\r
void GILL2D::transformEmbeddedNulls(std::string& respStr)
{
    DLOG(("GILL2D::transformEmbeddedNulls(): respStr before: ") << respStr);
    for (std::size_t i=0; i<respStr.length(); ++i) {
        char& rC = respStr[i];
        DLOG(("GILL2D::transformEmbeddedNulls(): rC: 0X%02X", rC));
        if (isNonPrintable(rC, false)) {
            DLOG(("GILL2D::transformEmbeddedNulls(): non-printable, replacing with <space>"));
            rC = ' ';
        }
        else if (i < respStr.length()-1) {
            char& rC2 = respStr[i+1];
            if (i == 0) {
                if ((rC == '\r' && rC2 != '\n')) {
                    respStr.insert(i+1, "\n");
                    ++i;
                    DLOG(("GILL2D::transformEmbeddedNulls(): insert \\n after \\r"));
                }
                else if ((rC == '\n' && rC2 != '\r')) {
                    respStr.insert(i+1, "\r");
                    ++i;
                    DLOG(("GILL2D::transformEmbeddedNulls(): insert \\r after \\n"));
                }
                else if ((rC == '\n' && rC2 == '\r')
                         || (rC == '\r' && rC2 == '\n')) {
                    DLOG(("GILL2D::transformEmbeddedNulls(): found \\n\\r pair, continue..."));
                    ++i;
                    continue;
                }
            }
            else {
                char& rCminusOne = respStr[i-1];
                if ((rCminusOne == '\r' && rC == '\n')
                    || (rCminusOne == '\n' && rC == '\r')
                    || (rC == '\n' && rC2 == '\r')
                    || (rC == '\r' && rC2 == '\n')) {
                    DLOG(("GILL2D::transformEmbeddedNulls(): found \\n\\r pair, continue..."));
                    ++i;
                    continue;
                }
                else if ((rCminusOne == '\n' && rC != '\r')) {
                    DLOG(("GILL2D::transformEmbeddedNulls(): insert \\r after \\n"));
                    respStr.insert(i, "\r");
                    ++i;
                }
                else if ((rCminusOne == '\r' && rC != '\n')) {
                    DLOG(("GILL2D::transformEmbeddedNulls(): insert \\n after \\r"));
                    respStr.insert(i, "\n");
                    ++i;
                }
            }
        }
    }
    DLOG(("GILL2D::transformEmbeddedNulls(): respStr after: ") << respStr);
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

    static const int BUF_SIZE = 240;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    int numCharsRead = readEntireResponse(&(respBuf[0]), BUF_SIZE-1, MSECS_PER_SEC);

    if (numCharsRead > 0) {
        std::string respStr(&respBuf[0], numCharsRead);

        // trim beyond null/non-printables
        transformEmbeddedNulls(respStr);
        DLOG(("Response: "));
        DLOG((respStr.c_str()));

        // This is where the response is checked for signature elements
        VLOG(("GILL2D::checkResponse() - Check the general format of the config mode response"));
        cmatch results;
        bool regexFound = regex_search(respStr.c_str(), results, GILL2D_RESPONSE_REGEX_STR);
        retVal = regexFound && results[0].matched;
        if (!retVal) {
            DLOG(("GILL2D::checkResponse(): regex_search(GILL2D_RESPONSE_REGEX_STR) failed"));
        }
        else {
            // Check for Speed of Sound and Temperature measurement capability
            if (results[1].matched) {
                if (results.str(1) != "0") {
                    _sosEnabled = true;
                }
            }
        }
    }

    else {
        DLOG(("Didn't get any chars from serial port, or got garbage"));
    }

    DLOG(("GILL2D::checkResponse(): exit..."));
    return retVal;
}


void GILL2D::sendSensorCmd(int cmd, n_c::SensorCmdArg arg)
{
    assert(cmd < NUM_SENSOR_CMDS);
    std::string snsrCmd(cmdTable[cmd]);
    std::ostringstream argStr;

    // Don't bother sending the SOS/Temp command
    // if the instrument can't handle it.
    if (cmd == SENSOR_SOS_TEMP_CMD && !_sosEnabled) {
       ILOG(("GILL2D::sendSensorCmd(): Not sending SOS/Temp command"
             "as this particular model of WindObserver does not support it."));
       return;
    }

    // Most GIL commands take character args starting at '1'.
    // Some do not, so if a value < 0 is passed in, skip this part.
    if (arg.intArg >= 0) {
        if (cmd == SENSOR_AVG_PERIOD_CMD) {
            // requires at least 4 numeric characters, 0 padded.
            char buf[5];
            snprintf(buf, 5, "%04d", arg.intArg);
            argStr << std::string(buf, 5);
        }
    	// TODO ???What???
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
    writePause(snsrCmd.c_str(), snsrCmd.length());

    if (isConfigCmd(cmd) || cmd == SENSOR_QRY_ID_CMD) {
        if (cmd == SENSOR_SERIAL_BAUD_CMD || cmd == SENSOR_SERIAL_DATA_WORD_CMD) {
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

        else {
            const int BUF_SIZE = 20;
            char cmdRespBuf[BUF_SIZE];
            memset(cmdRespBuf, 0, BUF_SIZE);

            int numCharsRead = readEntireResponse(cmdRespBuf, BUF_SIZE-1, MSECS_PER_SEC);
            if (numCharsRead > 0) {
                std::string respStr(cmdRespBuf, numCharsRead);
                transformEmbeddedNulls(respStr);
                std::ostringstream oss;
                oss << "Sent: " << snsrCmd << std::endl << "   Received: " << respStr;
                DLOG((oss.str().c_str()));
                if (cmd == SENSOR_QRY_ID_CMD) {
                    std::size_t stxPos = respStr.find_first_of('\x02');
                    std::size_t etxPos = respStr.find_first_of('\x03', stxPos);
                    if (stxPos != string::npos && etxPos != string::npos) {
                        if ((etxPos - stxPos) == 2) {
                            // we can probably believe that we have captured the unit address response
                            _unitId = respStr[stxPos+1];
                        }
                    }
                }
            }
        }
    }
}

bool GILL2D::confirmGillSerialPortChange(int cmd, int arg)
{
    DLOG(("GILL2D::confirmGillSerialPortChange(): enter"));
    DLOG(("GILL2D::confirmGillSerialPortChange(): verifying the confirm > prompt.."));

    const int BUF_SIZE = 50;
    char confirmRespBuf[BUF_SIZE];
    memset(confirmRespBuf, 0, BUF_SIZE);

    std::size_t numRespChars = readEntireResponse(confirmRespBuf, BUF_SIZE-1, MSECS_PER_SEC);
    std::string respStr;
    if (numRespChars > 0) {
        respStr.append(confirmRespBuf);
        transformEmbeddedNulls(respStr);
        DLOG(("Confirmation Prompt: ") << respStr);
    }

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
    writePause(confirmCmd.c_str(), 3);

    DLOG(("Reading the confirmation response..."));
    memset(confirmRespBuf, 0, BUF_SIZE);

    numRespChars = readEntireResponse(confirmRespBuf, BUF_SIZE-1, MSECS_PER_SEC);
    if (numRespChars > 0) {
        respStr.assign(confirmRespBuf);
        transformEmbeddedNulls(respStr);
        DLOG(("Confirmation Response: ") << respStr);
    }

    DLOG(("confirmGillSerialPortChange(): exit"));
    return  (respStr.find(entireCmd.str()) != std::string::npos);
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

    numCharsRead = readEntireResponse(&(respBuf[0]), BUF_SIZE-1, MSECS_PER_SEC);

    if (RANGE_CHECK_EXC(-1, numCharsRead, 21)) {
        numCharsRead += readEntireResponse(&(respBuf[numCharsRead]), BUF_SIZE-numCharsRead-1, MSECS_PER_SEC);
    }
    if (numCharsRead > 0) {
        std::string respStr;
        respStr.append(&respBuf[0], numCharsRead);
        transformEmbeddedNulls(respStr);

        DLOG(("Config Mode Response: "));
        DLOG((respStr.c_str()));

        // This is where the response is checked for signature elements
        VLOG(("GILL2D::checkConfigMode() - Check the general format of the config mode response"));
        cmatch results;
        bool regexFound = regex_search(respStr.c_str(), results, GILL2D_CONFIG_MODE_REGEX_STR);
        retVal = regexFound && results[0].matched;
        if (!retVal) {
            DLOG(("GILL2D::checkConfigMode(): Didn't find matches to the model ID string as expected."));
        }
    }

    else {
        DLOG(("Didn't get any chars from serial port or got garbage"));
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

void GILL2D::updateMetaData()
{
    setManufacturer("GILL Instruments, Ltd");

    static const int BUF_SIZE = 75;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);
    int numCharsRead = 0;
    bool regexFound = false;
    std::string respStr;
    cmatch results;
    bool matchFound = false;

    serPortFlush(O_RDWR);
    sendSensorCmd(SENSOR_DIAG_QRY_CMD, nidas::core::SensorCmdArg(TYPE_SER_NO));
    numCharsRead = readEntireResponse(&respBuf[0], BUF_SIZE-1, MSECS_PER_SEC);
    if (numCharsRead > 0) {
        respStr.append(&respBuf[0], numCharsRead);
        transformEmbeddedNulls(respStr);

        DLOG(("GILL2D::updateMetaData(): Serial number response: ") << respStr);

        regexFound = regex_search(respStr.c_str(), results, GILL2D_SERNO_REGEX_STR);
        DLOG(("GILL2D::updateMetaData(): regex_search(GILL2D_SERNO_REGEX_STR) ") << (regexFound ? "Did " : "Did NOT ") << "succeed!");
        matchFound = regexFound && results[0].matched && results[1].matched;
        if (matchFound) {
            std::string serNoStr;
            serNoStr.append(results[1].first, results[1].second - results[1].first);
            setSerialNumber(serNoStr);
        } else {
            DLOG(("GILL2D::updateMetaData(): Didn't find serial number string as expected."));
        }
    }

    memset(respBuf, 0, BUF_SIZE);
    serPortFlush(O_RDWR);
    sendSensorCmd(SENSOR_DIAG_QRY_CMD, nidas::core::SensorCmdArg(SW_VER));
    numCharsRead = readEntireResponse(&respBuf[0], BUF_SIZE-1, MSECS_PER_SEC);
    respStr.clear();
    if (numCharsRead > 0) {
        respStr.append(&respBuf[0], numCharsRead);
        DLOG(("GILL2D::updateMetaData(): FW version response: ") << respStr);
        transformEmbeddedNulls(respStr);

        regexFound = regex_search(respStr.c_str(), results, GILL2D_FW_VER_REGEX_STR);
        DLOG(("GILL2D::updateMetaData(): regex_search(GILL2D_FW_VER_REGEX_STR) ") << (regexFound ? "Did " : "Did NOT ") << "succeed!");
        matchFound = regexFound && results[0].matched && results[1].matched;
        if (matchFound) {
            std::string fwVerStr;
            fwVerStr.append(results[1].first, results[1].second - results[1].first);
            setFwVersion(fwVerStr);
        }
        else {
            DLOG(("GILL2D::updateMetaData(): Didn't find firmware version string as expected."));
        }
    }
}

void GILL2D::initCustomMetadata()
{
    addMetaDataItem(MetaDataItem(SOS_TEMP_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(AVERAGING_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(HEATING_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(NMEA_ID_STR_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(MSG_TERM_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(MSG_STREAM_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(FIELD_FMT_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(OUTPUT_RATE_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(MEAS_UNITS_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(NODE_ADDR_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(VERT_MEAS_PAD_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(ALIGN_45_DEG_CFG_DESC, ""));
}


}}} //namespace nidas { namespace dynld { namespace isff {

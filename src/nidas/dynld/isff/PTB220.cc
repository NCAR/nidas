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
#include <regex>

using namespace nidas::core;
using namespace nidas::util;
using namespace std;

NIDAS_CREATOR_FUNCTION_NS(isff,PTB220)

namespace nidas { namespace dynld { namespace isff {

const char* PTB220::SENSOR_SEND_MODE_CMD_STR = "SMODE \r\n";
// Requires SW4 set to write enable
//const char* PTB220::SENSOR_MEAS_MODE_CMD_STR = "MMODE \r\n";
//const char* PTB220::SENSOR_LINEAR_CORR_CMD_STR = "LC \r\n";
//const char* PTB220::SENSOR_MULTPT_CORR_CMD_STR = "MPC \r\n";
//const char* PTB220::SENSOR_SET_LINEAR_CORR_CMD_STR = "LCI \r\n";
//const char* PTB220::SENSOR_SET_MULTPT_CORR_CMD_STR = "MPCI \r\n";
//const char* PTB220::SENSOR_SET_CAL_DATE_CMD_STR = "CALD \r\n";
const char* PTB220::SENSOR_SERIAL_CMD_STR = "SERI \r\n";
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
const char* PTB220::SENSOR_CONFIG_QRY_CMD_STR = "?\r\n";
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
	SENSOR_SERIAL_CMD_STR,
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

static const int SENSOR_RESET_WAIT_TIME = USECS_PER_SEC * 3;


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


// regular expression contexts
// NOTE: the regular expressions need to search a buffer w/multiple lines separated by \r\n
static string PTB220_RESPONSE_REGEX_SPEC(
        "Software version.+[[:space:]]"
        "Serial number.+[[:space:]]"
        "Configuration.+[[:space:]]"
        "Linear adjustments.+[[:space:]]"
        "Multipoint adjustments.+[[:space:]]"
        "Calibration date.+[[:space:]]"
        "Baud Parity Data Stop Dpx.+[[:space:]]"
        "Echo.+[[:space:]]"
        "Sending mode.+[[:space:]]"
        "Measurement mode.+[[:space:]]"
        "Pulse mode.+[[:space:]]"
        "Address.+[[:space:]]"
        "Output interval.+[[:space:]]"
        "Output format.+[[:space:]]"
        "Error output format.+[[:space:]]"
        "SCOM format.+[[:space:]]"
        "Pressure unit.+[[:space:]]"
        "Temperature unit.+[[:space:]]"
        "Averaging time.+[[:space:]]");
static regex PTB220_RESPONSE_REGEX(PTB220_RESPONSE_REGEX_SPEC, std::regex_constants::extended);

static string PTB220_MODEL_REGEX_SPEC("Software version[[:blank:]]+([[:alnum:]]+) / [[:digit:]].[[:digit:]]{1,2}");
static regex PTB220_MODEL_REGEX(PTB220_MODEL_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_VER_REGEX_SPEC("Software version[[:blank:]]+PTB220 / ([[:digit:]].[[:digit:]]{1,2})");
static regex PTB220_VER_REGEX(PTB220_VER_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_SERIAL_NUMBER_REGEX_SPEC("Serial number[[:blank:]]+([[:upper:]][[:digit:]]+)");
static regex PTB220_SERIAL_NUMBER_REGEX(PTB220_SERIAL_NUMBER_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_CONFIG_REGEX_SPEC("Configuration[[:blank:]]+([[:digit:]])");
static regex PTB220_CONFIG_REGEX(PTB220_CONFIG_REGEX_SPEC);
static string PTB220_LINEAR_CORR_REGEX_SPEC("Linear adjustments[[:blank:]]+(ON|OFF){1}");
static regex PTB220_LINEAR_CORR_REGEX(PTB220_LINEAR_CORR_REGEX_SPEC);
static string PTB220_MULTI_PT_CORR_REGEX_SPEC("Multipoint adjustments[[:blank:]]+(ON|OFF)");
static regex PTB220_MULTI_PT_CORR_REGEX(PTB220_MULTI_PT_CORR_REGEX_SPEC, std::regex_constants::extended);
static regex PTB220_CAL_DATE_REGEX("Calibration date[[:blank:]]+([?[:digit:]]{4}(-[?[:digit:]]{2}){2})");
static string PTB220_SERIAL_CFG_REGEX_SPEC("Baud Parity Data Stop Dpx[[:blank:]]+([[:digit:]]{4,5})[[:blank:]]+"
										         "(N|E|O){1}[[:blank:]]+(7|8){1}[[:blank:]]+(1|2){1}[[:blank:]]+"
										         "([[:upper:]])");
static regex PTB220_SERIAL_CFG_REGEX(PTB220_SERIAL_CFG_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_ECHO_REGEX_SPEC("Echo[[:blank:]]+(ON|OFF){1}");
static regex PTB220_ECHO_REGEX(PTB220_ECHO_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_SENDING_MODE_REGEX_SPEC("Sending mode[[:blank:]]+(RUN|POLL){1}( / OPEN)*");
static regex PTB220_SENDING_MODE_REGEX(PTB220_SENDING_MODE_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_PULSE_MODE_REGEX_SPEC("Pulse mode[[:blank:]]+(OFF){1}.*");
static regex PTB220_PULSE_MODE_REGEX(PTB220_PULSE_MODE_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_MEAS_MODE_REGEX_SPEC("Measurement mode[[:blank:]]+(NORMAL|FAST){1}");
static regex PTB220_MEAS_MODE_REGEX(PTB220_MEAS_MODE_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_ADDRESS_REGEX_SPEC("Address[[:blank:]]+([[:digit:]]{1,3})");
static regex PTB220_ADDRESS_REGEX(PTB220_ADDRESS_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_OUTPUT_INTERVAL_REGEX_SPEC("Output interval[[:blank:]]+([[:digit:]]{1,3}) ((s|min|hr){1})");
static regex PTB220_OUTPUT_INTERVAL_REGEX(PTB220_OUTPUT_INTERVAL_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_OUTPUT_FMT_REGEX_SPEC("Output format[[:blank:]]+([[:alnum:]\"#.])+");
static regex PTB220_OUTPUT_FMT_REGEX(PTB220_OUTPUT_FMT_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_ERR_OUT_FMT_REGEX_SPEC("Error output format[[:blank:]]+([[:alnum:]\"#.])*");
static regex PTB220_ERR_OUT_FMT_REGEX(PTB220_ERR_OUT_FMT_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_USR_OUT_FMT_REGEX_SPEC("SCOM format[[:blank:]]+([[:alnum:]\"#.])*");
static regex PTB220_USR_OUT_FMT_REGEX(PTB220_USR_OUT_FMT_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_PRESS_UNIT_REGEX_SPEC("Pressure unit[[:blank:]]+([[:alnum:]]{2,5})");
static regex PTB220_PRESS_UNIT_REGEX(PTB220_PRESS_UNIT_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_TEMP_UNIT_REGEX_SPEC("Temperature unit[[:blank:]]+'(C|F){1}");
static regex PTB220_TEMP_UNIT_REGEX(PTB220_TEMP_UNIT_REGEX_SPEC, std::regex_constants::extended);
static string PTB220_AVG_TIME_REGEX_SPEC("Averaging time[[:blank:]]+([[:digit:]]{1,3}.[[:digit:]]) s");
static regex PTB220_AVG_TIME_REGEX(PTB220_AVG_TIME_REGEX_SPEC, std::regex_constants::extended);

static const std::string PTB220_XDUCER_CFG_CFG_DESC("Configuration");
static const std::string PTB220_LINEAR_CORR_CFG_DESC("Linear adjustments");
static const std::string PTB220_MULTI_PT_CORR_CFG_DESC("Multipoint adjustments");
static const std::string PTB220_CAL_DATE_CFG_DESC("Calibration date");
static const std::string PTB220_SERIAL_PARAM_CFG_DESC("Baud Parity Data Stop Dpx");
static const std::string PTB220_SERIAL_ECHO_CFG_DESC("Echo");
static const std::string PTB220_SENDING_MODE_CFG_DESC("Sending mode");
static const std::string PTB220_MEAS_MODE_CFG_DESC("Measurement mode");
static const std::string PTB220_PULSE_MODE_CFG_DESC("Pulse mode");
static const std::string PTB220_ADDRESS_CFG_DESC("Address");
static const std::string PTB220_OUTPUT_INTVL_CFG_DESC("Output interval");
static const std::string PTB220_OUTPUT_FMT_CFG_DESC("Output format");
static const std::string PTB220_ERR_OUTPUT_FMT_CFG_DESC("Error output format");
static const std::string PTB220_USR_SEND_CFG_DESC("SCOM format");
static const std::string PTB220_PRESS_UNIT_CFG_DESC("Pressure unit");
static const std::string PTB220_TEMP_UNIT_CFG_DESC("Temperature unit");
static const std::string PTB220_AVG_TIME_CFG_DESC("Averaging time");


PTB220::PTB220()
    : SerialSensor(),
      defaultMessageConfig(0, "\r\n", true),
      desiredScienceParameters(), sensorSWVersion(""), sensorSerialNumber("")
{
    auto bauds = {9600, 4800, 2400, 1200, 600};
    for (int baud: bauds)
    {
        addPortConfig(PortConfig(baud, 7, Parity::ODD, 1, RS232));
        addPortConfig(PortConfig(baud, 7, Parity::EVEN, 1, RS232));
        addPortConfig(PortConfig(baud, 8, Parity::NONE, 1, RS232));
        addPortConfig(PortConfig(baud, 8, Parity::EVEN, 1, RS232));
        addPortConfig(PortConfig(baud, 8, Parity::ODD, 1, RS232));
    }

    // put 2 stop bits last because these are really rarely used.
    for (int baud: bauds)
    {
        addPortConfig(PortConfig(baud, 7, Parity::N, 2, RS232));
        addPortConfig(PortConfig(baud, 7, Parity::E, 2, RS232));
        addPortConfig(PortConfig(baud, 7, Parity::O, 2, RS232));
        addPortConfig(PortConfig(baud, 8, Parity::N, 2, RS232));
    }

    // We set the defaults at construction, 
    // letting the base class modify according to fromDOMElement() 
    setMessageParameters(defaultMessageConfig);

    desiredScienceParameters = new n_c::SensorCmdData[NUM_DEFAULT_SCIENCE_PARAMETERS];
    for (int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        desiredScienceParameters[i] = DEFAULT_SCIENCE_PARAMETERS[i];
    }

    initCustomMetaData();
    setAutoConfigSupported();
}

PTB220::~PTB220()
{
    delete [] desiredScienceParameters;
}

void PTB220::fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException)
{
    // let the base classes have first shot at it, since we only care about an autoconfig child element
    // however, any duplicate items in autoconfig will override any items in the base classes
    SerialSensor::fromDOMElement(node);

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

nidas::core::CFG_MODE_STATUS PTB220::enterConfigMode()
{
    sendSensorCmd(SENSOR_STOP_CMD);
    return nidas::core::ENTERED;
}

void PTB220::exitConfigMode()
{
    sendSensorCmd(SENSOR_RUN_CMD);
}

bool PTB220::installDesiredSensorConfig(const PortConfig& rDesiredPortConfig)
{
    bool installed = false;
    PortConfig sensorPortConfig = getPortConfig();

    // at this point we need to determine whether or not the current working config 
    // is the desired config, and adjust as necessary
    if (rDesiredPortConfig != sensorPortConfig) {
        // Gotta modify the PTB220 parameters first, and the modify our parameters to match and hope for the best.
        // We only do this for the serial and science parameters, as the sensor is physically configured to use  
        // the transceiver mode we discovered it works on. To change these parameters, the user would have to  
        // physically reconfigure the sensor and re-start the auto-config process.
        DLOG(("Attempting to set the serial configuration to the desired configuration."));

        serPortFlush(O_RDWR);
        
        std::ostringstream baudStr;
        baudStr << rDesiredPortConfig.termios.getBaudRate()
                << " " << rDesiredPortConfig.termios.getBitsString()
                << " F";

        DLOG(("PTB220::installDesiredSensorConfig(): serial command string: ") << baudStr.str());

        sendSensorCmd(SENSOR_SERIAL_CMD, n_c::SensorCmdArg(baudStr.str()), true);
        // wait for the sensor to reset - 3 seconds
        usleep(SENSOR_RESET_WAIT_TIME);

        setPortConfig(rDesiredPortConfig);
        applyPortConfig();
        if (getPortConfig() == rDesiredPortConfig) {
            enterConfigMode();
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
            DLOG(("Desired PortConfig: ") << rDesiredPortConfig);
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

    if (desiredIsDefault) NLOG(("Base class did not modify the default science parameters for this PTB220"));
    else NLOG(("Base class modified the default science parameters for this PTB220"));

    DLOG(("Sending science parameters"));
    for (int j=0; j<NUM_DEFAULT_SCIENCE_PARAMETERS; ++j) {
        sendSensorCmd(desiredScienceParameters[j].cmd, desiredScienceParameters[j].arg);
    }

    sendSensorCmd(SENSOR_RESET_CMD);
    usleep(SENSOR_RESET_WAIT_TIME);
}

bool PTB220::checkScienceParameters()
{
    VLOG(("PTB220::checkScienceParameters() - Flush port and send query command"));
    // We should have just reset, so get back into config mode
    enterConfigMode();

    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    sendSensorCmd(SENSOR_CONFIG_QRY_CMD);

    static const int BUF_SIZE = 1240;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    VLOG(("PTB220::checkScienceParameters() - Read the entire response"));
    int numCharsRead = readEntireResponse(&(respBuf[0]), BUF_SIZE-1, MSECS_PER_SEC, true);

    std::string respStr;
    bool scienceParametersOK = false;
    if (numCharsRead > 0) {
        respStr.append(&respBuf[0], numCharsRead);
        DLOG(("Response: "));
        DLOG((respStr.c_str()));

        VLOG(("PTB220::checkScienceParameters() - Check the individual parameters available to us"));
        cmatch results;

        // check for sample averaging
        bool regexFound = regex_search(respStr.c_str(), results, PTB220_AVG_TIME_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            string argStr = results.str(1);
            DLOG(("Checking sample averaging with argument: ") << argStr);
            scienceParametersOK = compareScienceParameter(SENSOR_AVG_TIME_CMD, argStr.c_str());
            updateMetaDataItem(MetaDataItem(PTB220_AVG_TIME_CFG_DESC, argStr));
            if (!scienceParametersOK) {
                DLOG(("PTB220::checkScienceParameters(): Reported averaging time doesn't match expected value ") << argStr );
            }
        }
        else {
            DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find average time regex: ")
                  << PTB220_AVG_TIME_REGEX_SPEC);
        }

        // check for output interval
        if (scienceParametersOK) {
            regexFound = regex_search(respStr.c_str(), results, PTB220_OUTPUT_INTERVAL_REGEX);
            if (regexFound && results[0].matched && results[1].matched && results[2].matched) {
                string argStr = results.str(1);
                DLOG(("Checking output interval with argument: ") << argStr);
                scienceParametersOK = compareScienceParameter(SENSOR_OUTPUT_INTERVAL_VAL_CMD, argStr.c_str());
                updateMetaDataItem(MetaDataItem(PTB220_OUTPUT_INTVL_CFG_DESC, argStr));
                if (!scienceParametersOK) {
                    DLOG(("PTB220::checkScienceParameters(): Reported output interval does not match expected value: ") << argStr );
                }
                else if (results[2].matched) {
                    string argStr = results[2].str();
                    DLOG(("Checking output interval units with argument: ") << argStr);
                    scienceParametersOK = compareScienceParameter(SENSOR_OUTPUT_INTERVAL_UNIT_CMD, argStr.c_str());
                    updateMetaDataItem(MetaDataItem(PTB220_OUTPUT_FMT_CFG_DESC, argStr));
                    if (!scienceParametersOK) {
                        DLOG(("PTB220::checkScienceParameters(): Reported output interval units does not match expected value: ") << argStr );
                    }
                }
            }
            else {
                DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find output interval regex: ")
                      << PTB220_OUTPUT_INTERVAL_REGEX_SPEC);
            }
        }

        // check for pressure units
        if (scienceParametersOK) {
            regexFound = regex_search(respStr.c_str(), results, PTB220_PRESS_UNIT_REGEX);
            if (regexFound && results[0].matched && results[1].matched) {
                string argStr = results.str(1);
                DLOG(("Checking output interval with argument: ") << argStr);
                scienceParametersOK = compareScienceParameter(SENSOR_PRESS_UNIT_CMD, argStr.c_str());
                updateMetaDataItem(MetaDataItem(PTB220_PRESS_UNIT_CFG_DESC, argStr));
                if (!scienceParametersOK) {
                    DLOG(("PTB220::checkScienceParameters(): Reported pressure units does not match expected value: ") << argStr );
                }
            }
            else {
                DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find pressure units regex: ")
                      << PTB220_PRESS_UNIT_REGEX_SPEC);
            }
        }

        // check for temperature units
        if (scienceParametersOK) {
            regexFound = regex_search(respStr.c_str(), results, PTB220_TEMP_UNIT_REGEX);
            if (regexFound && results[0].matched && results[1].matched) {
                string argStr = results.str(1);
                DLOG(("Checking output interval with argument: ") << argStr);
                scienceParametersOK = compareScienceParameter(SENSOR_TEMP_UNIT_CMD, argStr.c_str());
                updateMetaDataItem(MetaDataItem(PTB220_TEMP_UNIT_CFG_DESC, argStr));
                if (!scienceParametersOK) {
                    DLOG(("PTB220::checkScienceParameters(): Reported temperature units does not match expected value: ") << argStr );
                }
            }
            else {
                DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find temperature units regex: ")
                      << PTB220_TEMP_UNIT_REGEX_SPEC);
            }
        }

        // check for multi-point correction
        if (scienceParametersOK) {
            regexFound = regex_search(respStr.c_str(), results, PTB220_MULTI_PT_CORR_REGEX);
            if (regexFound && results[0].matched && results[1].matched) {
                string argStr = results.str(1);
                DLOG(("Checking multipoint correction enabled with argument: ") << argStr);
                scienceParametersOK = (argStr == "ON");
                updateMetaDataItem(MetaDataItem(PTB220_MULTI_PT_CORR_CFG_DESC, argStr));
                if (!scienceParametersOK) {
                    DLOG(("PTB220::checkScienceParameters(): Reported multipoint correction does not match expected value: ") << argStr );
                }
            }
            else {
                DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find multipoint correction regex: ")
                      << PTB220_MULTI_PT_CORR_REGEX_SPEC);
            }
        }

        /*
         *  These items we just keep track of - no checking necessary at this point.
         */
        regexFound = regex_search(respStr.c_str(), results, PTB220_CONFIG_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            updateMetaDataItem(MetaDataItem(PTB220_XDUCER_CFG_CFG_DESC, results.str(1)));
        }
        else {
            DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find transducer config regex: ")
                  << PTB220_CONFIG_REGEX_SPEC);
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_LINEAR_CORR_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            updateMetaDataItem(MetaDataItem(PTB220_LINEAR_CORR_CFG_DESC, results.str(1)));
        }
        else {
            DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find linear correction cfg regex: ")
                  << PTB220_LINEAR_CORR_REGEX_SPEC);
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_SERIAL_CFG_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            updateMetaDataItem(MetaDataItem(PTB220_SERIAL_PARAM_CFG_DESC, results.str(1)));
        }
        else {
            DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find serial port cfg regex: ")
                  << PTB220_SERIAL_CFG_REGEX_SPEC);
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_ECHO_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            updateMetaDataItem(MetaDataItem(PTB220_SERIAL_ECHO_CFG_DESC, results.str(1)));
        }
        else {
            DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find echo chars cfg regex: ")
                  << PTB220_ECHO_REGEX_SPEC);
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_SENDING_MODE_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            updateMetaDataItem(MetaDataItem(PTB220_SENDING_MODE_CFG_DESC, results.str(1)));
        }
        else {
            DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find send mode cfg regex: ")
                  << PTB220_SENDING_MODE_REGEX_SPEC);
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_PULSE_MODE_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            updateMetaDataItem(MetaDataItem(PTB220_PULSE_MODE_CFG_DESC, results.str(1)));
        }
        else {
            DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find pulse mode cfg regex: ")
                  << PTB220_PULSE_MODE_REGEX_SPEC);
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_MEAS_MODE_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            updateMetaDataItem(MetaDataItem(PTB220_MEAS_MODE_CFG_DESC, results.str(1)));
        }
        else {
            DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find measurement mode cfg regex: ")
                  << PTB220_MEAS_MODE_REGEX_SPEC);
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_ADDRESS_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            updateMetaDataItem(MetaDataItem(PTB220_ADDRESS_CFG_DESC, results.str(1)));
        }
        else {
            DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find unit address cfg regex: ")
                  << PTB220_ADDRESS_REGEX_SPEC);
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_OUTPUT_FMT_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            updateMetaDataItem(MetaDataItem(PTB220_OUTPUT_FMT_CFG_DESC, results.str(1)));
        }
        else {
            DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find output format cfg regex: ")
                  << PTB220_OUTPUT_FMT_REGEX_SPEC);
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_ERR_OUT_FMT_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            updateMetaDataItem(MetaDataItem(PTB220_ERR_OUTPUT_FMT_CFG_DESC, results.str(1)));
        }
        else {
            DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find error output format cfg regex: ")
                  << PTB220_ERR_OUT_FMT_REGEX_SPEC);
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_USR_OUT_FMT_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            updateMetaDataItem(MetaDataItem(PTB220_USR_SEND_CFG_DESC, results.str(1)));
        }
        else {
            DLOG(("PTB220::checkScienceParameters() - regex_search() failed to find user output format cfg regex: ")
                  << PTB220_USR_OUT_FMT_REGEX_SPEC);
        }
    }
    else {
        DLOG(("PTB220::checkScienceParameters(): Getting garbage or no chars from serial port"));
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
    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    sendSensorCmd(SENSOR_CONFIG_QRY_CMD);

    static const int BUF_SIZE = 1240; // from the example above * 2
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    int numCharsRead = readEntireResponse(&(respBuf[0]), BUF_SIZE-1, MSECS_PER_SEC, true);

    if (numCharsRead > 0) {
        std::string respStr;
        respStr.append(&respBuf[0], numCharsRead);

        DLOG(("Response: "));
        DLOG((respStr.c_str()));

        cmatch results;

        // check for sample averaging
        bool regexFound = regex_search(respStr.c_str(), results, PTB220_RESPONSE_REGEX);
        return (regexFound && results[0].matched);
    }
    else {
        DLOG(("PTB220::checkResponse(): Didn't get any chars or getting garbage from serial port"));
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
        case SENSOR_SERIAL_CMD:
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
    writePause(snsrCmd.c_str(), snsrCmd.length());

    // Check whether the client wants to send a reset command for those that require it to take effect
    switch (cmd) {
		case SENSOR_SERIAL_CMD:
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

void PTB220::updateDesiredScienceParameter(int cmd, n_c::SensorCmdArg arg) {
    for(int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if (cmd == desiredScienceParameters[i].cmd) {
            desiredScienceParameters[i].arg = arg;
            break;
        }
    }
}

void PTB220::updateMetaData()
{
    setManufacturer("Vaisala, Inc.");

    VLOG(("PTB220::updateMetaData() - Flush port and send query command"));
    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    sendSensorCmd(SENSOR_CONFIG_QRY_CMD);

    static const int BUF_SIZE = 1240;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    VLOG(("PTB220::udpateMetaData() - Read the entire response"));
    int numCharsRead = readEntireResponse(&(respBuf[0]), BUF_SIZE-1, MSECS_PER_SEC, true);

    std::string respStr;
    if(numCharsRead > 0) {
        respStr.append(&respBuf[0], numCharsRead);
        DLOG(("Response: "));
        DLOG((respStr.c_str()));

        cmatch results;
        bool regexFound = regex_search(respStr.c_str(), results, PTB220_MODEL_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            setModel(results.str(1));
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_SERIAL_NUMBER_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            setSerialNumber(results.str(1));
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_VER_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            setFwVersion(results.str(1));
        }

        regexFound = regex_search(respStr.c_str(), results, PTB220_CAL_DATE_REGEX);
        if (regexFound && results[0].matched && results[1].matched) {
            setCalDate(results.str(1));
        }
    }
    else {
        DLOG(("PTB220::updateMetaData(): Getting garbage or no response."));
    }
}

void PTB220::initCustomMetaData()
{
    addMetaDataItem(MetaDataItem(PTB220_XDUCER_CFG_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_LINEAR_CORR_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_MULTI_PT_CORR_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_CAL_DATE_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_SERIAL_PARAM_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_SERIAL_ECHO_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_SENDING_MODE_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_MEAS_MODE_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_PULSE_MODE_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_ADDRESS_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_OUTPUT_INTVL_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_OUTPUT_FMT_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_ERR_OUTPUT_FMT_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_USR_SEND_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_PRESS_UNIT_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_TEMP_UNIT_CFG_DESC, ""));
    addMetaDataItem(MetaDataItem(PTB220_AVG_TIME_CFG_DESC, ""));
}


}}} //namespace nidas { namespace dynld { namespace isff {

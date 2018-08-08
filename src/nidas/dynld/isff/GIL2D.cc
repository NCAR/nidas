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

#include "GIL2D.h"
#include <nidas/core/SerialPortIODevice.h>
#include <nidas/util/ParseException.h>

#include <sstream>
#include <limits>
#include <regex.h>

using namespace nidas::core;
using namespace std;

NIDAS_CREATOR_FUNCTION_NS(isff,GIL2D)

namespace nidas { namespace dynld { namespace isff {

const char GIL2D::DEFAULT_MSG_SEP_CHAR = '\n';

const char* GIL2D::SENSOR_CONFIG_MODE_CMD_STR = "*\r";
const char* GIL2D::SENSOR_ENABLE_POLLED_MODE_CMD_STR = "?\r";
const char* GIL2D::SENSOR_POLL_MEAS_CMD_STR = "\r";
const char* GIL2D::SENSOR_QRY_ID_CMD_STR = "&\r";
const char* GIL2D::SENSOR_DISABLE_POLLED_MODE_CMD_STR = "!\r";
const char* GIL2D::SENSOR_SERIAL_BAUD_CMD_STR = "B\r";
const char* GIL2D::SENSOR_DIAG_QRY_CMD_STR = "D\r";
const char* GIL2D::SENSOR_DUPLEX_COMM_CMD_STR = "E\r";
const char* GIL2D::SENSOR_SERIAL_DATA_WORD_CMD_STR = "F\r";
const char* GIL2D::SENSOR_AVG_PERIOD_CMD_STR = "G\r";
const char* GIL2D::SENSOR_HEATING_CMD_STR = "H\r";
const char* GIL2D::SENSOR_NMEA_ID_STR_CMD_STR = "K\r";
const char* GIL2D::SENSOR_MSG_TERM_CMD_STR = "L\r";
const char* GIL2D::SENSOR_MSG_STREAM_CMD_STR = "M\r";
const char* GIL2D::SENSOR_NODE_ADDR_CMD_STR = "N\r";
const char* GIL2D::SENSOR_OUTPUT_FIELD_FMT_CMD_STR = "O\r";
const char* GIL2D::SENSOR_OUTPUT_RATE_CMD_STR = "P\r";
const char* GIL2D::SENSOR_START_MEAS_CMD_STR = "Q\r";
const char* GIL2D::SENSOR_MEAS_UNITS_CMD_STR = "U\r";
const char* GIL2D::SENSOR_VERT_MEAS_PADDING_CMD_STR = "V\r";
const char* GIL2D::SENSOR_ALIGNMENT_CMD_STR = "X\r";

const char* GIL2D::cmdTable[NUM_SENSOR_CMDS] =
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
const int GIL2D::SENSOR_BAUDS[NUM_SENSOR_BAUDS] = {19200, 9600, 4800, 2400, 1200};
const WordSpec GIL2D::SENSOR_WORD_SPECS[GIL2D::NUM_SENSOR_WORD_SPECS] = {
    {7,Termios::EVEN,1}, 
    {7,Termios::ODD,1}, 
    {8,Termios::NONE,1}
};
const n_c::PORT_TYPES GIL2D::SENSOR_PORT_TYPES[GIL2D::NUM_PORT_TYPES] = {n_c::RS232, n_c::RS422, n_c::RS485_HALF };


// static default configuration to send to base class...
const PortConfig GIL2D::DEFAULT_PORT_CONFIG(GIL2D::DEFAULT_BAUD_RATE, GIL2D::DEFAULT_DATA_BITS,
                                             GIL2D::DEFAULT_PARITY, GIL2D::DEFAULT_STOP_BITS,
                                             GIL2D::DEFAULT_PORT_TYPE, GIL2D::DEFAULT_SENSOR_TERMINATION, 
                                             GIL2D::DEFAULT_SENSOR_POWER, GIL2D::DEFAULT_RTS485, 
                                             GIL2D::DEFAULT_CONFIG_APPLIED);

const PTB_CMD_ARG GIL2D::DEFAULT_SCIENCE_PARAMETERS[] = {
    {DEFAULT_PRESSURE_UNITS_CMD, DEFAULT_PRESSURE_UNITS},
    {DEFAULT_SAMPLE_RATE_CMD, DEFAULT_SAMPLE_RATE},
    {DEFAULT_SAMPLE_AVERAGING_CMD, DEFAULT_NUM_SAMPLES_AVERAGED},
    {DEFAULT_OUTPUT_UNITS_CMD, 0},
    {DEFAULT_USE_CORRECTION_CMD, 0}
};

const int GIL2D::NUM_DEFAULT_SCIENCE_PARAMETERS = sizeof(DEFAULT_SCIENCE_PARAMETERS)/sizeof(PTB_CMD_ARG);

/* Typical GIL2D D3 query response. L1 means all line endings are \r\n
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
// NOTE: the regular expressions need to search a buffer w/multiple lines separated by \r\n
static const char* GIL2D_RESPONSE_REGEX_STR =           "";

static regex_t response;

static bool compileRegex() {
    static bool regexCompiled = false;
    int regStatus = 0;

    if (!regexCompiled) {
        regexCompiled = (regStatus = ::regcomp(&response, GIL2D_RESPONSE_REGEX_STR, REG_EXTENDED)) != 0;
        if (regexCompiled) {
            char regerrbuf[64];
            regerror(regStatus, &response, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("GIL2D version regular expression", string(regerrbuf));
        }
    }

    return regexCompiled;
}

static void freeRegex() {
    regfree(&response);
}

GIL2D::GIL2D()
    : Wind2D(DEFAULT_PORT_CONFIG), testPortConfig(), desiredPortConfig(), 
      defaultMessageConfig(DEFAULT_MESSAGE_LENGTH, DEFAULT_MSG_SEP_CHAR, DEFAULT_MSG_SEP_EOM),
      desiredScienceParameters()
{
    // We set the defaults at construction, 
    // letting the base class modify according to fromDOMElement() 
    setMessageParameters(defaultMessageConfig);

    desiredScienceParameters = new GIL2D_CMD_ARG[NUM_DEFAULT_SCIENCE_PARAMETERS];
    for (int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        desiredScienceParameters[i] = DEFAULT_SCIENCE_PARAMETERS[i];
    }

    compileRegex();
}

GIL2D::~GIL2D()
{
    freeRegex();
    delete [] desiredScienceParameters;
}

void GIL2D::fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException)
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
                // DLOG(("GIL2D:fromDOMElement(): aname: ") << aname << " upperAval: " << upperAval);

                // start with science parameters, assuming SerialSensor took care of any overrides to 
                // the default port config.
                if (aname == "units") {
                    GIL2D_UNITS_ARGS units = DEFAULT_WINDSPD_UNITS;
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
                            string("GIL2D:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_MEAS_UNITS_CMD, units);
                }
                else if (aname == "samplesaveraged") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || val < SENSOR_SAMPLE_AVG_MIN || val > SENSOR_SAMPLE_AVG_MAX)
                        throw n_u::InvalidParameterException(
                            string("GIL2D:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_NUM_SAMP_AVG_CMD, val);
                }
                else if (aname == "samplerate") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || val < SENSOR_MEAS_RATE_MIN || val > SENSOR_MEAS_RATE_MAX)
                        throw n_u::InvalidParameterException(
                            string("GIL2D:") + getName(), aname, aval);

                    updateDesiredScienceParameter(SENSOR_MEAS_RATE_CMD, val);
                }
                else if (aname == "porttype") {
                    else if (upperAval == "RS422") 
                        desiredPortConfig.xcvrConfig.portType = RS422;
                    else if (upperAval == "RS485_HALF") 
                        desiredPortConfig.xcvrConfig.portType = RS485_HALF;
                    else if (upperAval == "RS485_FULL") 
                        desiredPortConfig.xcvrConfig.portType = RS485_FULL;
                    else
                        throw n_u::InvalidParameterException(
                            string("GIL2D:") + getName(), aname, aval);
                }
                else if (aname == "baud") {
                    istringstream ist(aval);
                    int val;
                    ist >> val;
                    if (ist.fail() || !desiredPortConfig.termios.setBaudRate(val))
                        throw n_u::InvalidParameterException(
                            string("GIL2D:") + getName(), aname,aval);
                }
                else if (aname == "parity") {
                    if (upperAval == "ODD") 
                        desiredPortConfig.termios.setParity(n_u::Termios::ODD);
                    else if (upperAval == "EVEN") 
                        desiredPortConfig.termios.setParity(n_u::Termios::EVEN);
                    else if (upperAval == "NONE") 
                        desiredPortConfig.termios.setParity(n_u::Termios::NONE);
                    else throw n_u::InvalidParameterException(
                        string("GIL2D:") + getName(),
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
                        string("GIL2D:") + getName(),
                            aname, aval);
                    }
                }
            }
        }
    }
}

void GIL2D::open(int flags) throw (n_u::IOException, n_u::InvalidParameterException)
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
        NLOG(("Couldn't find a serial port configuration that worked with this GIL2D sensor. "
              "May need to troubleshoot the sensor or cable. "
              "!!!NOTE: Sensor is not open for data collection!!!"));
    }
}

bool GIL2D::findWorkingSerialPortConfig(int flags)
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

bool GIL2D::installDesiredSensorConfig()
{
    bool installed = false;
    PortConfig sensorPortConfig = getPortConfig();

    // at this point we need to determine whether or not the current working config 
    // is the desired config, and adjust as necessary
    if (desiredPortConfig != sensorPortConfig) {
        // Gotta modify the GIL2D parameters first, and the modify our parameters to match and hope for the best.
        // We only do this for the serial and science parameters, as the sensor is physically configured to use  
        // the transceiver mode we discovered it works on. To change these parameters, the user would have to  
        // physically reconfigure the sensor and re-start the auto-config process.
        DLOG(("Attempting to set the serial configuration to the desired configuration."));

        serPortFlush(O_RDWR);

        sendSensorCmd(SENSOR_SERIAL_BAUD_CMD, desiredPortConfig.termios.getBaudRate());
        
        // GIL2D only supports three combinations of word format - all based on parity
        // So just force it based on parity. Go ahead and reset now, so we can see if we're
        // still talking to each other...
        switch (desiredPortConfig.termios.getParityString(true).c_str()[0]) {
            case 'O':
                sendSensorCmd(SENSOR_SERIAL_ODD_WORD_CMD, 0, true);
                break;

            case 'E':
                sendSensorCmd(SENSOR_SERIAL_EVEN_WORD_CMD, 0, true);
                break;

            case 'N':
                sendSensorCmd(SENSOR_SERIAL_NO_WORD_CMD, 0, true);
                break;

            default:
                break;
        }

        setPortConfig(desiredPortConfig);
        applyPortConfig();
        if (getPortConfig() == desiredPortConfig) {
            // wait for the sensor to reset - ~1 second
            usleep(SENSOR_RESET_WAIT_TIME);
            if (!doubleCheckResponse()) {
                if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                    NLOG(("GIL2D::installDesiredSensorConfig() failed to achieve sensor communication "
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
                    DLOG(("GIL2D reset to original!!!"));
                    printPortConfig();
                }
            }
            else {
                if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                    NLOG(("Success!! GIL2D set to desired configuration!!!"));
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

bool GIL2D::configureScienceParameters()
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

void GIL2D::sendScienceParameters() {
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

bool GIL2D::checkScienceParameters() {

    VLOG(("GIL2D::checkScienceParameters() - Flush port and send query command"));
    // flush the serial port - read and write
    serPortFlush(O_RDWR);

    sendSensorCmd(SENSOR_CONFIG_QRY_CMD);

    static const int BUF_SIZE = 512;
    int bufRemaining = BUF_SIZE;
    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);

    VLOG(("GIL2D::checkScienceParameters() - Read the entire response"));
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

        if (LOG_LEVEL_IS_ACTIVE(LOGGER_VERBOSE)) {
            if (numCharsRead == 0) {
                VLOG(("Took ") << i+1 << " reads to get entire response");
            }
        }
    }

    if (totalCharsRead && LOG_LEVEL_IS_ACTIVE(LOGGER_VERBOSE)) {
        std::string respStr;
        respStr.append(&respBuf[0], totalCharsRead);

        VLOG(("Response: "));
        VLOG((respStr.c_str()));

    }

    VLOG(("GIL2D::checkScienceParameters() - Check the individual parameters available to us"));
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

bool GIL2D::compareScienceParameter(PTB_COMMANDS cmd, const char* match)
{
    PTB_CMD_ARG desiredCmd = getDesiredCmd(cmd);
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
            VLOG(("Arguments match: ") << (desiredCmd.arg == pressUnitStr2PressUnit(match) ? "TRUE" : "FALSE"));
            return (desiredCmd.arg == pressUnitStr2PressUnit(match));
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

                VLOG(("Arguments match: ") << (desiredCmd.arg == arg ? "TRUE" : "FALSE"));
                return (desiredCmd.arg == arg);
            }
            break;
    }

    // gotta shut the compiler up...
    return false;
}

PTB_CMD_ARG GIL2D::getDesiredCmd(PTB_COMMANDS cmd) {
    VLOG(("Looking in desiredScienceParameters[] for ") << cmd);
    for (int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if (desiredScienceParameters[i].cmd == cmd) {
            VLOG(("Found command: ") << cmd);
            return desiredScienceParameters[i];
        }
    }

    VLOG(("Requested cmd not found: ") << cmd);

    PTB_CMD_ARG nullRetVal = {NULL_COMMAND, 0};
    return(nullRetVal);
}


bool GIL2D::testDefaultPortConfig()
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

bool GIL2D::sweepParameters(bool defaultTested)
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

void GIL2D::setTargetPortConfig(PortConfig& target, int baud, int dataBits, Termios::parity parity, int stopBits, 
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

bool GIL2D::isDefaultConfig(const n_c::PortConfig& target)
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

bool GIL2D::doubleCheckResponse()
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

bool GIL2D::checkResponse()
{
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
        int foundPos = 0;
        bool retVal = (foundPos = respStr.find(GIL2D_RESPONSER_REGEX_STR, foundPos) != string::npos);
        VLOG(("GIL2D::checkResponse() - Check the general format of the query response"));
        int regexStatus = -1;
        regmatch_t matches[1];
        int nmatch = sizeof(matches) / sizeof(regmatch_t);
        
        // check for sample averaging
        retVal = ((regexec(&response, &(respBuf[0]), 1, matches, 0) == 0) && matches[0].rm_so >= 0);

        if (!retVal) {
            char regerrbuf[64];
            ::regerror(regexStatus, &response, regerrbuf, sizeof regerrbuf);
            DLOG(("GIL2d::checkResponse() regex failed: ") << std::string(regerrbuf));
        }

        return retVal;
    }

    else {
        DLOG(("Didn't get any chars from serial port"));
        return false;
    }
}


void GIL2D::sendSensorCmd(PTB_COMMANDS cmd, int arg)
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
                argStr << arg;
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

size_t GIL2D::readResponse(void *buf, size_t len, int msecTimeout)
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

void GIL2D::updateDesiredScienceParameter(PTB_COMMANDS cmd, int arg) {
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
            desiredScienceParameters[i].arg = arg;
            break;
        }
    }
}



}}} //namespace nidas { namespace dynld { namespace isff {

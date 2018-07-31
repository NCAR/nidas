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

#include <sstream>
#include <limits>

using namespace nidas::core;
using namespace std;

NIDAS_CREATOR_FUNCTION_NS(isff,PTB210)

namespace nidas { namespace dynld { namespace isff {

const char* PTB210::DEFAULT_MSG_SEP_CHARS = "\r\n";

// 9600 baud, even parity, 7 data bits, one stop bit, rate 1/sec, no averaging, 
// units: millibars, no units reported, multipoint cal on, term off
const char* PTB210::DEFAULT_SENSOR_INIT_CMD_STR = 
                "\r.BAUD.9600\r.E71\r.MPM.60\r.AVRG.0\r.UNIT.1\r"
                ".FORM0\r.MPCON\r.ROFF\r.RESET\r";
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
    DEFAULT_SENSOR_INIT_CMD_STR,
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
    SENSOR_EXC_UNIT_CMD_STR,
    SENSOR_INC_UNIT_CMD_STR,
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
                                             PTB210::DEFAULT_SENSOR_POWER, PTB210::DEFAULT_RTS485, 
                                             PTB210::DEFAULT_CONFIG_APPLIED);

PTB210::PTB210()
    : SerialSensor(DEFAULT_PORT_CONFIG), testPortConfig(), desiredPortConfig(), 
      defaultMessageConfig(DEFAULT_MESSAGE_LENGTH, DEFAULT_MSG_SEP_CHARS, DEFAULT_MSG_SEP_EOM)
{
    // We set the defaults at construction, 
    // letting the base class modify according to fromDOMElement() 
    setMessageParameters(defaultMessageConfig);
}

PTB210::~PTB210()
{
}

void PTB210::open(int flags) throw (n_u::IOException, n_u::InvalidParameterException)
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
        NLOG(("Couldn't find a serial port configuration that worked with this PTB210 sensor. "
              "May need to troubleshoot the sensor or cable. "
              "!!!NOTE: Sensor is not open for data collection!!!"));
    }
}

bool PTB210::findWorkingSerialPortConfig(int flags)
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
    ILOG(("Raw mode is ") << (desiredPortConfig.termios.getRaw() ? "ON" : "OFF"));

    if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
        NLOG(("Testing initial config which may be custom "));
        printPortConfig();
    }

    if (!checkResponse()) {
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

bool PTB210::installDesiredSensorConfig()
{
    bool installed = false;
    PortConfig sensorPortConfig = getPortConfig();

    // at this point we need to determine whether or not the current working config 
    // is the desired config, and adjust as necessary
    if (desiredPortConfig != sensorPortConfig) {
        // Gotta modify the PTB210 parameters first, and the modify our parameters to match and hope for the best.
        // We only do this for the serial and science parameters, as the sensor is physically configured to use  
        // the transceiver mode we discovered it works on. To change these parameters, the user would have to  
        // physically reconfigure the sensor and re-start the auto-config process.
        ILOG(("Attempting to set the serial configuration to the desired configuration."));

        serPortFlush(O_RDWR);

        sendSensorCmd(SENSOR_SERIAL_BAUD_CMD, desiredPortConfig.termios.getBaudRate());
        
        // PTB210 only supports three combinations of word format - all based on parity
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
            usleep(DEFAULT_RESET_WAIT_TIME);
            if (!checkResponse()) {
                if (LOG_LEVEL_IS_ACTIVE(LOGGER_INFO)) {
                    ILOG(("PTB210::installDesiredSensorConfig() failed to achieve sensor communication "
                            "after setting desired serial port parameters. This is the current PortConfig"));
                    printPortConfig();
                }

                setPortConfig(sensorPortConfig);
                applyPortConfig();

                if (LOG_LEVEL_IS_ACTIVE(LOGGER_INFO)) {
                    ILOG(("Setting the port config back to something that works for a retry"));
                    printPortConfig();
                }
                
                if (!checkResponse()) {
                    ILOG(("The sensor port config which originally worked before attempting "
                          "to set the desired config no longer works. Really messed up now!"));
                }

                else if (LOG_LEVEL_IS_ACTIVE(LOGGER_INFO)) {
                    ILOG(("PTB210 reset to original!!!"));
                    printPortConfig();
                }
            }
            else {
                if (LOG_LEVEL_IS_ACTIVE(LOGGER_INFO)) {
                    ILOG(("Success!! PTB210 set to desired configuration!!!"));
                    printPortConfig();
                }
                installed = true;
            }
        }

        else if (LOG_LEVEL_IS_ACTIVE(LOGGER_INFO)) {
            ILOG(("Attempt to set PortConfig to desiredPortConfig failed."));
            ILOG(("Desired PortConfig: "));
            printTargetConfig(desiredPortConfig);
            ILOG(("Actual set PortConfig: "));
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

    NLOG(("Returning installed status: ") << (installed ? "SUCCESS!!" : "failed..."));
    return installed;
}

bool PTB210::configureScienceParameters() 
{
    return false;
}

bool PTB210::testDefaultPortConfig()
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
    return checkResponse();
}

bool PTB210::sweepParameters(bool defaultTested)
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

                if (LOG_LEVEL_IS_ACTIVE(LOGGER_INFO)) {
                    ILOG(("Asking for PortConfig:"));
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

                NLOG(("Checking response once..."));
                if (checkResponse()) {
                    if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                        // tell everyone
                        NLOG(("Found working port config: "));
                        printPortConfig();
                    }

                    foundIt = true;
                    return foundIt;
                } 
                else {
                    NLOG(("Checking response twice..."));
                    if (checkResponse()) {
                        // tell everyone
                        NLOG(("Response checks out on second try..."));
                        if (LOG_LEVEL_IS_ACTIVE(LOGGER_INFO)) {
                            ILOG(("Found working port config: "));
                            printPortConfig();
                        }

                        foundIt = true;
                        return foundIt;
                    }
                    else {
                        NLOG(("Checked response twice, and failed twice."));
                        if (portType == n_c::RS485_HALF || portType == n_c::RS422) {
                            // test the connection w/termination turned on.
                            setTargetPortConfig(testPortConfig, baud, wordSpec.dataBits, wordSpec.parity,
                                                                wordSpec.stopBits, rts485, portType, TERM_120_OHM, 
                                                                DEFAULT_SENSOR_POWER);
                            if (LOG_LEVEL_IS_ACTIVE(LOGGER_INFO)) {
                                ILOG(("Asking for PortConfig:"));
                                printTargetConfig(testPortConfig);
                            }

                            setPortConfig(testPortConfig);
                            applyPortConfig();

                            if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                                NLOG(("Testing PortConfig on RS422/RS485 with termination: "));
                                printPortConfig();
                            }

                            if (checkResponse()) {
                                // tell everyone
                                if (LOG_LEVEL_IS_ACTIVE(LOGGER_NOTICE)) {
                                    NLOG(("Found working port config: "));
                                    printPortConfig();
                                }
                                
                                foundIt = true;
                                return foundIt;
                            }
                        }
                    }
                }
            }
        }
    }

    return foundIt;
}

void PTB210::setTargetPortConfig(PortConfig& target, int baud, int dataBits, Termios::parity parity, int stopBits, 
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

bool PTB210::isDefaultConfig(const n_c::PortConfig& target)
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

    if (LOG_LEVEL_IS_ACTIVE(LOGGER_INFO)) {
        if (numCharsRead > 0) {
            ILOG(("Initial num chars read is: ") << numCharsRead << " comprised of: ");
            for (int i=0; i<5; ++i) {
                char hexBuf[60];
                memset(hexBuf, 0, 60);
                for (int j=0; j<10; ++j) {
                    snprintf(&(hexBuf[j*6]), 6, "%-#.2x     ", respBuf[(i*10)+j]);
                }
                ILOG((&(hexBuf[0])));
            }
        }
    }
    
    for (int i=0; (numCharsRead > 0 && bufRemaining > 0); ++i) {
        numCharsRead = readResponse(&(respBuf[totalCharsRead]), bufRemaining, 2000);
        totalCharsRead += numCharsRead;
        bufRemaining -= numCharsRead;

        if (LOG_LEVEL_IS_ACTIVE(LOGGER_INFO)) {
            if (numCharsRead == 0) {
                ILOG(("Took ") << i+1 << " reads to get entire response");
            }
        }
    }

    if (totalCharsRead) {
        std::string respStr;
        respStr.append(&respBuf[0], totalCharsRead);

        ILOG(("Response: "));
        ILOG((respStr.c_str()));

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
                                                    ILOG(("Coundn't find ") << "\"" << PTB210_RS485_RES_STR << "\"");
                                            }
                                            else
                                                ILOG(("Coundn't find ") << "\"" << PTB210_CURR_MODE_STR << "\"");
                                        }
                                        else
                                            ILOG(("Coundn't find ") << "\"" << PTB210_PRESS_MINMAX_STR << "\"");
                                    }
                                    else
                                        ILOG(("Coundn't find ") << "\"" << PTB210_PRESS_UNIT_STR << "\"");
                                }
                                else
                                    ILOG(("Coundn't find ") << "\"" << PTB210_NUM_SMPLS_AVG_STR << "\"");
                            }
                            else
                                ILOG(("Coundn't find ") << "\"" << PTB210_MEAS_PER_MIN_STR << "\"");
                        }
                        else
                            ILOG(("Coundn't find ") << "\"" << PTB210_MULTI_PT_CORR_STR << "\"");
                    }
                    else
                        ILOG(("Coundn't find ") << "\"" << PTB210_SERIAL_NUMBER_STR << "\"");
                }
                else
                    ILOG(("Coundn't find ") << "\"" << PTB210_ID_CODE_STR << "\"");
            }
            else
                ILOG(("Coundn't find ") << "\"" << PTB210_CAL_DATE_STR << "\"");
        }
        else
            ILOG(("Coundn't find ") << "\"" << PTB210_VER_STR << "\"");

        return retVal;
    }

    else {
        NLOG(("Didn't get any chars from serial port"));
        return false;
    }
}


void PTB210::sendSensorCmd(PTB_COMMANDS cmd, int arg, bool resetNow)
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

        case DEFAULT_SENSOR_INIT_CMD:
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
    ILOG(("Sending command: "));
    ILOG((snsrCmd.c_str()));
    for (unsigned int i=0; i<snsrCmd.length(); ++i) {
        write(&(snsrCmd.c_str()[i]), 1);
        usleep(USECS_PER_SEC/10);
    }
    ILOG(("write() sent ") << snsrCmd.length());;

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

        case DEFAULT_SENSOR_INIT_CMD:
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

size_t PTB210::readResponse(void *buf, size_t len, int msecTimeout)
{
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(getReadFd(), &fdset);

    struct timeval tmpto = { msecTimeout / MSECS_PER_SEC,
        (msecTimeout % MSECS_PER_SEC) * USECS_PER_MSEC };

    int res = ::select(getReadFd()+1,&fdset,0,0,&tmpto);

    if (res < 0) {
        NLOG(("General select error on: ") << getDeviceName() << ": error: " << errno);
        return -1;
    }

    if (res == 0) {
        ILOG(("Select timeout on: ") << getDeviceName() << ": " << msecTimeout << " msec");
        return 0;
    }

    // no select timeout or error, so get the goodies out of the buffer...
    return read(buf,len);
}


}}} //namespace nidas { namespace dynld { namespace isff {

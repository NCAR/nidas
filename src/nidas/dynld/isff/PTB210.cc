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

struct WordSpec
{
    int dataBits;
    Termios::parity parity;
    int stopBits;
};

// lists of serial port parametners, arranged in most to least likely.
static const int NUM_BAUDS = 5;
static const int bauds[NUM_BAUDS] = {38400, 19200, 9600, 4800, 2400};
static const int NUM_WORDSPEC = 3;
static const WordSpec wordSpec[NUM_WORDSPEC] = {
    {7,Termios::EVEN,1}, 
    {7,Termios::ODD,1}, 
    {8,Termios::EVEN,1}
};
static const int NUM_PORT_TYPES = 3;
static const n_c::PORT_TYPES portTypes[NUM_PORT_TYPES] = {n_c::RS232, n_c::RS422, n_c::RS485_HALF };

const char* PTB210::DEFAULT_SEP_CHARS = "\n";

// 9600 baud, even parity, 7 data bits, one stop bit, rate 1/sec, no averaging, 
// units: millibars, no units reported, multipoint cal on, term off
const char* PTB210::DEFAULT_SENSOR_INIT_CMD_STR = 
                "\r\n.BAUD.9600\r\n.E71\r\n.MPM.60\r\n.AVRG.0\r\n.UNIT.1\r\n"
                ".FORM0\r\n.MPCON\r\n.ROFF\r\n.RESET\r\n";
const char* PTB210::SENSOR_RESET_CMD_STR = ".RESET\r\n";
const char* PTB210::SENSOR_SERIAL_BAUD_CMD_STR = ".BAUD.\r\n";
const char* PTB210::SENSOR_SERIAL_EVENP_WORD_CMD_STR = ".E71";
const char* PTB210::SENSOR_SERIAL_ODDP_WORD_CMD_STR = ".O71";
const char* PTB210::SENSOR_SERIAL_NOP_WORD_CMD_STR = ".N81";
const char* PTB210::SENSOR_PRESS_MIN_CMD_STR = ".PMIN.\r\n";
const char* PTB210::SENSOR_PRESS_MAX_CMD_STR = ".PMAX.\r\n";
const char* PTB210::SENSOR_MEAS_RATE_CMD_STR = ".MPM.\r\n";
const char* PTB210::SENSOR_NUM_SAMP_AVG_CMD_STR = ".AVRG.\r\n";
const char* PTB210::SENSOR_POWER_DOWN_CMD_STR = ".PD\r\n";
const char* PTB210::SENSOR_POWER_UP_CMD_STR = "\r\n";
const char* PTB210::SENSOR_SINGLE_SAMP_CMD_STR = ".P\r\n";
const char* PTB210::SENSOR_START_CONT_SAMP_CMD_STR = ".BP\r\n";
const char* PTB210::SENSOR_STOP_CONT_SAMP_CMD_STR = "\r\n";
const char* PTB210::SENSOR_SAMP_UNIT_CMD_STR = ".UNIT.\r\n";
const char* PTB210::SENSOR_EXC_UNIT_CMD_STR = ".FORM.0";
const char* PTB210::SENSOR_INC_UNIT_CMD_STR = ".FORM.1";
const char* PTB210::SENSOR_CORRECTION_ON_CMD_STR = ".MPCON\r\n";
const char* PTB210::SENSOR_CORRECTION_OFF_CMD_STR = ".MPCOFF\r\n";
const char* PTB210::SENSOR_TERM_ON_CMD_STR = ".RON\r\n";
const char* PTB210::SENSOR_TERM_OFF_CMD_STR = ".ROFF\r\n";
const char* PTB210::SENSOR_CONFIG_QRY_CMD_STR = "\r\n.?\r\n";

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

// static default configuration to send to base class...
const PortConfig PTB210::DEFAULT_PORT_CONFIG(PTB210::DEFAULT_BAUD_RATE, PTB210::DEFAULT_DATA_BITS,
                                             PTB210::DEFAULT_PARITY, PTB210::DEFAULT_STOP_BITS,
                                             PTB210::DEFAULT_PORT_TYPE, PTB210::DEFAULT_SENSOR_TERMINATION, 
                                             PTB210::DEFAULT_SENSOR_POWER, PTB210::DEFAULT_RTS485, 
                                             PTB210::DEFAULT_CONFIG_APPLIED);

PTB210::PTB210()
    : SerialSensor(DEFAULT_PORT_CONFIG), testPortConfig(), desiredPortConfig(), 
      defaultMessageConfig(DEFAULT_MESSAGE_LENGTH, DEFAULT_SEP_CHARS, DEFAULT_SEP_EOM)
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
    ILOG(("first figure out whether we're talking to the sensor"));
    if (findWorkingSerialPortConfig(flags)) {
        if (installDesiredSensorConfig()) {
            configureScienceParameters();
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
    
    DSMSensor::open(flags); // this could result in an infinite loop, if polymorphism holds????
    n_c::SerialPortIODevice* pSIODevice = dynamic_cast<n_c::SerialPortIODevice*>(getIODevice());
    applyPortConfig();
    // Make sure blocking is set properly
    pSIODevice->getBlocking();
    // Save off desiredConfig - base class should have modified it by now.
    // Do this after applying, as getPortConfig() only gets the items in the SerialPortIODevice object.
    desiredPortConfig = getPortConfig();


    std::cout << "Testing initial config which may be custom " << std::endl;
    printPortConfig();

    if (!checkResponse()) {
        // initial config didn't work, so sweep through all parameters starting w/the default
        if (!isDefaultConfig(getPortConfig())) {
            // it's a custom config, so test default first
            std::cout << "Testing default config because SerialSensor applied a custom config" << std::endl;
            if (!testDefaultPortConfig()) {
                std::cout << "Default PortConfig failed. Now testing all the other serial parameter configurations..." << std::endl;
                foundIt = sweepParameters(true);
            }
            else {
                // found it!! Tell someone!!
                foundIt = true;
                cout << "Default PortConfig was successfull!!!" << std::endl;
                printPortConfig();
            }
        }
        else {
            std::cout << "Default PortConfig was not changed and failed. Now testing all the other serial parameter configurations..." << std::endl;
            foundIt = sweepParameters(true);
        }
    }
    else {
        // Found it! Tell someone!
        if (!isDefaultConfig(getPortConfig())) {
            std::cout << "SerialSensor customimized the PortConfig and it succeeded!!" << std::endl;
        }
        else {
            std::cout << "SerialSensor did not customimize the PortConfig (default config) and it succeeded!!" << std::endl;
        }

        foundIt = true;
        printPortConfig();
    }

    return foundIt;
}

bool PTB210::installDesiredSensorConfig()
{
    bool installed = false;

    // at this point we need to determine whether or not the current working config 
    // is the desired config, and adjust as necessary
    if (desiredPortConfig != getPortConfig()) {
        // Gotta modify the PTB210 parameters first, and the modify our parameters to match and hope for the best.
        // We only do this for the serial and science parameters, as the sensor is physically configured to use  
        // the transceiver mode we discovered it works on. To change these parameters, the user would have to  
        // physically reconfigure the sensor and re-start the auto-config process.

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

        SerialSensor::setPortConfig(desiredPortConfig);
        applyPortConfig();
        // wait for the sensor to reset - ~1 second
        usleep(USECS_PER_SEC*1.5);
        if (!checkResponse()) {
            cout << "PTB210::installDesiredSensorConfig() failed to achieve sensor communication "
                    "after setting desired serial port parameters." << endl;
        }
        else {
            installed = true;
        }
    }

    else {
        installed = true;
    }

    return installed;
}

void PTB210::configureScienceParameters() 
{

}

bool PTB210::testDefaultPortConfig()
{
    // set up the local PortConfig...
    setPortConfig(testPortConfig, DEFAULT_BAUD_RATE, DEFAULT_DATA_BITS, DEFAULT_PARITY, DEFAULT_STOP_BITS, DEFAULT_RTS485, 
                                     DEFAULT_PORT_TYPE, DEFAULT_SENSOR_TERMINATION, DEFAULT_SENSOR_POWER);
    SerialSensor::setPortConfig(testPortConfig);
    applyPortConfig();
    return checkResponse();
}

bool PTB210::sweepParameters(bool defaultTested)
{
    bool foundIt = false;

    // no point if sensor is turned off...
    testPortConfig.xcvrConfig.sensorPower = DEFAULT_SENSOR_POWER;

    for (int i=0; i<NUM_PORT_TYPES; ++i) {
        int rts485 = 0;
        if (portTypes[i] == n_c::RS485_HALF)
            rts485 = -1; // start low. Let write manage setting high
        else if (portTypes[i] == n_c::RS422)
            rts485 = -1; // always high, since there are two drivers going both ways

        for (int j=0; j<NUM_BAUDS; ++j) {
            for (int k=0; k<NUM_WORDSPEC; ++k) {
                setPortConfig(testPortConfig, bauds[j], wordSpec[k].dataBits, wordSpec[k].parity,
                                              wordSpec[k].stopBits, rts485, portTypes[i], NO_TERM, 
                                              DEFAULT_SENSOR_POWER);

                cout << "Asking for PortConfig:" << endl;
                printPortConfig();
                cout << endl << std::flush;

                // don't test the default if already tested.
                if (defaultTested && isDefaultConfig(testPortConfig))
                {
                    // skip
                    std::cout << "Skipping default configuration since it's already tested..." << std::endl << std::endl;
                    continue;
                }

                SerialSensor::setPortConfig(testPortConfig);
                applyPortConfig();
                std::cout << "Testing PortConfig: " << std::endl;
                printPortConfig();
                cout << endl << std::flush;

                // sendSensorCmd(SENSOR_CONFIG_QRY_CMD);
                if (checkResponse()) {
                    foundIt = true;
                    cout << "Found working port config: " << endl;
                    // tell everyone
                    printPortConfig();
                    cout << endl << std::flush;
                    return foundIt;
                }
                else if (portTypes[i] == n_c::RS485_HALF || portTypes[i] == n_c::RS422) {
                    // test the connection w/termination turned on.
                    setPortConfig(testPortConfig, bauds[j], wordSpec[k].dataBits, wordSpec[k].parity,
                                                wordSpec[k].stopBits, rts485, portTypes[i], TERM_120_OHM, 
                                                DEFAULT_SENSOR_POWER);
                    cout << "Asking for PortConfig:" << endl;
                    printPortConfig();
                    cout << endl << std::flush;

                    SerialSensor::setPortConfig(testPortConfig);
                    applyPortConfig();
                    std::cout << "Testing PortConfig on RS422/RS485 with termination: " << std::endl;
                    printPortConfig();
                    cout << endl << std::flush;
                    // sendSensorCmd(SENSOR_CONFIG_QRY_CMD);
                    if (checkResponse()) {
                        foundIt = true;
                        // tell everyone
                        cout << "Found working port config: " << endl;
                        printPortConfig();
                        return foundIt;
                    }
                }

                if (testPortConfig.termios.getBaudRate() == 9600
                    && testPortConfig.termios.getDataBits() == 7
                    && testPortConfig.termios.getParityString(true).c_str()[0] == 'E'
                    && testPortConfig.termios.getStopBits() == 1
                    && testPortConfig.xcvrConfig.portType == n_c::RS232) {
                        std::cout << "Let's snooze on what should be the winning candidate for a second." << std::endl << std::endl << std::flush;
                        usleep(USECS_PER_SEC*10);
                    }
            }
        }
    }

    return foundIt;
}

void PTB210::setPortConfig(PortConfig& target, int baud, int dataBits, Termios::parity parity, int stopBits, int rts485,
                                               n_c::PORT_TYPES portType, n_c::TERM termination, n_c::SENSOR_POWER_STATE power)
{
    target.termios.setBaudRate(baud);
    target.termios.setDataBits(dataBits);
    target.termios.setParity(parity);
    target.termios.setStopBits(stopBits);
    target.rts485 = (rts485);
    target.xcvrConfig.portType = portType;
    target.xcvrConfig.termination = termination;
    target.xcvrConfig.sensorPower = power;
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

    char respBuf[256];

    int numCharsRead = readResponse(&respBuf, 256, 2000);
    int totalCharsRead = numCharsRead;
    while (numCharsRead > 0) {
        numCharsRead = readResponse(&respBuf[totalCharsRead], 256-totalCharsRead, 2000);
        totalCharsRead += numCharsRead;
    }

    if (totalCharsRead) {
        std::string respStr;
        respStr.append(&respBuf[0], totalCharsRead);

        cout << "Response: " << endl << respStr << endl;

        int foundPos = 0;
        char* endLinePos = strstr(respBuf, "\n");
        cout << string(&respBuf[0], endLinePos) << endl;

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
                                                if (!retVal) {
                                                    cout << "Coundn't find " << "\"" << PTB210_RS485_RES_STR << "\"" << endl;
                                                }
                                            }
                                            else {
                                                cout << "Coundn't find " << "\"" << PTB210_CURR_MODE_STR << "\"" << endl;
                                            }
                                        }
                                        else {
                                            cout << "Coundn't find " << "\"" << PTB210_PRESS_MINMAX_STR << "\"" << endl;
                                        }
                                    }
                                    else {
                                        cout << "Coundn't find " << "\"" << PTB210_PRESS_UNIT_STR << "\"" << endl;
                                    }
                                }
                                else {
                                    cout << "Coundn't find " << "\"" << PTB210_NUM_SMPLS_AVG_STR << "\"" << endl;
                                }
                            }
                            else {
                                cout << "Coundn't find " << "\"" << PTB210_MEAS_PER_MIN_STR << "\"" << endl;
                            }
                        }
                        else {
                            cout << "Coundn't find " << "\"" << PTB210_MULTI_PT_CORR_STR << "\"" << endl;
                        }
                    }
                    else {
                        cout << "Coundn't find " << "\"" << PTB210_SERIAL_NUMBER_STR << "\"" << endl;
                    }
                }
                else {
                    cout << "Coundn't find " << "\"" << PTB210_ID_CODE_STR << "\"" << endl;
                }
            }
            else {
                cout << "Coundn't find " << "\"" << PTB210_CAL_DATE_STR << "\"" << endl;
            }
        }
        else {
            cout << "Coundn't find " << "\"" << PTB210_VER_STR << "\"" << endl;
        }

        return retVal;
    }

    else {
        cout << "Didn't get any chars from serial port" << endl;
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
                snsrCmd.insert(snsrCmd[insertIdx], argStr.str());
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

    // write the command - assume the port is already open
    size_t numCharsWritten = write(snsrCmd.c_str(), snsrCmd.length());
    cout << "write() sent " << numCharsWritten << endl;

    // Want to send a reset command for those that require it to take effect
    switch (cmd) {
        // these commands all take an argument...
        case SENSOR_SERIAL_BAUD_CMD:
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
        case SENSOR_SERIAL_EVEN_WORD_CMD:
        case SENSOR_SERIAL_ODD_WORD_CMD:
        case SENSOR_SERIAL_NO_WORD_CMD:
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

    int res;

    if ((res = ::select(getReadFd()+1,&fdset,0,0,&tmpto)) < 0) {
        std::cout << "General select error on: " << getDeviceName() << ": error: " << errno << std::endl << std::endl;
        return -1;
    }

    if (res == 0) {
        std::cout << "Select timeout on: " << getDeviceName() << ": " << msecTimeout << " msec" << std::endl << std::endl;
        return 0;
    }

    return read(buf,len);
}


}}} //namespace nidas { namespace dynld { namespace isff {

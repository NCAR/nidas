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
static const n_c::PORT_TYPES portTypes[NUM_PORT_TYPES] = {n_c::RS485_HALF, n_c::RS422, n_c::RS232};

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

PTB210::PTB210()
    : SerialSensor(), workingPortConfig(), testPortConfig(), 
      desiredPortConfig(), defaultMessageConfig(0, "\n", true)
{
    // We set the defaults at construction, 
    // letting the base class modify according to fromDOMElement() 
    setDefaultPortConfig();
    setMessageParameters(defaultMessageConfig);
}

PTB210::~PTB210()
{
}

void PTB210::open(int flags) throw (n_u::IOException, n_u::InvalidParameterException)
{
    ILOG(("first figure out whether we're talking to the sensor"));
    findWorkingSerialPortConfig(flags);

    ILOG(("Must've found something that works, so invoking SerialSensor::open(flags)"));
    // complete the rest of the open process...
    SerialSensor::open(flags);
}

void PTB210::findWorkingSerialPortConfig(int flags)
{
    // Save off desiredConfig - base class should have modified it by now.
    desiredPortConfig = workingPortConfig;

    // first see if the current configuration is working. If so, all done!
    // So open the device at a the base class so we don't recurse ourselves to death...
    
    DSMSensor::open(flags); // this could result in an infinite loop, if polymorphism holds????
    n_c::SerialPortIODevice* pSIODevice = dynamic_cast<n_c::SerialPortIODevice*>(getIODevice());
    applyPortConfig();

    // sendSensorCmd(SENSOR_CONFIG_QRY_CMD);
    if (!checkResponse()) {
        // initial config didn't work, so sweep through all parameters starting w/the default
        if (!isDefaultConfig(workingPortConfig)) {
            // it's a custom config, so test default first
            std::cout << "Testing default config because SerialSensor applied a custom config" << std::endl;
            if (!testDefaultPortConfig()) {
                std::cout << "PortConfig tested: " << std::endl;
                pSIODevice->printPortConfig();

                std::cout << "Default PortConfig failed. Now testing all the other serial parameter configurations..." << std::endl;
                sweepParameters(true);
            }
            else {
                // found it!! Tell someone!!
                cout << "Default PortConfig was successfull!!!" << std::endl;
                pSIODevice->printPortConfig();
            }
        }
        else {
            std::cout << "Default PortConfig was not changed and failed. Now testing all the other serial parameter configurations..." << std::endl;
            sweepParameters(true);
        }
    }
    else {
        // Found it! Tell someone!
        if (!isDefaultConfig(workingPortConfig)) {
            std::cout << "SerialSensor customimized the PortConfig and it succeeded!!" << std::endl;
        }
        else {
            std::cout << "SerialSensor did not customimize the PortConfig (default config) and it succeeded!!" << std::endl;
        }
        pSIODevice->printPortConfig();
    }
}

bool PTB210::testDefaultPortConfig()
{
    setDefaultPortConfig();
    // sendSensorCmd(SENSOR_CONFIG_QRY_CMD);
    return checkResponse();
}

void PTB210::sweepParameters(bool defaultTested)
{
    SerialPortIODevice* pSIODevice = dynamic_cast<SerialPortIODevice*>(getIODevice());

    // no point if sensor is turned off...
    testPortConfig.xcvrConfig.sensorPower = DEFAULT_SENSOR_POWER;

    for (int i=0; i<NUM_PORT_TYPES; ++i) {
        int rts485 = 0;
        if (portTypes[i] == n_c::RS485_HALF)
            rts485 = 1; // start low. Let write manage setting high
        else if (portTypes[i] == n_c::RS422)
            rts485 = -1; // always high, since there are two drivers going both ways

        for (int j=0; j<NUM_BAUDS; ++j) {
            for (int k=0; k<NUM_WORDSPEC; ++k) {
                setPortConfig(testPortConfig, bauds[j], wordSpec[k].dataBits, wordSpec[k].parity,
                                              wordSpec[k].stopBits, rts485, portTypes[i], NO_TERM, 
                                              DEFAULT_SENSOR_POWER);

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
                pSIODevice->printPortConfig();
                // sendSensorCmd(SENSOR_CONFIG_QRY_CMD);
                if (checkResponse()) {
                    // working with test
                    workingPortConfig = testPortConfig;
                    // tell everyone
                    pSIODevice->printPortConfig();
                    break;
                }
                else if (portTypes[i] == n_c::RS485_HALF || portTypes[i] == n_c::RS422) {
                    // test the connection w/termination turned on.
                    setPortConfig(testPortConfig, bauds[j], wordSpec[k].dataBits, wordSpec[k].parity,
                                                wordSpec[k].stopBits, rts485, portTypes[i], TERM_120_OHM, 
                                                DEFAULT_SENSOR_POWER);
                    SerialSensor::setPortConfig(testPortConfig);
                    applyPortConfig();
                    std::cout << "Testing PortConfig on RS422/RS485 with termination: " << std::endl;
                    pSIODevice->printPortConfig();
                    // sendSensorCmd(SENSOR_CONFIG_QRY_CMD);
                    if (checkResponse()) {
                        // copy test to working
                        workingPortConfig = testPortConfig;
                        // tell everyone
                        pSIODevice->printPortConfig();
                        break;
                    }
                }
            }
        }
    }
}

void PTB210::setDefaultPortConfig()
{
    // set up the local PortConfig...
    setPortConfig(workingPortConfig, DEFAULT_BAUD_RATE, DEFAULT_DATA_BITS, DEFAULT_PARITY, DEFAULT_STOP_BITS, DEFAULT_RTS485, 
                                     DEFAULT_PORT_TYPE, DEFAULT_SENSOR_TERMINATION, DEFAULT_SENSOR_POWER);

    // send local PortConfig to the SerialSensor base class PortConfig
    // fromDOMElement() may come by later and add it's own ideas later
    // and that is expected.
    SerialSensor::setPortConfig(workingPortConfig);
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
    static const char* PTB210_CAL_DATE_STR =      "CAL DATE       :";
    static const char* PTB210_SERIAL_NUMBER_STR = "SERIAL NUMBER  :";
    static const char* PTB210_MULTI_PT_CORR_STR = "MULTIPOINT CORR:";
    static const char* PTB210_MEAS_PER_MIN_STR =  "MEAS PER MINUTE:";
    static const char* PTB210_NUM_SMPLS_AVG_STR = "AVERAGING      :";
    static const char* PTB210_PRESS_UNIT_STR =    "PRESSURE UNIT  :";
    static const char* PTB210_PRESS_MINMAX_STR =  "Pressure Min...Max:";
    static const char* PTB210_CURR_MODE_STR =     "CURRENT MODE";
    static const char* PTB210_RS485_RES_STR =     "RS485 RESISTOR";

    // wait for data...
    SerialPortIODevice* pSIODevice = dynamic_cast<SerialPortIODevice*>(getIODevice());
    int fd = pSIODevice->getFd();
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd,&readfds);
    int nfds = fd + 1;
    struct timespec timeout;
    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;
    fd_set fds = readfds;

    sendSensorCmd(SENSOR_CONFIG_QRY_CMD);

    int nfd = pselect(nfds,&fds,0,0,&timeout, 0);

    if (nfd < 0) {
        if (errno == EINTR) 
            return false;
        cout << "Select on device: " << getDeviceName() << ": general error: " << errno << std::endl;
        throw n_u::IOException(getDeviceName(),"select",errno);
    }
    if (nfd == 0) {
        cout << "Select on device: " << getDeviceName() << ": timed out: " << timeout.tv_sec <<
            " seconds" << endl << endl;
        return false;
    }

    cout << "Found something! Let's go read it!" << endl;
    
    char respBuf[256];
    // need to test numCharsRead?
    int numCharsRead = ::read(pSIODevice->getFd(), &respBuf, 256);
    std::string respStr(respBuf);

    cout << "Response: " << respStr << endl;

    return (respStr.find(PTB210_VER_STR) && respStr.find(PTB210_CAL_DATE_STR) && 
            respStr.find(PTB210_SERIAL_NUMBER_STR) && respStr.find(PTB210_MULTI_PT_CORR_STR) && 
            respStr.find(PTB210_MEAS_PER_MIN_STR) && respStr.find(PTB210_NUM_SMPLS_AVG_STR) && 
            respStr.find(PTB210_PRESS_UNIT_STR) && respStr.find(PTB210_PRESS_MINMAX_STR) && 
            respStr.find(PTB210_CURR_MODE_STR) && respStr.find(PTB210_RS485_RES_STR));
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
    write(cmdTable[cmd], sizeof(cmdTable[cmd]));

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

}}} //namespace nidas { namespace dynld { namespace isff {

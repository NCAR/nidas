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

#ifndef NIDAS_DYNLD_ISFF_PTB_210_H
#define NIDAS_DYNLD_ISFF_PTB_210_H

#include <nidas/core/SerialSensor.h>

namespace n_c = nidas::core;
namespace n_u = nidas::util;

namespace nidas { namespace dynld { namespace isff {

struct WordSpec
{
    int dataBits;
    Termios::parity parity;
    int stopBits;
};



// create table indices
enum PTB_COMMANDS
{   DEFAULT_SENSOR_INIT_CMD,
    SENSOR_RESET_CMD,
	SENSOR_SERIAL_BAUD_CMD,
	SENSOR_SERIAL_EVEN_WORD_CMD,
	SENSOR_SERIAL_ODD_WORD_CMD,
	SENSOR_SERIAL_NO_WORD_CMD,
	SENSOR_MEAS_RATE_CMD,
	SENSOR_NUM_SAMP_AVG_CMD,
	SENSOR_PRESS_MIN_CMD,
	SENSOR_PRESS_MAX_CMD,
	SENSOR_SINGLE_SAMP_CMD,
	SENSOR_START_CONT_SAMP_CMD,
	SENSOR_STOP_CONT_SAMP_CMD,
	SENSOR_POWER_DOWN_CMD,
	SENSOR_POWER_UP_CMD,
	SENSOR_SAMP_UNIT_CMD,
	SENSOR_INC_UNIT_CMD,
	SENSOR_EXC_UNIT_CMD,
	SENSOR_CORRECTION_ON_CMD,
	SENSOR_CORRECTION_OFF_CMD,
    // No way to set the calibration points, so no need to set the Cal date.
    // SENSOR_SET_CAL_DATE_CMD,
	SENSOR_TERM_ON_CMD,
	SENSOR_TERM_OFF_CMD,
    SENSOR_CONFIG_QRY_CMD,
    NUM_SENSOR_CMDS
};

/**
 * Sensor class for the PTB210 barometer, built by Vaisala.
 * 
 * This subclass adds features to automatically set the serial port configuration 
 * to default values, or overridden values provided by the sensor XML definition as 
 * provided in the sensor, dsm or project catalogs.
 * 
 * If this class cannot communicate with the sensor, then it will modify the serial port
 * settings, such as baud rate, word length/parity/stop bits, as well as port type (RS232, 
 * RS422/485, or RS485 half duplex), until it can successfully communicate with the sensor.
 * 
 * This class also provides features to send to the sensor, configuration settings, including 
 * both serial port and science/measurement settings. 
 */
class PTB210: public nidas::core::SerialSensor
{
public:

    PTB210();

    ~PTB210();

    // override open to provide the default settings to the DSM port, and search for the correct 
    // settings, should the default settings not result in successful communication.
    void open(int flags) throw (n_u::IOException, n_u::InvalidParameterException);

protected:
    bool isDefaultConfig(const n_c::PortConfig& target);
    bool findWorkingSerialPortConfig(int flags);
    bool checkResponse();
    void sendSensorCmd(PTB_COMMANDS cmd, int arg=0, bool resetNow=false);
    bool testDefaultPortConfig();
    bool sweepParameters(bool defaultTested=false);
    void setTargetPortConfig(n_c::PortConfig& target, int baud, int dataBits, Termios::parity parity, int stopBits, 
                                                      int rts485, n_c::PORT_TYPES portType, n_c::TERM termination, 
                                                      n_c::SENSOR_POWER_STATE power);
    bool installDesiredSensorConfig();
    bool configureScienceParameters();
    size_t readResponse(void *buf, size_t len, int msecTimeout);
    void printTargetConfig(n_c::PortConfig target)
    {
        target.print();
        target.xcvrConfig.print();
        std::cout << "PortConfig " << (target.applied ? "IS " : "IS NOT " ) << "applied" << std::endl;
        std::cout << std::endl;
    }

private:
    // default serial parameters for the PB210
    static const int DEFAULT_BAUD_RATE = 9600;
    static const Termios::parity DEFAULT_PARITY = Termios::ODD;
    static const int DEFAULT_STOP_BITS = 1;
    static const int DEFAULT_DATA_BITS = 7;
    static const int DEFAULT_RTS485 = 0;
    static const n_c::PORT_TYPES DEFAULT_PORT_TYPE = n_c::RS232;
    static const n_c::SENSOR_POWER_STATE DEFAULT_SENSOR_POWER = n_c::SENSOR_POWER_ON;
    static const n_c::TERM DEFAULT_SENSOR_TERMINATION = n_c::NO_TERM;
    static const bool DEFAULT_CONFIG_APPLIED = false;

    // default message parameters for the PB210
    static const int DEFAULT_MESSAGE_LENGTH = 0;
    static const bool DEFAULT_MSG_SEP_EOM = true;
    static const char* DEFAULT_MSG_SEP_CHARS;

    // PB210 pre-packaged commands
    static const char* DEFAULT_SENSOR_INIT_CMD_STR;
    static const char* SENSOR_RESET_CMD_STR;
    static const char* SENSOR_SERIAL_BAUD_CMD_STR;
    static const char* SENSOR_SERIAL_EVENP_WORD_CMD_STR;
    static const char* SENSOR_SERIAL_ODDP_WORD_CMD_STR;
    static const char* SENSOR_SERIAL_NOP_WORD_CMD_STR;
    static const char* SENSOR_MEAS_RATE_CMD_STR;
    static const char* SENSOR_NUM_SAMP_AVG_CMD_STR;
    static const char* SENSOR_PRESS_MIN_CMD_STR;
    static const char* SENSOR_PRESS_MAX_CMD_STR;
    static const char* SENSOR_SINGLE_SAMP_CMD_STR;
    static const char* SENSOR_START_CONT_SAMP_CMD_STR;
    static const char* SENSOR_STOP_CONT_SAMP_CMD_STR;
    static const char* SENSOR_POWER_DOWN_CMD_STR;
    static const char* SENSOR_POWER_UP_CMD_STR;
    static const char* SENSOR_SAMP_UNIT_CMD_STR;
    static const char* SENSOR_EXC_UNIT_CMD_STR;
    static const char* SENSOR_INC_UNIT_CMD_STR;
    static const char* SENSOR_CORRECTION_ON_CMD_STR;
    static const char* SENSOR_CORRECTION_OFF_CMD_STR;
    // No way to set the calibration points, so no need to set the Cal date.
    // static const char* SENSOR_SET_CAL_DATE_CMD_STR;
    static const char* SENSOR_TERM_ON_CMD_STR;
    static const char* SENSOR_TERM_OFF_CMD_STR;
    static const char* SENSOR_CONFIG_QRY_CMD_STR;

    // table to hold the strings for easy lookup
    static const char* cmdTable[NUM_SENSOR_CMDS];

    // NOTE: list sensor bauds from highest to lowest as the higher  
    //       ones are the most likely
    static const int NUM_SENSOR_BAUDS = 5;
    static const int SENSOR_BAUDS[NUM_SENSOR_BAUDS];
    static const int NUM_SENSOR_WORD_SPECS = 3;
    static const WordSpec SENSOR_WORD_SPECS[NUM_SENSOR_WORD_SPECS];
    static const int NUM_PORT_TYPES = 3;
    static const n_c::PORT_TYPES SENSOR_PORT_TYPES[NUM_PORT_TYPES];

    static const n_c::PortConfig DEFAULT_PORT_CONFIG;

    static const int DEFAULT_RESET_WAIT_TIME = 3 * USECS_PER_SEC;

    n_c::PortConfig testPortConfig;
    n_c::PortConfig desiredPortConfig;
    n_c::MessageConfig defaultMessageConfig;

    // no copying
    PTB210(const PTB210& x);

    // no assignment
    PTB210& operator=(const PTB210& x);
};

}}}	// namespace nidas namespace dynld namespace isff

#endif

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

// create table indices
enum PTB_COMMANDS
{   NULL_COMMAND = -1, // don't put in table, only used for null return value
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

// PTB pressure unit enum
enum PTB_PRESSURE_UNITS {
    hPa,
    mbar,
    inHg,
    psia,
    torr,
    mmHg,
    kPa,
    Pa,
    mmH2O,
    inH2O,
    bar
};

// Valid PTB210 Command argument ranges
struct PTB210_ARG_RANGE {
    int min;
    int max;
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
 * 
 * NOTE: The default settings are taken from the entry in sensor_catalog.xml:
 *  <serialSensor ID="PTB210" class="DSMSerialSensor" init_string="\r\n.BP\r\n"
 *		  baud="9600" parity="even" databits="7" stopbits="1">
 *      <!-- 838.26\r\n -->
 *    <sample id="1" scanfFormat="%f" rate="1">
 *	    <variable name="P" units="mb"
 *		    longname="Barometric Pressure, Vaisala PTB 210"
 *		    plotrange="$P_RANGE"/>
 *    </sample>
 *    <message separator="\n" position="end" length="0"/>
 *  </serialSensor>
 * 
 * 
 */
class PTB210: public nidas::core::SerialSensor
{
public:

    PTB210();

    ~PTB210();

    // override fromDOMElement() to provide a means to intercept custom auto config instructions from the XML
    void fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException);

    // utility to turn a pressure unit string designation into a PRESS_UNIT enum
    static PTB_PRESSURE_UNITS pressUnitStr2PressUnit(const char* unitStr) {
        std::string units(unitStr);

        if (units == "hPa") {
            return hPa;
        }
        else if (units == "mBar") {
            return mbar;
        }
        else if (units == "inHg") {
            return inHg;
        }
        else if (units == "psia") {
            return psia;
        }
        else if (units == "torr") {
            return torr;
        }
        else if (units == "mmHg") {
            return mmHg;
        }
        else if (units == "kPa") {
            return kPa;
        }
        else if (units == "Pa") {
            return Pa;
        }
        else if (units == "mmH2O") {
            return mmH2O;
        }
        else if (units == "inH2O") {
            return inH2O;
        }
        else if (units == "Bar") {
            return bar;
        }
        else {
            std::stringstream errMsg;
            errMsg << "Requested units not found: " << units;
            throw n_u::InvalidParameterException("AutoConfig", "PTB210::pressUnitStr2PressUnit() ", (errMsg.str()));
        }
    }

protected:
    virtual bool supportsAutoConfig() { return true; }
    virtual bool checkResponse();
    virtual void sendSensorCmd(int cmd, n_c::SensorCmdArg arg = n_c::SensorCmdArg(0), bool resetNow=false);
    virtual bool installDesiredSensorConfig(const n_c::PortConfig& rDesiredConfig);
    void sendScienceParameters();
    bool checkScienceParameters();
    bool compareScienceParameter(PTB_COMMANDS cmd, const char* match);
    void updateDesiredScienceParameter(PTB_COMMANDS cmd, int arg=0);
    n_c::SensorCmdData getDesiredCmd(PTB_COMMANDS cmd);
    virtual nidas::core::CFG_MODE_STATUS enterConfigMode();
    virtual void exitConfigMode();
    virtual void updateMetaData();

private:
    // default serial parameters for the PB210
    static const int DEFAULT_BAUD_RATE = 9600;
    static const Termios::parity DEFAULT_PARITY = Termios::ODD;
    static const int DEFAULT_STOP_BITS = 1;
    static const int DEFAULT_DATA_BITS = 7;
    static const int DEFAULT_RTS485 = 0;
    static const n_c::PORT_TYPES DEFAULT_PORT_TYPE = n_c::RS232;
//    static const n_c::SENSOR_POWER_STATE DEFAULT_SENSOR_POWER = n_c::SENSOR_POWER_ON;
    static const n_c::TERM DEFAULT_SENSOR_TERMINATION = n_c::NO_TERM;
    static const bool DEFAULT_CONFIG_APPLIED = false;

    // default message parameters for the PB210
    static const int DEFAULT_MESSAGE_LENGTH = 0;
    static const bool DEFAULT_MSG_SEP_EOM = true;
    static const char* DEFAULT_MSG_SEP_CHARS;

    // default science parameters for the PB210
    static const PTB_COMMANDS DEFAULT_PRESSURE_UNITS_CMD = SENSOR_SAMP_UNIT_CMD;
    static const PTB_PRESSURE_UNITS DEFAULT_PRESSURE_UNITS = mbar;

    static const PTB_COMMANDS DEFAULT_OUTPUT_UNITS_CMD = SENSOR_EXC_UNIT_CMD;

    static const PTB_COMMANDS DEFAULT_SAMPLE_RATE_CMD = SENSOR_MEAS_RATE_CMD;
    static const int DEFAULT_SAMPLE_RATE = 60; // measurements per minute = 1/sec
    static const int SENSOR_MEAS_RATE_MIN = 6;
    static const int SENSOR_MEAS_RATE_MAX = 4200;

    static const PTB_COMMANDS DEFAULT_SAMPLE_AVERAGING_CMD = SENSOR_NUM_SAMP_AVG_CMD;
    static const int DEFAULT_NUM_SAMPLES_AVERAGED = 0; // no averaging performed
    static const int SENSOR_SAMPLE_AVG_MIN = 0;
    static const int SENSOR_SAMPLE_AVG_MAX = 255;

    static const PTB_COMMANDS DEFAULT_USE_CORRECTION_CMD = SENSOR_CORRECTION_ON_CMD;
    static const int NUM_DEFAULT_SCIENCE_PARAMETERS;
    static const n_c::SensorCmdData DEFAULT_SCIENCE_PARAMETERS[];

    // PB210 pre-packaged commands
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
    static const n_c::WordSpec SENSOR_WORD_SPECS[NUM_SENSOR_WORD_SPECS];
    static const int NUM_PORT_TYPES = 3;
    static const n_c::PORT_TYPES SENSOR_PORT_TYPES[NUM_PORT_TYPES];

    static const n_c::PortConfig DEFAULT_PORT_CONFIG;

    static const int SENSOR_RESET_WAIT_TIME = USECS_PER_SEC * 3;
    // static const int CHAR_WRITE_DELAY = USECS_PER_MSEC * 100; // 100mSec
    static const int CHAR_WRITE_DELAY = USECS_PER_MSEC * 110; // 110mSec

    n_c::MessageConfig defaultMessageConfig;
    n_c::SensorCmdData* desiredScienceParameters;


    // no copying
    PTB210(const PTB210& x);

    // no assignment
    PTB210& operator=(const PTB210& x);
};

}}}	// namespace nidas namespace dynld namespace isff

#endif

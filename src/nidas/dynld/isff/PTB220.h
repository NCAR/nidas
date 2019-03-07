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

#ifndef NIDAS_DYNLD_ISFF_PTB_220_H
#define NIDAS_DYNLD_ISFF_PTB_220_H

#include <nidas/core/SerialSensor.h>

namespace n_c = nidas::core;
namespace n_u = nidas::util;

namespace nidas { namespace dynld { namespace isff {

// create table indices
enum PTB220_COMMANDS
{   NULL_COMMAND = -1, // don't put in table, only used for null return value
	SENSOR_SEND_MODE_CMD,
	// Require SW4 set to WRITE ENABLE
	// SENSOR_MEAS_MODE_CMD,
	// END - Require SW4 set to WRITE ENABLE
	SENSOR_SERIAL_BAUD_CMD,
	SENSOR_SERIAL_PARITY_CMD,
	SENSOR_SERIAL_DATABITS_CMD,
	SENSOR_SERIAL_STOPBITS_CMD,
	SENSOR_ECHO_CMD,
	SENSOR_DATA_OUTPUT_FORMAT_CMD,
	SENSOR_ERROR_OUTPUT_FORMAT_CMD,
	// not implementing external display
	// SENSOR_DISPLAY_OUTPUT_FORMAT_CMD,
	// SENSOR_KEYBD_LCK_CMD,
	SENSOR_PRESS_UNIT_CMD,
	SENSOR_TEMP_UNIT_CMD,
	SENSOR_HEIGHT_CORR_CMD,
	SENSOR_AVG_TIME_CMD,
	SENSOR_OUTPUT_INTERVAL_VAL_CMD,
	SENSOR_OUTPUT_INTERVAL_UNIT_CMD,
	SENSOR_ADDR_CMD,
	// Not handling this use case
	//SENSOR_USR_DEFINED_SEND_CMD,
	SENSOR_PRESS_STABILITY_CMD,
	SENSOR_PRESS_LIMIT_ALARMS_CMD,
	// units only have one xducer
	// SENSOR_PRESS_DIFF_LIMIT_CMD,
	SENSOR_RUN_CMD,
	SENSOR_STOP_CMD,
	SENSOR_STOP_SEND_CMD,
	SENSOR_POLL_SEND_CMD,
	SENSOR_VERIF_CMD,
	SENSOR_SELF_DIAG_CMD,
	SENSOR_PRESS_TRACK_CMD,
	SENSOR_PRESS_LIMIT_LIST_CMD,
    SENSOR_RESET_CMD,
	SENSOR_POLL_OPEN_CMD,
	SENSOR_POLL_CLOSE_CMD,
	SENSOR_CORR_STATUS_CMD,
	// Require SW4 set to WRITE ENABLE
	// SENSOR_LINEAR_CORR_CMD,
	// SENSOR_MULTPT_CORR_CMD,
	// SENSOR_SET_LINEAR_CORR_CMD,
	// SENSOR_SET_MULTPT_CORR_CMD,
	// SENSOR_SET_CAL_DATE_CMD,
	// END - Require SW4 set to WRITE ENABLE
	SENSOR_CONFIG_QRY_CMD,
	SENSOR_SW_VER_CMD,
	SENSOR_SER_NUM_CMD,
	SENSOR_ERR_LIST_CMD,
	SENSOR_TEST_CMD,
	SENSOR_XDUCER_COEFF_LIST_CMD,
	// Requires SW3 set
	// SENSOR_PULSE_MODE_TEST_CMD,
    NUM_SENSOR_CMDS
};

// PTB220 Send command args
enum PTB220_SEND_ARGS {
	STOP,
	RUN,
	SEND,
	POLL
};

enum PTB220_MEAS_MODE_ARGS { NORMAL, FAST };

enum PTB220_SERIAL_BAUD_ARGS {
	P300,
	P600,
	P1200,
	P2400,
	P4800,
	P9600
};

enum PTB220_SERIAL_PARITY_ARGS { PE='E', PO='O', PN='N' };

enum PTB220_SERIAL_DATA_BITS_ARGS { D7='7', D8='8' };

enum PTB220_SERIAL_STOP_BITS_ARGS { S1='1', S2='2' };

// PTB pressure unit enum
enum PTB220_PRESS_TEMP_UNITS {
    hPa,
    kPa,
    Pa,
    mbar,
    inHg,
    mmHg,
    torr,
    mmH2O,
    psia,
    C,
	F
};
std::string pressTempUnitsArgsToStr(PTB220_PRESS_TEMP_UNITS ptUnits)
{
	std::string retval("");
	switch (ptUnits) {
		case hPa:
			retval.append("hPa");
			break;
		case kPa:
			retval.append("kPa");
			break;
		case Pa:
			retval.append("Pa");
			break;
		case mbar:
			retval.append("mbar");
			break;
		case inHg:
			retval.append("inHg");
			break;
		case torr:
			retval.append("torr");
			break;
		case mmH2O:
			retval.append("mmH2O");
			break;
		case psia:
			retval.append("psia");
			break;
		case C:
			retval.append("C");
			break;
		case F:
			retval.append("F");
			break;
		default:
			{
		        std::stringstream errMsg;
		        errMsg << "Requested press/temp units not found: " << ptUnits;
		        throw n_u::InvalidParameterException("AutoConfig", "PTB220::pressTempUnitsArgsToStr() ", (errMsg.str()));
		    }
			break;
	}

	return retval;
}
// utility to turn a pressure unit string designation into a PRESS_UNIT enum
PTB220_PRESS_TEMP_UNITS strToPressTempUnits(std::string ptStr)
{
	PTB220_PRESS_TEMP_UNITS retval = mbar;
	if (ptStr.find("hPa") || ptStr.find("HPA")) {
		retval = hPa;
	}
	else if (ptStr.find("kPa") || ptStr.find("KPA")) {
		retval = kPa;
	}
	else if (ptStr.find("Pa") || ptStr.find("PA")) {
		retval = kPa;
	}
	else if (ptStr.find("Pa") || ptStr.find("PA")) {
		retval = kPa;
	}
	else if (ptStr.find("mbar") || ptStr.find("MBAR")) {
		retval = mbar;
	}
	else if (ptStr.find("mbar") || ptStr.find("MBAR")) {
		retval = mbar;
	}
	else if (ptStr.find("inHg") || ptStr.find("INHG")) {
		retval = inHg;
	}
	else if (ptStr.find("torr") || ptStr.find("TORR")) {
		retval = torr;
	}
	else if (ptStr.find("mmH2O") || ptStr.find("MMH2O")) {
		retval = mmH2O;
	}
	else if (ptStr.find("psia") || ptStr.find("PSIA")) {
		retval = psia;
	}
	else if (ptStr.find("c") || ptStr.find("C")) {
		retval = C;
	}
	else if (ptStr.find("f") || ptStr.find("F")) {
		retval = F;
	}
    else {
        std::stringstream errMsg;
        errMsg << "Requested press/temp units not found: " << ptStr;
        throw n_u::InvalidParameterException("AutoConfig", "PTB220::strToPressTempUnits() ", (errMsg.str()));
    }

	return retval;
}

PTB220_PRESS_TEMP_UNITS pressUnitStr2PressUnit(const char* unitStr)
{
    return strToPressTempUnits(std::string(unitStr));
}

enum PTB220_INTERVAL_UNITS_ARGS { s, min, hr };
std::string intervalUnitsToStr(PTB220_INTERVAL_UNITS_ARGS intervalUnits)
{
	std::string retval("");
	switch (intervalUnits) {
		case s:
			retval.append("s");
			break;

		case min:
			retval.append("min");
			break;

		case hr:
			retval.append("hr");
			break;
		default:
			{
		        std::stringstream errMsg;
		        errMsg << "Requested interval units not found: " << intervalUnits;
		        throw n_u::InvalidParameterException("AutoConfig", "PTB220::intervalUnitsToStr() ", (errMsg.str()));
		    }
			break;
	}
	return retval;
}

PTB220_INTERVAL_UNITS_ARGS strToIntervalUnits(std::string intervalStr)
{
	PTB220_INTERVAL_UNITS_ARGS retval = s;
	if (intervalStr == "s") {
		retval = s;
	}
	else if (intervalStr == "min") {
		retval = min;
	}
	else if (intervalStr == "hr") {
		retval = hr;
	}
    else {
        std::stringstream errMsg;
        errMsg << "Requested units not found: " << intervalStr;
        throw n_u::InvalidParameterException("AutoConfig", "PTB220::strToIntervalUnits() ", (errMsg.str()));
    }

	return retval;
}

PTB220_INTERVAL_UNITS_ARGS strToIntervalUnits(const char* intervalStr)
{
	return strToIntervalUnits(std::string(intervalStr));
}

/**
 * Sensor class for the PTB220 barometer, built by Vaisala.
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
 *  <serialSensor ID="PTB220" class="DSMSerialSensor"
 *      baud="9600" parity="none" databits="8" stopbits="1">
 *      <!-- |B7  832.44  22.7\r\n| -->
 *      <sample id="1" scanfFormat="%*c%*d%f%f" rate="1">
 *          <variable name="P" units="mb" longname="Barometric Pressure, Vaisala PTB" plotrange="$P_RANGE"/>
 *          <variable name="Tbaro" units="degC" longname="Vaisala PTB internal temperature" plotrange="$T_RANGE"/>
 *      </sample>
 *      <message separator="\n" position="end" length="0"/>
 *  </serialSensor>
 * 
 * 
 */
class PTB220: public nidas::core::SerialSensor
{
public:

    PTB220();

    ~PTB220();

    // override fromDOMElement() to provide a means to intercept custom auto config instructions from the XML
    void fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException);

protected:
    void sendSensorCmd(int cmd, n_c::SensorCmdArg arg=n_c::SensorCmdArg(0), bool resetNow=false);
    bool compareScienceParameter(int cmd, const char* match);
//    size_t readResponse(void *buf, size_t len, int msecTimeout);
    void printTargetConfig(n_c::PortConfig target)
    {
        target.print();
        target.xcvrConfig.print();
        std::cout << "PortConfig " << (target.applied ? "IS " : "IS NOT " ) << "applied" << std::endl;
        std::cout << std::endl;
    }
    void updateDesiredScienceParameter(int cmd, n_c::SensorCmdArg arg=n_c::SensorCmdArg(0));
    n_c::SensorCmdData getDesiredCmd(int cmd);

    virtual bool supportsAutoConfig() { return true; }
    nidas::core::CFG_MODE_STATUS enterConfigMode();
    void exitConfigMode();
    virtual bool checkResponse();
    virtual bool installDesiredSensorConfig(const n_c::PortConfig& rDesiredPortConfig);
    virtual void sendScienceParameters();
    virtual bool checkScienceParameters();

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
    static const PTB220_COMMANDS DEFAULT_SENSOR_SEND_MODE_CMD = SENSOR_SEND_MODE_CMD;
    static const char* DEFAULT_SENSOR_SEND_MODE;

    static const PTB220_COMMANDS DEFAULT_PRESS_UNITS_CMD = SENSOR_PRESS_UNIT_CMD;
    static const PTB220_PRESS_TEMP_UNITS DEFAULT_PRESS_UNITS = mbar;

    static const PTB220_COMMANDS DEFAULT_TEMP_UNITS_CMD = SENSOR_TEMP_UNIT_CMD;
    static const PTB220_PRESS_TEMP_UNITS DEFAULT_TEMP_UNITS = C;

    static const PTB220_COMMANDS DEFAULT_OUTPUT_RATE_CMD = SENSOR_OUTPUT_INTERVAL_VAL_CMD;
    static const int SENSOR_OUTPUT_RATE_MIN = 0;
    static const int SENSOR_OUTPUT_RATE_MAX = 255;
    static const int DEFAULT_OUTPUT_RATE = 1; // 1/sec

    static const PTB220_COMMANDS DEFAULT_OUTPUT_RATE_UNITS_CMD = SENSOR_OUTPUT_INTERVAL_UNIT_CMD;
    static const char* DEFAULT_OUTPUT_RATE_UNIT;

    static const PTB220_COMMANDS DEFAULT_SAMPLE_AVERAGING_CMD = SENSOR_AVG_TIME_CMD;
    static const int SENSOR_AVG_TIME_MIN = 1;
    static const int SENSOR_AVG_TIME_MAX = 600;
    static const int DEFAULT_AVG_TIME = SENSOR_AVG_TIME_MIN;

    static const PTB220_COMMANDS DEFAULT_ADDR_CMD = SENSOR_ADDR_CMD;
    static const int SENSOR_ADDR_MIN = 0;
    static const int SENSOR_ADDR_MAX = 99;
    static const int DEFAULT_SENSOR_ADDR = 0;

    static const PTB220_COMMANDS DEFAULT_OUTPUT_FORMAT_CMD = SENSOR_DATA_OUTPUT_FORMAT_CMD;
    static const char* DEFAULT_SENSOR_OUTPUT_FORMAT;

    static const PTB220_COMMANDS DEFAULT_HEIGHT_CORR_CMD = SENSOR_HEIGHT_CORR_CMD;
    static const int SENSOR_HEIGHT_CORR_MIN = -40;
    static const int SENSOR_HEIGHT_CORR_MAX = 40;
    static const int DEFAULT_HEIGHT_CORR = 0;

    static const int NUM_DEFAULT_SCIENCE_PARAMETERS;
    static const n_c::SensorCmdData DEFAULT_SCIENCE_PARAMETERS[];

    // PB210 pre-packaged commands
    static const char* SENSOR_SEND_MODE_CMD_STR;
    // Requires SW4 set to write enable
	// static const char* SENSOR_MEAS_MODE_CMD_STR;
	// static const char* SENSOR_LINEAR_CORR_CMD_STR;
	// static const char* SENSOR_MULTPT_CORR_CMD_STR;
	// static const char* SENSOR_SET_LINEAR_CORR_CMD_STR;
	// static const char* SENSOR_SET_MULTPT_CORR_CMD_STR;
	// static const char* SENSOR_SET_CAL_DATE_CMD_STR;
	static const char* SENSOR_SERIAL_BAUD_CMD_STR;
	static const char* SENSOR_SERIAL_PARITY_CMD_STR;
	static const char* SENSOR_SERIAL_DATABITS_CMD_STR;
	static const char* SENSOR_SERIAL_STOPBITS_CMD_STR;
	static const char* SENSOR_ECHO_CMD_STR;
	static const char* SENSOR_DATA_OUTPUT_FORMAT_CMD_STR;
	static const char* SENSOR_ERROR_OUTPUT_FORMAT_CMD_STR;
	// Not managing external display
	// static const char* SENSOR_DISPLAY_OUTPUT_FORMAT_CMD_STR;
	// static const char* SENSOR_KEYBD_LCK_CMD_STR;
	static const char* SENSOR_PRESS_UNIT_CMD_STR;
	static const char* SENSOR_TEMP_UNIT_CMD_STR;
	static const char* SENSOR_HEIGHT_CORR_CMD_STR;
	static const char* SENSOR_AVG_TIME_CMD_STR;
	static const char* SENSOR_OUTPUT_INTERVAL_VAL_CMD_STR;
	static const char* SENSOR_OUTPUT_INTERVAL_UNIT_CMD_STR;
	static const char* SENSOR_ADDR_CMD_STR;
	// Not handling this use case
	// static const char* SENSOR_USR_DEFINED_SEND_CMD_STR;
	static const char* SENSOR_PRESS_STABILITY_CMD_STR;
	static const char* SENSOR_PRESS_LIMIT_ALARMS_CMD_STR;
	// Our units only have one xducer
	// static const char* SENSOR_PRESS_DIFF_LIMIT_CMD_STR;
	static const char* SENSOR_RUN_CMD_STR;
	static const char* SENSOR_STOP_CMD_STR;
	static const char* SENSOR_STOP_SEND_CMD_STR;
	static const char* SENSOR_POLL_SEND_CMD_STR;
	static const char* SENSOR_VERIF_CMD_STR;
	static const char* SENSOR_SELF_DIAG_CMD_STR;
	static const char* SENSOR_PRESS_TRACK_CMD_STR;
	static const char* SENSOR_PRESS_LIMIT_LIST_CMD_STR;
    static const char* SENSOR_RESET_CMD_STR;
	static const char* SENSOR_POLL_OPEN_CMD_STR;
	static const char* SENSOR_POLL_CLOSE_CMD_STR;
	static const char* SENSOR_CORR_STATUS_CMD_STR;
	static const char* SENSOR_CONFIG_QRY_CMD_STR;
	static const char* SENSOR_SW_VER_CMD_STR;
	static const char* SENSOR_SER_NUM_CMD_STR;
	static const char* SENSOR_ERR_LIST_CMD_STR;
	static const char* SENSOR_TEST_CMD_STR;
	static const char* SENSOR_XDUCER_COEFF_LIST_CMD_STR;
	static const char* SENSOR_PULSE_MODE_TEST_CMD_STR;

	// table to hold the strings for easy lookup
	static const char* cmdTable[NUM_SENSOR_CMDS];

	// NOTE: list sensor bauds from highest to lowest as the higher
    //       ones are the most likely
    static const int NUM_SENSOR_BAUDS = 5;
    static const int SENSOR_BAUDS[NUM_SENSOR_BAUDS];
    static const int NUM_SENSOR_WORD_SPECS = 9;
    static const n_c::WordSpec SENSOR_WORD_SPECS[NUM_SENSOR_WORD_SPECS];
    static const int NUM_PORT_TYPES = 3;
    static const n_c::PORT_TYPES SENSOR_PORT_TYPES[NUM_PORT_TYPES];

    static const n_c::PortConfig DEFAULT_PORT_CONFIG;

    static const int SENSOR_RESET_WAIT_TIME = USECS_PER_SEC * 3;
    // static const int CHAR_WRITE_DELAY = USECS_PER_MSEC * 100; // 100mSec
    static const int CHAR_WRITE_DELAY = USECS_PER_MSEC * 110; // 110mSec

    n_c::PortConfig testPortConfig;
    n_c::PortConfig desiredPortConfig;
    n_c::MessageConfig defaultMessageConfig;

    n_c::SensorCmdData* desiredScienceParameters;

    std::string sensorSWVersion;
    std::string sensorSerialNumber;

    // no copying
    PTB220(const PTB220& x);

    // no assignment
    PTB220& operator=(const PTB220& x);
};

}}}	// namespace nidas namespace dynld namespace isff

#endif

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

#ifndef NIDAS_DYNLD_AUTOCONFIG_TEMPLATE_H
#define NIDAS_DYNLD_AUTOCONFIG_TEMPLATE_H

#include <nidas/core/SerialSensor.h>

namespace n_c = nidas::core;
namespace n_u = nidas::util;

namespace nidas { namespace dynld { // Add additional namespaces here, such as: namespace isff {

/*
 * OPTIONAL
 *
 * A listing of the various sensor commands. Need some way to keep track of and easily access sensor commands.
 * Name according to actual sensor.
 */
// create table indices
enum AUTOCONFIG_SENSOR_COMMANDS
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

/*
 * OPTIONAL
 *
 * Various enums to keep track of valid sensor attribute values. Name appropriately.
 */
// AutoConfig sensor attrib value enum
enum AUTOCONFIG_SENSOR_ATTRIB_VALUES {
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


/**
 * NOTE: Never add this pattern class to any SConscript for NIDAS!!!
 *
 * Sensor class for the AutoConfig capable sensor, built by Some Vendor.
 * 
 * This subclass adds features to aid in the automation of setting the sensor serial port
 * configuration to default values, or overriding values which are provided by the
 * sensor XML definition as provided in the sensor, dsm or project catalogs.
 *
 * It also provides features to aid in the automation of setting the sensor science
 * parameter configuration to default values, or overriding values which are provided by
 * the sensor XML definition as provided in the sensor, dsm or project catalogs.
 *
 * Theory of Operation
 *
 * Sensor AutoConfig is driven by the base class, SerialSensor, its open() and fromDOMElement()
 * methods, and from the various base class, and subclass overrides of fromDOMElement(). The
 * purpose of this class is to provide the data and tools for SerialSensor to perform the
 * AutoConfig functionality/feature. Which features are required are spelled out below in
 * the class definition.
 *
 * The NIDAS dynld feature scans the DSM xml config file and sensor catalog file for sensor
 * specifications, and instantiates the sensor classes, such as this one. The instantiation of
 * the sensor class results default parameters for serial communication to be provided to the
 * base class and science parameters to be provided to the subclass. It also results in calling
 * fromDOMElement() for the sensor class and all base classes which may modify the default
 * communication and science configurations.
 *
 * When the SerialSensor::open() method is invoked, it first checks to see if it has been
 * subclassed to provide AutoConfig functionality. If so, then it starts by setting the
 * serial port configuration to the default values and invokes the subclass method: checkResponse().
 *
 * If checkResponse() returns true, then it assumes that the serial port config is already
 * modified to by the XML, and then SerialSensor::open() continues the process set and check
 * the science parameters.
 * 
 * If checkResponse() returns false, then the open() method will modify the serial port
 * settings, such as baud rate, word length/parity/stop bits, as well as port type (RS232, 
 * RS422/485, or RS485 half duplex), until checkResponse() returns true.
 * 
 * At that point, SerialSensor::open() attempts to set the sensor serial configuration
 * to the desired configuration, and changes its own configuration to match. It again checks
 * the response. If successful, then SerialSensor::open() continues the process by setting and
 * checking the science parameters.
 * .
 * If all has gone well, SerialSensor::open() continues and sampling commences. If not, then
 * the port is closed, and it will be re-opened periodically in an attempt to communicate.
 * 
 * NOTE: The default settings are taken from the entry in sensor_catalog.xml:
 *
 * !!!Replace the following lines w/the sensor catalog entry for the AutoConfig sensor !!!
 *
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
class AutoConfigTemplate: public nidas::core::SerialSensor
{
public:

    AutoConfigTemplate();

    ~AutoConfigTemplate();

    /*
     * REQUIRED, must implement
     *
     * Override fromDOMElement() to provide a means to intercept custom auto config instructions from the
     * sensor-specific XML specification.
     * Use only for sensor-specific science parameters. Ignore all other XML tags/attributes.
     */
    virtual void fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException);

    // Add necessary utilities to manipulate the various argument types for the sensor attributes
    // utility to turn a pressure unit string designation into a PRESS_UNIT enum
    static AUTOCONFIG_SENSOR_ATTRIB_VALUES sensorUnitStr2SensorUnit(const char* unitStr) {
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
            throw n_u::InvalidParameterException("AutoConfig", "AutoConfigTemplate::pressUnitStr2PressUnit() ", (errMsg.str()));
        }
    }

protected:
    /*
     * REQUIRED, must implement
     *
     * Tells SerialSensor that its subclass is capable of participating in AutoConfig activities
     */
    virtual bool supportsAutoConfig() { return true; }

    /*
     * REQUIRED, must implement
     *
     * Tells SerialSensor whether the response from the sensor is understood as coming from the target sensor type
     */
    virtual bool checkResponse();

    /*
     * REQUIRED
     *
     * Used by SerialSensor to tell the subclass to install the serial port configuration which
     * was specified by the XML config file.
     */
    virtual bool installDesiredSensorConfig(const n_c::PortConfig& rDesiredConfig);

    /*
     * REQUIRED
     *
     * Used by Serial Sensor to tell the subclass to change the sensor science parameters to
     * match those in the science parameter object.
     */
    virtual void sendScienceParameters();

    /*
     * REQUIRED
     *
     * Used by Serial Sensor to tell the subclass to verify the sensor
     * science parameters after installing them, and return that status.
     */
    virtual bool checkScienceParameters();

    /*
     *  Required/Optional
     *
     *  Functionality is required, actual method signature is optional
     *
     *  Performs the task of communicating with the sensor to get status, update configuration, etc
     *  May be replaced with any other communication method/means.
     */
    virtual void sendSensorCmd(int cmd, n_c::SensorCmdArg arg = n_c::SensorCmdArg(0), bool resetNow=false);

    /*
     *  Required/Optional
     *
     *  Functionality is required, actual method signature is optional
     *
     *  Performs the task of comparing the science parameter attributes reported with those
     *  in the desired science parameters object.
     */
    bool compareScienceParameter(AUTOCONFIG_SENSOR_COMMANDS cmd, const char* match);

    /*
     *  Required/Optional
     *
     *  Functionality is required, actual method signature is optional
     *
     *  Performs the task of reading the open port to get the entire response to a command.
     */
    std::size_t readResponse(void *buf, std::size_t len, int msecTimeout);

    /*
     *  Required/Optional
     *
     *  Functionality is required, actual method signature is optional
     *
     *  Performs the task of taking a sensor science parameter reported by fromDOMElement() and updating
     *  the appropriate desired science parameter.
     */
    void updateDesiredScienceParameter(AUTOCONFIG_SENSOR_COMMANDS cmd, int arg=0);
    n_c::SensorCmdData getDesiredCmd(AUTOCONFIG_SENSOR_COMMANDS cmd);

private:
    /*
     * REQUIRED: Must supply some form of default serial port config
     * OPTIONAL: Form of the default definition is optional, but it must be passed
     *           to the base class constructor in a PortConfig object.
     */
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

    /*
     * REQUIRED: Needed for compatibility with dsm_auto_config utility.
     */
    // default message parameters for the PB210
    static const int DEFAULT_MESSAGE_LENGTH = 0;
    static const bool DEFAULT_MSG_SEP_EOM = true;
    static const char* DEFAULT_MSG_SEP_CHARS;

    /*
     * REQUIRED: Must supply a default science parameters configuration
     * OPTIONAL: Form of science parameter defaults is optional to meet the requirements of the
     *           particular sensor.
     *
     * NOTE: There is a helper class called nidas::core::SensorCmdData which can be used to
     *       assemble the default/desired science parameter commands and the associated payload.
     */
    // default science parameters for the PB210
    static const AUTOCONFIG_SENSOR_COMMANDS DEFAULT_PRESSURE_UNITS_CMD = SENSOR_SAMP_UNIT_CMD;
    static const AUTOCONFIG_SENSOR_ATTRIB_VALUES DEFAULT_PRESSURE_UNITS = mbar;

    static const AUTOCONFIG_SENSOR_COMMANDS DEFAULT_OUTPUT_UNITS_CMD = SENSOR_EXC_UNIT_CMD;

    static const AUTOCONFIG_SENSOR_COMMANDS DEFAULT_SAMPLE_RATE_CMD = SENSOR_MEAS_RATE_CMD;
    static const int DEFAULT_SAMPLE_RATE = 60; // measurements per minute = 1/sec
    static const int SENSOR_MEAS_RATE_MIN = 6;
    static const int SENSOR_MEAS_RATE_MAX = 4200;

    static const AUTOCONFIG_SENSOR_COMMANDS DEFAULT_SAMPLE_AVERAGING_CMD = SENSOR_NUM_SAMP_AVG_CMD;
    static const int DEFAULT_NUM_SAMPLES_AVERAGED = 0; // no averaging performed
    static const int SENSOR_SAMPLE_AVG_MIN = 0;
    static const int SENSOR_SAMPLE_AVG_MAX = 255;

    static const AUTOCONFIG_SENSOR_COMMANDS DEFAULT_USE_CORRECTION_CMD = SENSOR_CORRECTION_ON_CMD;
    static const int NUM_DEFAULT_SCIENCE_PARAMETERS;
    static const n_c::SensorCmdData DEFAULT_SCIENCE_PARAMETERS[];

    /*
     * REQUIRED - Some means of keeping track of the sensor commands
     * OPTIONAL - Need not be a part of the class, could be static to the file
     */
    // AutoConfig Sensor commands string templates
    // Some may contain the argument as well
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

    /*
     * REQUIRED - Need some way to inform the SerialSensor base class what serial
     *            port configurations are supported by the sensor so that it
     *            doesn't waste time attempting configurations which will not work.
     * OPTIONAL - exact form may be different (map, list, etc)
     */
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
    AutoConfigTemplate(const AutoConfigTemplate& x);

    // no assignment
    AutoConfigTemplate& operator=(const AutoConfigTemplate& x);
};

}}	// namespace nidas namespace dynld namespace isff

#endif // NIDAS_DYNLD_AUTOCONFIG_TEMPLATE_H

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

#ifndef NIDAS_DYNLD_ISFF_GILL2D_H
#define NIDAS_DYNLD_ISFF_GILL2D_H

#include <nidas/dynld/isff/Wind2D.h>

namespace n_c = nidas::core;
namespace n_u = nidas::util;

namespace nidas { namespace dynld { namespace isff {

enum GILL2D_CFG_MODE_STATUS {
	NOT_ENTERED,
	ENTERED_RESP_CHECKED,
	ENTERED
};

enum GILL2D_OUTPUT_MODE
{
	CONTINUOUS = true,
	POLLED = false
};

// create table indices
enum GILL2D_COMMANDS
{   NULL_COMMAND = -1, // don't put in table, only used for null return value
    SENSOR_CONFIG_MODE_CMD,
    SENSOR_ENABLE_POLLED_MODE_CMD,
    SENSOR_POLL_MEAS_CMD,
    SENSOR_QRY_ID_CMD,
    SENSOR_DISABLE_POLLED_MODE_CMD,
	//TODO - Add A, C, T, etc commands so that this becomes more generic.
	SENSOR_SERIAL_BAUD_CMD,
    SENSOR_DIAG_QRY_CMD,
    SENSOR_DUPLEX_COMM_CMD,
	SENSOR_SERIAL_DATA_WORD_CMD,
	SENSOR_AVG_PERIOD_CMD,
	SENSOR_HEATING_CMD,
	SENSOR_NMEA_ID_STR_CMD,
	SENSOR_MSG_TERM_CMD,
	SENSOR_MSG_STREAM_CMD,
	SENSOR_NODE_ADDR_CMD,
	SENSOR_OUTPUT_FIELD_FMT_CMD,
	SENSOR_OUTPUT_RATE_CMD,
	SENSOR_START_MEAS_CMD,
	SENSOR_MEAS_UNITS_CMD,
	SENSOR_VERT_MEAS_PADDING_CMD,
	SENSOR_ALIGNMENT_CMD,
    NUM_SENSOR_CMDS
};

enum GILL2D_BAUD_ARGS 
{
    G2400 = 1,
    G4800,
    G9600,
    G19200,
    G38400,
    G1200,
    G300,
    NUM_BAUD_ARGS = G300
};

enum GILL2D_DIAG_ARGS
{
    TYPE_SER_NO = 1,
    SW_VER,
    OPER_CONFIG,
    ANALOG_ID,
    PWR_SPLY_VLTS,
    SELF_TEST,
    NUM_DIAG_ARGS = SELF_TEST
};

enum GILL2D_DUPLEX_ARGS
{
    FULL = 1,
    HALF,
    NUM_DUPLEX_ARGS = HALF
};

enum GILL2D_DATA_WORD_ARGS
{
    N81 = 1,
    E81,
    O81,
    NUM_DATA_WORD_ARGS = O81
};

enum GILD2D_HEATING_ARGS
{
    DISABLED = 1,
    ACTIVE_H2,
    ACTIVE_H3,
    NUM_HEATING_ARGS = ACTIVE_H3
};

enum GILL2D_NMEA_STR_ARGS
{
    IIMWV = 1,
    WIMWV,
    NUM_NMEA_STR_ARGS = WIMWV
};

enum GILL2D_MSG_TERM_ARGS
{
    CRLF = 1,
    LF,
    NUM_MSG_TERM_ARGS = LF
};

enum GILL2D_MSG_STREAM_ARGS
{
    ASC_UV_CONT = 1,
    ASC_PLR_CONT,
    ASC_UV_POLLED,
    ASC_PLR_POLLED,
    NMEA_CONT,
    NUM_MSG_STREAM_ARGS = NMEA_CONT
};

enum GILL2D_OUTPUT_FMT_ARGS
{
    CSV = 1,
    FIXED_FIELD,
    NUM_OUTPUT_FMT_ARGS = FIXED_FIELD
};

enum GILL2D_OUTPUT_RATE_ARGS
{
    ONE_PER_SEC = 1,
    TWO_PER_SEC,
    FOUR_PER_SEC,
    NUM_OUTPUT_RATE_ARGS = FOUR_PER_SEC
};

enum GILL2D_UNITS_ARGS
{
    MPS = 1,
    KNOTS,
    MPH,
    KPH,
    FPM,
    NUM_UNITS_ARGS = FPM
};

enum GILL2D_VERT_OUTPUT_PAD_ARGS
{
    DISABLE_VERT_PAD = 1,
    ENABLE_VERT_PAD,
    NUM_VERT_OUTPUT_PAD_ARGS = ENABLE_VERT_PAD
};

enum GILL2D_ALIGNMENT_ARGS
{
    U_EQ_NS = 1,
    U_PLUS_45,
    UPSIDE_DOWN_REV_PLR,
    UPSIDE_DOWN_PLUS_45,
    NUM_ALIGNMENT_ARGS = UPSIDE_DOWN_PLUS_45
};

// Valid GILL2D Command argument ranges
struct GILL2D_ARG_RANGE {
    int min;
    int max;
};

struct GILL2D_CMD_ARG {
    GILL2D_COMMANDS cmd;
    int arg;
};

/**
 * Sensor class for the GIL 2D sonic anemometers.
 * 
 * This subclass adds features to automatically set the serial port configuration 
 * to default values, or overridden values provided by the sensor XML definition as 
 * provided in the sensor, dsm or project catalogs.
 * 
 * If this class cannot communicate with the sensor, then it will modify the serial port
 * settings, such as baud rate, word length/parity/stop bits, as well as port type (RS232, 
 * RS422/485, or RS485 half duplex), until it can successfully communicate with the sensor.
 * 
 * This class also provides features to send to the sensor, the desired configuration settings, including 
 * both serial port and science/measurement settings. 
 * 
 * NOTE: The default settings are taken from the entry in sensor_catalog.xml:
 *  <serialSensor class="isff.PropVane" ID="GILL_WO"
 *      baud="9600" parity="none" databits="8" stopbits="1">
 *      <!-- enabled Tc output, turned off heater:
 *           *  (enter config mode)
 *           A2 cr
 *           H1 cr
 *           Q cr
 *           -->
 *      <!-- \0x02A,157,000.08,M,+019.83,00,\0x031F\r\n -->
 *      <sample id="1" scanfFormat="%*2c,%f,%f,M,%f,%f">
 *          <variable name="Dir" units="deg" longname="Gill WindObserver, wind direction" plotrange="$DIR_RANGE">
 *              <linear>
 *                  <calfile path="$ISFS/projects/$PROJECT/ISFS/cal_files/$CAL_DIR/${SITE}:$ISFS/projects/$PROJECT/ISFS/cal_files/$CAL_DIR"
 *                      file="dir_gillwo.dat"/>
 *              </linear>
 *          </variable>
 *          <variable name="Spd" units="m/s" longname="Gill WindObserver, wind speed" plotrange="$SPD_RANGE"/>
 *          <variable name="Tc" units="degC" longname="Gill WindObserver, sonic temperature" plotrange="$T_RANGE"/>
 *          <variable name="Status" units="" longname="Gill WindObserver, status" plotrange="0 60"/>
 *          <variable name="U" units="m/s" longname="Gill WindObserver, wind U component" plotrange="$UV_RANGE"/>
 *          <variable name="V" units="m/s" longname="Gill WindObserver, wind V component" plotrange="$UV_RANGE"/>
 *      </sample>
 *      <message separator="\n" position="end" length="0"/>
 *  </serialSensor>


 */
class GILL2D: public Wind2D
{
public:
    GILL2D();
    ~GILL2D();

    // override fromDOMElement() to provide a means to intercept custom auto config instructions from the XML
    void fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException);

    // override open() to provide the default settings to the DSM port, and search for the correct 
    // settings, should the default settings not result in successful communication.
    void open(int flags) throw (n_u::IOException, n_u::InvalidParameterException);

protected:
    bool isDefaultConfig(const n_c::PortConfig& target);
    bool findWorkingSerialPortConfig();
    bool doubleCheckResponse();
    bool checkResponse();
    void sendSensorCmd(GILL2D_COMMANDS cmd, int arg=0);
    bool testDefaultPortConfig();
    bool sweepParameters(bool defaultTested=false);
    void setTargetPortConfig(n_c::PortConfig& target, int baud, int dataBits, Termios::parity parity, int stopBits, 
                                                      int rts485, n_c::PORT_TYPES portType, n_c::TERM termination, 
                                                      n_c::SENSOR_POWER_STATE power);
    bool installDesiredSensorConfig();
	void mergeDesiredWithWorkingConfig(n_c::PortConfig& rDesired, const n_c::PortConfig& rWorking);
    bool configureScienceParameters();
    void sendScienceParameters();
    bool checkScienceParameters();
    bool compareScienceParameter(GILL2D_COMMANDS cmd, const char* match);
    size_t readEntireResponse(void *buf, size_t len, int msecTimeout);
    size_t readResponse(void *buf, size_t len, int msecTimeout);
    void printTargetConfig(n_c::PortConfig target)
    {
        target.print();
        target.xcvrConfig.print();
        std::cout << "PortConfig " << (target.applied ? "IS " : "IS NOT " ) << "applied" << std::endl;
        std::cout << std::endl;
    }
    void updateDesiredScienceParameter(GILL2D_COMMANDS cmd, int arg=0);
    GILL2D_CMD_ARG getDesiredCmd(GILL2D_COMMANDS cmd);
    bool checkConfigMode(bool continuous = CONTINUOUS);
    GILL2D_CFG_MODE_STATUS enterConfigMode();
    bool isConfigCmd(GILL2D_COMMANDS cmd)
    {
    	return (cmd != SENSOR_QRY_ID_CMD && cmd != SENSOR_CONFIG_MODE_CMD
    			&& cmd != SENSOR_ENABLE_POLLED_MODE_CMD && cmd != SENSOR_POLL_MEAS_CMD
				&& cmd != SENSOR_DISABLE_POLLED_MODE_CMD);
    }
    bool confirmGillSerialPortChange(GILL2D_COMMANDS cmd, int arg);

private:
    // default serial parameters for the GIL 2D Wind Observer
    static const int DEFAULT_BAUD_RATE = 9600;
    static const Termios::parity DEFAULT_PARITY = Termios::NONE;
    static const int DEFAULT_STOP_BITS = 1;
    static const int DEFAULT_DATA_BITS = 8;
    static const int DEFAULT_RTS485 = -1;
    static const n_c::PORT_TYPES DEFAULT_PORT_TYPE = n_c::RS422;
    static const n_c::SENSOR_POWER_STATE DEFAULT_SENSOR_POWER = n_c::SENSOR_POWER_ON;
    static const n_c::TERM DEFAULT_SENSOR_TERMINATION = n_c::NO_TERM;
    static const bool DEFAULT_CONFIG_APPLIED = false;

    // default message parameters for the PB210
    static const int DEFAULT_MESSAGE_LENGTH = 0;
    static const bool DEFAULT_MSG_SEP_EOM = true;
    static const char* DEFAULT_MSG_SEP_CHAR;

    static const int MIN_AVERAGING_TIME = 0;
    static const int MAX_AVERAGING_TIME = 3600;

    static const int NUM_DEFAULT_SCIENCE_PARAMETERS;
    static const GILL2D_CMD_ARG DEFAULT_SCIENCE_PARAMETERS[];

    // GILL2D pre-packaged commands
    static const char* SENSOR_CONFIG_MODE_CMD_STR;
    static const char* SENSOR_ENABLE_POLLED_MODE_CMD_STR;
    static const char* SENSOR_POLL_MEAS_CMD_STR;
    static const char* SENSOR_QRY_ID_CMD_STR;
    static const char* SENSOR_DISABLE_POLLED_MODE_CMD_STR;
	static const char* SENSOR_SERIAL_BAUD_CMD_STR;
    static const char* SENSOR_DIAG_QRY_CMD_STR;
    static const char* SENSOR_DUPLEX_COMM_CMD_STR;
	static const char* SENSOR_SERIAL_DATA_WORD_CMD_STR;
	static const char* SENSOR_AVG_PERIOD_CMD_STR;
	static const char* SENSOR_HEATING_CMD_STR;
	static const char* SENSOR_NMEA_ID_STR_CMD_STR;
	static const char* SENSOR_MSG_TERM_CMD_STR;
	static const char* SENSOR_MSG_STREAM_CMD_STR;
	static const char* SENSOR_NODE_ADDR_CMD_STR;
	static const char* SENSOR_OUTPUT_FIELD_FMT_CMD_STR;
	static const char* SENSOR_OUTPUT_RATE_CMD_STR;
	static const char* SENSOR_START_MEAS_CMD_STR;
	static const char* SENSOR_MEAS_UNITS_CMD_STR;
	static const char* SENSOR_VERT_MEAS_PADDING_CMD_STR;
	static const char* SENSOR_ALIGNMENT_CMD_STR;

    // table to hold the strings for easy lookup
    static const char* cmdTable[NUM_SENSOR_CMDS];

    // NOTE: list sensor bauds from highest to lowest as the higher  
    //       ones are the most likely
    static const int SENSOR_BAUDS[NUM_BAUD_ARGS];
    static const GILL2D_DATA_WORD_ARGS SENSOR_WORD_SPECS[NUM_DATA_WORD_ARGS];
    static const int NUM_PORT_TYPES = 2;
    static const n_c::PORT_TYPES SENSOR_PORT_TYPES[NUM_PORT_TYPES];

    static const n_c::PortConfig DEFAULT_PORT_CONFIG;

    static const int CHAR_WRITE_DELAY = NSECS_PER_MSEC * 110; // 110mSec

    n_c::PortConfig testPortConfig;
    n_c::PortConfig desiredPortConfig;
    n_c::MessageConfig defaultMessageConfig;

    GILL2D_CMD_ARG* desiredScienceParameters;

    char unitId;
    bool polling;


    // no copying
    GILL2D(const GILL2D&);

    // no assignment
    GILL2D& operator=(const GILL2D&);
};

}}}	// namespace nidas namespace dynld namespace isff

#endif
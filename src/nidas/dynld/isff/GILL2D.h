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
#include <nidas/core/Metadata.h>

#include <memory>

namespace nidas { namespace dynld { namespace isff {

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
    SENSOR_SOS_TEMP_CMD,
    //TODO - Add C, T, etc commands so that this becomes more generic.
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

/*
 *  Configuration cmmands for GILL2D sonic anemometers are typically
 *  single letters, A-Z, with some letters being left out, depending
 *  on the model.
 *
 *  Arguments for these commands are typically a single digit, 0-9.
 *  So the following enums define those digits for each command.
 */

enum GILL2D_SOS_TEMP_ARGS
{
    REPORT_DISABLED=0,
    REPORT_SOS,    // speed of sound
    REPORT_TEMP,
    REPORT_BOTH,
    NUM_SOS_TEMP_ARGS
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
    HTG_DISABLED = 1,
    HTG_ACTIVE,
    NUM_HEATING_ARGS = HTG_ACTIVE
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


class GILL2D_Metadata: public nidas::core::SensorMetadata
{
public:
    GILL2D_Metadata(): nidas::core::SensorMetadata("GILL2D_Metadata")
    {
    }

    MetadataInt averaging{this, READWRITE, "averaging", "Avg secs"};
    MetadataString sos_temp{this, READWRITE, "sos_temp", "SpdOfSnd/Temp Rprt"};
    MetadataString heating{this, READWRITE, "heating", "Heater"};
    MetadataString nmea_id_str{this, READWRITE, "nmea_id_str", "NMEA"};
    MetadataString msg_term{this, READWRITE, "msg_term", "Msg Term"};
    MetadataString msg_stream{this, READWRITE, "msg_stream", "Msg Stream"};
    MetadataString field_fmt{this, READWRITE, "field_fmt", "Field Fmt"};
    MetadataInt output_rate{this, READWRITE, "output_rate", "Output Rate Hz"};
    MetadataString meas_units{this, READWRITE, "meas_units", "Meas Units"};
    MetadataString node_addr{this, READWRITE, "node_addr", "Node Addr"};
    MetadataString vert_meas_pad{this, READWRITE, "vert_meas_pad", "Vert Pad"};
    MetadataString align_45_deg{this, READWRITE, "align_45_deg", "Align/45 Deg"};
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

    // override fromDOMElement() to provide a means to intercept custom
    // auto config instructions from the XML
    void fromDOMElement(const xercesc::DOMElement* node)
        throw(nidas::util::InvalidParameterException);

    /**
     * Gill WindObserver 65 sonics only support output rates of 1, 2, 4, 5,
     * 8, and 10 Hertz.  Models 75 and 90 only support 1, 2, and 4, but
     * this test does not distinguish between models.
     **/
    bool
    validOutputRate(unsigned int rate);

    /**
     * Convert the rate as a string into an integer rate, in hertz, and
     * throw InvalidParameterException if it is not a valid string or rate.
     **/
    unsigned int
    outputRateFromString(const std::string& aval);

    /**
     * Set the desired output rate for this sensor, in Hz.  If the rate is
     * invalid, throws InvalidParameterException.  See validOutputRate().
     **/
    void
    setOutputRate(unsigned int rate);

    /**
     * Return the Gill command to set outputrate to the given rate in Hz.
     **/
    std::string
    outputRateCommand(unsigned int rate);

    /**
     * Format the sensor command and its argument into the string that
     * would be written to the Gill.
     **/
    std::string
    formatSensorCmd(int cmd, nidas::core::SensorCmdArg arg);

    /**
     * Parse the response string from the configuration query command D3,
     * and update all the metadata settings.  Return false if the string
     * cannot be parsed.
     **/
    bool
    parseConfigResponse(const std::string& respStr);

    void getMetadata(nidas::core::MetadataInterface&) override;

protected:
    void
    sendSensorCmd(int cmd,
                  nidas::core::SensorCmdArg arg=nidas::core::SensorCmdArg(0));
    bool compareScienceParameter(GILL2D_COMMANDS cmd, const std::string& match);
    void updateDesiredScienceParameter(GILL2D_COMMANDS cmd, int arg=0);
    nidas::core::SensorCmdData getDesiredCmd(GILL2D_COMMANDS cmd);
    bool checkConfigMode(bool continuous = CONTINUOUS);
    bool isConfigCmd(int cmd)
    {
        return (cmd != SENSOR_DIAG_QRY_CMD
                && cmd != SENSOR_QRY_ID_CMD
                && cmd != SENSOR_CONFIG_MODE_CMD
                && cmd != SENSOR_ENABLE_POLLED_MODE_CMD
                && cmd != SENSOR_POLL_MEAS_CMD
                && cmd != SENSOR_DISABLE_POLLED_MODE_CMD);
    }
    bool confirmGillSerialPortChange(int cmd, int arg);

    virtual nidas::core::CFG_MODE_STATUS enterConfigMode();
    virtual void exitConfigMode();
    virtual bool checkResponse();
    virtual bool
    installDesiredSensorConfig(const nidas::core::PortConfig& rDesiredConfig);
    virtual void sendScienceParameters();
    virtual bool checkScienceParameters();
    virtual void updateMetaData();
    void transformEmbeddedNulls(std::string& respStr, bool allowStxEtx=false);

private:
    static const nidas::core::PortType DEFAULT_PORT_TYPE;


    static const int MIN_AVERAGING_TIME = 0;
    static const int MAX_AVERAGING_TIME = 3600;

    static const int NUM_DEFAULT_SCIENCE_PARAMETERS;
    static const nidas::core::SensorCmdData DEFAULT_SCIENCE_PARAMETERS[];

    // GILL2D pre-packaged commands
    static const char* SENSOR_CONFIG_MODE_CMD_STR;
    static const char* SENSOR_ENABLE_POLLED_MODE_CMD_STR;
    static const char* SENSOR_POLL_MEAS_CMD_STR;
    static const char* SENSOR_QRY_ID_CMD_STR;
    static const char* SENSOR_DISABLE_POLLED_MODE_CMD_STR;
    static const char* SENSOR_SOS_TEMP_CMD_STR;
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

    nidas::core::SensorCmdData* _desiredScienceParameters;
    bool _sosEnabled;

    char _unitId;

    GILL2D_Metadata _metadata;

    // no copying
    GILL2D(const GILL2D&);

    // no assignment
    GILL2D& operator=(const GILL2D&);
};

}}} // namespace nidas namespace dynld namespace isff

#endif

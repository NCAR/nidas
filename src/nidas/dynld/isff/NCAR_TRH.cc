// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
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

#include "NCAR_TRH.h"
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/core/CalFile.h>
#include <nidas/core/AsciiSscanf.h>

#include <sstream>
#include <limits>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,NCAR_TRH)

/* 
 *  Autoconfig port defintions
 */
// NOTE: list sensor bauds from highest to lowest as the higher
//       ones are the most likely
static const int NUM_SENSOR_BAUDS = 1;
static const int SENSOR_BAUDS[NUM_SENSOR_BAUDS] = {9600};
static const int NUM_SENSOR_WORD_SPECS = 1;
static const WordSpec SENSOR_WORD_SPECS[NUM_SENSOR_WORD_SPECS] =
{
    WordSpec(8, Termios::NONE, 1),
};

// TRH instruments only use RS232
static const int NUM_PORT_TYPES = 1;
static const PORT_TYPES SENSOR_PORT_TYPES[NUM_PORT_TYPES] = {RS232};

// static default configuration to send to base class...
static const int DEFAULT_BAUD_RATE = SENSOR_BAUDS[0];
static const int DEFAULT_DATA_BITS = SENSOR_WORD_SPECS[0].dataBits;
static const n_u::Termios::parity DEFAULT_PARITY = SENSOR_WORD_SPECS[0].parity;
static const int DEFAULT_STOP_BITS = SENSOR_WORD_SPECS[0].stopBits;
static const PORT_TYPES DEFAULT_PORT_TYPE = SENSOR_PORT_TYPES[0];
static const TERM DEFAULT_SENSOR_TERMINATION = NO_TERM;
static const int DEFAULT_RTS485 = 0;
static const bool DEFAULT_CONFIG_APPLIED = false;

static const PortConfig DEFAULT_PORT_CONFIG(DEFAULT_BAUD_RATE, DEFAULT_DATA_BITS,
                                             DEFAULT_PARITY, DEFAULT_STOP_BITS,
                                             DEFAULT_PORT_TYPE, DEFAULT_SENSOR_TERMINATION,
                                             DEFAULT_RTS485, DEFAULT_CONFIG_APPLIED);

/* 
 *  AutoConfig: TRH Commands
 */
static const SensorCmdData DEFAULT_SCIENCE_PARAMETERS[] =
{
    SensorCmdData((int)ENTER_EEPROM_MENU_CMD, SensorCmdArg()),
    SensorCmdData(DATA_RATE_CMD, SensorCmdArg(10)),
    SensorCmdData(EXIT_EEPROM_MENU_CMD, SensorCmdArg())
};

const int NUM_DEFAULT_SCIENCE_PARAMETERS = sizeof(DEFAULT_SCIENCE_PARAMETERS)/sizeof(SensorCmdData);

// default message parameters for the PB210
static const int DEFAULT_MESSAGE_LENGTH = 0;
static const bool DEFAULT_MSG_SEP_EOM = true;
static const char* DEFAULT_MSG_SEP_CHARS = "\n";
static MessageConfig defaultMessageConfig(DEFAULT_MESSAGE_LENGTH, DEFAULT_MSG_SEP_CHARS, DEFAULT_MSG_SEP_EOM);

NCAR_TRH::NCAR_TRH():
    SerialSensor(DEFAULT_PORT_CONFIG),
    _ifan(),
    _minIfan(-numeric_limits<float>::max()),
    _maxIfan(numeric_limits<float>::max()),
    _traw(),
    _rhraw(),
    _t(),
    _rh(),
    _Ta(),
    _Ha(),
    _raw_t_handler(0),
    _raw_rh_handler(0),
    _compute_order(),
    _desiredScienceParameters(0),
    _scienceParametersOk(false)
{
    _raw_t_handler = makeCalFileHandler
        (std::bind1st(std::mem_fun(&NCAR_TRH::handleRawT), this));
    _raw_rh_handler = makeCalFileHandler
        (std::bind1st(std::mem_fun(&NCAR_TRH::handleRawRH), this));

    // We set the defaults at construction, 
    // letting the base class modify according to fromDOMElement() 
    setMessageParameters(defaultMessageConfig);

    // Let the base class know about PTB210 RS232 limitations
    for (int i=0; i<NUM_PORT_TYPES; ++i) {
    	_portTypeList.push_back(SENSOR_PORT_TYPES[i]);
    }

    for (int i=0; i<NUM_SENSOR_BAUDS; ++i) {
    	_baudRateList.push_back(SENSOR_BAUDS[i]);
    }

    for (int i=0; i<NUM_SENSOR_WORD_SPECS; ++i) {
    	_serialWordSpecList.push_back(SENSOR_WORD_SPECS[i]);
    }

    _desiredScienceParameters = new SensorCmdData[NUM_DEFAULT_SCIENCE_PARAMETERS];
    for (int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        _desiredScienceParameters[i] = DEFAULT_SCIENCE_PARAMETERS[i];
    }

    setManufacturer("NCAR");
    setModel("TRH");
    initCustomMetadata();
    setAutoConfigSupported();
}

NCAR_TRH::~NCAR_TRH()
{
    delete _raw_t_handler;
    delete _raw_rh_handler;
    delete [] _desiredScienceParameters;
}


void NCAR_TRH::convertNext(const VariableIndex& vi)
{
    if (vi.valid())
    {
        vector<VariableIndex>::iterator it;
        it = find(_compute_order.begin(), _compute_order.end(), vi);
        if (it == _compute_order.end())
        {
            _compute_order.push_back(vi);
        }
    }
}


void NCAR_TRH::validate() throw(n_u::InvalidParameterException)
{
    nidas::core::SerialSensor::validate();

    list<SampleTag*>& tags = getSampleTags();
    if (tags.size() != 1)
        throw n_u::InvalidParameterException
            ("NCAR_TRH sensor only handles a single sample tag.");
    SampleTag* stag = *tags.begin();

    _traw = findVariableIndex("Traw");
    _rhraw = findVariableIndex("RHraw");
    _rh = findVariableIndex("RH");
    _t = findVariableIndex("T");
    _ifan = findVariableIndex("Ifan");

    if (_ifan.valid())
    {
        Variable* ifan = _ifan.variable();
        _minIfan = ifan->getMinValue();
        _maxIfan = ifan->getMaxValue();
        ifan->setMinValue(-numeric_limits<float>::max());
        ifan->setMaxValue(numeric_limits<float>::max());
    }

    // Check the T and RH variables for converters.  If found, inject a
    // callback so this sensor can handle requests for raw calibrations,
    // meaning T and RH will be calculated from Traw and RHraw using the
    // coefficients stored here.  When a raw conversion is enabled, the
    // corresponding array of coefficients is non-empty, otherwise the
    // variable's converter is applied as usual.

    VariableConverter* vc;
    if (_t.valid() && (vc = _t.variable()->getConverter()))
    {
        DLOG(("sensor ") << getName()
             << ": installing CalFile handler for raw T");
        vc->setCalFileHandler(this->_raw_t_handler);
    }
    if (_rh.valid() && (vc = _rh.variable()->getConverter()))
    {
        DLOG(("sensor ") << getName()
             << ": installing CalFile handler for raw RH");
        vc->setCalFileHandler(this->_raw_rh_handler);
    }

    // Finally, we need to process variables in a particular order,
    // followed by anything else that wasn't inserted already.  Clear the
    // compute order in case validate() is called multiple times.
    _compute_order.clear();
    convertNext(_traw);
    convertNext(_rhraw);
    convertNext(_t);
    convertNext(_rh);
    convertNext(_ifan);

    const vector<Variable*>& vars = stag->getVariables();
    for (unsigned int iv = 0; iv < vars.size(); iv++)
    {
        convertNext(VariableIndex(vars[iv], iv));
    }
    static n_u::LogContext lp(LOG_DEBUG);
    if (lp.active())
    {
        n_u::LogMessage msg(&lp);
        msg << "TRH sensor " << getName() << " variable compute order: ";
        for (unsigned int iv = 0; iv < _compute_order.size(); iv++)
        {
            if (iv != 0)
                msg << ",";
            msg << _compute_order[iv].variable()->getName();
        }
    }
}


namespace {
void
setCoefficients(std::vector<float>& dest, int max, float* begin, float* end)
{
    if (!begin || begin == end)
    {
        dest.resize(0);
    }
    else
    {
        if (!end)
            end = begin + max;
        dest.resize(max, 0);
        vector<float>::iterator it = dest.begin();
        for (float* fp = begin ; fp != end && it != dest.end(); ++fp, ++it)
        {
            *it = *fp;
        }
    }
}
}

void
NCAR_TRH::
setRawTempCoefficients(float* begin, float* end)
{
    setCoefficients(_Ta, 3, begin, end);
}

void
NCAR_TRH::
setRawRHCoefficients(float* begin, float* end)
{
    setCoefficients(_Ha, 5, begin, end);
}


std::vector<float>
NCAR_TRH::
getRawTempCoefficients()
{
    return _Ta;
}


std::vector<float>
NCAR_TRH::
getRawRHCoefficients()
{
    return _Ha;
}


bool
NCAR_TRH::
handleRawT(CalFile* cf)
{
    // If the record starts with raw, then grab the raw temperature
    // coefficients, otherwise rest the raw coefficients and pass the
    // handling on the converter.
    const std::vector<std::string>& fields = cf->getCurrentFields();

    if (fields.size() > 0 && fields[0] == "raw")
    {
        // To compute T from raw, we need the raw T, so make sure it's
        // available.
        if (!_traw)
        {
            std::ostringstream out;
            out << "raw temperature calibration requested in "
                << cf->getCurrentFileName() << ", line " << cf->getLineNumber()
                << ", but Traw is not available from this sensor: "
                << getName();
            throw n_u::InvalidParameterException(out.str());
        }
        ILOG(("") << "sensor " << getName() << " switching to raw "
             << "T calibrations at "
             << n_u::UTime(cf->getCurrentTime()).format(true, "%Y%m%d,%H:%M:%S"));
        _Ta.resize(3);
        cf->getFields(1, 4, &(_Ta[0]));
        return true;
    }
    ILOG(("") << "sensor " << getName() << " disabling raw "
         << "T calibrations at "
         << n_u::UTime(cf->getCurrentTime()).format(true, "%Y%m%d,%H:%M:%S"));
    _Ta.resize(0);
    return false;
}


bool
NCAR_TRH::
handleRawRH(CalFile* cf)
{
    const std::vector<std::string>& fields = cf->getCurrentFields();

    if (fields.size() > 0 && fields[0] == "raw")
    {
        if (!_rhraw)
        {
            std::ostringstream out;
            out << "raw humidity calibration requested in "
                << cf->getCurrentFileName() << ", line " << cf->getLineNumber()
                << ", but RHraw is not available from this sensor: "
                << getName();
            throw n_u::InvalidParameterException(out.str());
        }
        ILOG(("") << "sensor " << getName() << " switching to raw "
             << "RH calibrations at "
             << n_u::UTime(cf->getCurrentTime()).format(true, "%Y%m%d,%H:%M:%S"));
        _Ha.resize(5);
        cf->getFields(1, 6, &(_Ha[0]));
        return true;
    }
    ILOG(("") << "sensor " << getName() << " disabling raw "
         << "RH calibrations at "
         << n_u::UTime(cf->getCurrentTime()).format(true, "%Y%m%d,%H:%M:%S"));
    _Ha.resize(0);
    return false;
}


/**
 * Here are the lines from the SHT PIC code.  As you would expect, the
 * coefficients would come from the files in the order Ta0, Ta1, Ta2, and Ha0,
 * Ha1, Ha2, Ha3, Ha4.
 *
 * temp_cal = Ta0 + Ta1*t + Ta2*t*t;
 *
 * humi_cal = Ha0 + Ha1*rh + Ha2*rh*rh + (Ha3 + Ha4*rh) * temp_cal;
 **/
double
NCAR_TRH::
tempFromRaw(double traw)
{
    double temp_cal = _Ta[0] + _Ta[1] * traw + _Ta[2] * traw * traw;
    return temp_cal;
}
    

double
NCAR_TRH::
rhFromRaw(double rhraw, double temp_cal)
{
    double humi_cal = _Ha[0] + _Ha[1]*rhraw + _Ha[2]*rhraw*rhraw +
        (_Ha[3] + _Ha[4]*rhraw) * temp_cal;
    return humi_cal;
}



void
NCAR_TRH::
convertVariable(SampleT<float>* outs, Variable* var, float* fp)
{
    VariableConverter* vc = var->getConverter();
    // Advance the cal file, if necessary.  This also causes any raw
    // calibrations to be handled.
    if (vc)
        vc->readCalFile(outs->getTimeTag());
    float* values = outs->getDataPtr();
    if (_t.valid() && _t.variable() == var && !_Ta.empty())
    {
        float Traw = values[_traw.index()];
        float T = tempFromRaw(Traw);
        *fp = T;
    }
    else if (_rh.valid() && _rh.variable() == var && !_Ha.empty())
    {
        float RHraw = values[_rhraw.index()];
        float T = values[_t.index()];
        *fp = rhFromRaw(RHraw, T);
    }
    else
    {
        var->convert(outs->getTimeTag(), fp);
    }
}



bool
NCAR_TRH::
process(const Sample* samp, std::list<const Sample*>& results) throw()
{
    // Try to scan the variables of a sample tag from the raw sensor
    // message.
    SampleTag* stag = 0;
    SampleT<float>* outs = searchSampleScanners(samp, &stag);
    if (!outs)
    {
        return false;
    }

    // Apply any time tag adjustments.
    adjustTimeTag(stag, outs);

    // Apply any variable conversions.  This replaces the call to
    // applyConversions() in the base class process() method, because we
    // need to detect and handle raw conversions.  We also need to do them
    // in a specific order: raw first, then T, then RH.

    float* values = outs->getDataPtr();
    for (unsigned int iv = 0; iv < _compute_order.size(); iv++)
    {
        VariableIndex vi(_compute_order[iv]);
        convertVariable(outs, vi.variable(), values + vi.index());
    }

    results.push_back(outs);
    ifanFilter(results);
    return true;
}

void
NCAR_TRH::
ifanFilter(std::list<const Sample*>& results)
{
    const Sample* csamp = results.front();
    unsigned int slen = csamp->getDataLength();

    if (_ifan.valid())
    {
        float ifan = csamp->getDataValue(_ifan.index());

        // flag T,RH if Ifan is less than _minIfan
        if (ifan < _minIfan || ifan > _maxIfan)
        {
            SampleT<float>* news = getSample<float>(slen);
            news->setTimeTag(csamp->getTimeTag());
            news->setId(csamp->getId());

            float* nfptr = news->getDataPtr();

            unsigned int i;
            // flag any values other than Ifan
            for (i = 0; i < slen; i++)
            {
                if (int(i) != _ifan.index())
                    nfptr[i] = floatNAN;
                else
                    nfptr[i] = csamp->getDataValue(i);
            }

            csamp->freeReference();

            results.front() = news;
        }
    }
}

/* 
 *  AutoConfig: TRH Commands
 */

// Get into the EEPROM menu
static const char* ENTER_EEPROM_MENU_CMD_STR =  "\x15";
// Available once in the EEPROM menu     
static const char* FW_VERSION_CMD_STR =         "VER";  // print out the firmware version
static const char* DATA_RESOLUTION_CMD_STR =    "RES";  // arg is 0=14bits or 1=12bits
static const char* SENSOR_ID_CMD_STR =          "SID";  // set the instrument ID
static const char* DATA_RATE_CMD_STR =          "RAT";  // Integrals of 0.1 seconds between sample messages
static const char* FAN_DUTY_CYCLE_CMD_STR =     "DUT";  // Fan motor duty cycle in % (0-100)
static const char* MIN_FAN_DUTY_CYCLE_CMD_STR = "Rmin"; // Minimum fan motor duty cycle (not implemented)
static const char* EEPROM_INIT_STATE_CMD_STR =  "STA";  // 1 = initialized, 255 = uninitialized
static const char* TEMP_CAL_0_CMD_STR =         "TA0";  // temp cal value 0 - float value
static const char* TEMP_CAL_1_CMD_STR =         "TA1";  // temp cal value 1 - float value
static const char* TEMP_CAL_2_CMD_STR =         "TA2";  // temp cal value 2 - float value
static const char* HUMD_CAL_0_CMD_STR =         "HA0";  // humidity cal value 0 - float value
static const char* HUMD_CAL_1_CMD_STR =         "HA0";  // humidity cal value 1 - float value
static const char* HUMD_CAL_2_CMD_STR =         "HA0";  // humidity cal value 2 - float value
static const char* HUMD_CAL_3_CMD_STR =         "HA0";  // humidity cal value 3 - float value
static const char* HUMD_CAL_4_CMD_STR =         "HA0";  // humidity cal value 4 - float value
static const char* CLEAR_EEPROM_CMD_STR =       "EEC";  // clear EEPROM contents
static const char* DEFAULT_EEPROM_CMD_STR =     "DEF";  // set EEPROM contents to default values
static const char* SHOW_CMDS_CMD_STR =          "CMD";  // print out this list of commands
static const char* SHOW_SETTINGS_CMD_STR =      "SET";  // print out all values which can be assigned
static const char* EXIT_EEPROM_MENU_CMD_STR =   "EXT";  // Exit EEPROM menu and reset

static const char* cmdTable[NUM_SENSOR_CMDS] =
{
    0,
    ENTER_EEPROM_MENU_CMD_STR, 
    FW_VERSION_CMD_STR,
    DATA_RESOLUTION_CMD_STR, 
    SENSOR_ID_CMD_STR,  
    DATA_RATE_CMD_STR,       
    FAN_DUTY_CYCLE_CMD_STR,  
    MIN_FAN_DUTY_CYCLE_CMD_STR,
    EEPROM_INIT_STATE_CMD_STR,
    TEMP_CAL_0_CMD_STR,
    TEMP_CAL_1_CMD_STR,
    TEMP_CAL_2_CMD_STR,
    HUMD_CAL_0_CMD_STR,
    HUMD_CAL_1_CMD_STR,
    HUMD_CAL_2_CMD_STR,
    HUMD_CAL_3_CMD_STR,
    HUMD_CAL_4_CMD_STR,
    CLEAR_EEPROM_CMD_STR,
    DEFAULT_EEPROM_CMD_STR,
    SHOW_CMDS_CMD_STR,
    SHOW_SETTINGS_CMD_STR,
    EXIT_EEPROM_MENU_CMD_STR,
};

/* 
 *  AutoConfig: TRH mode/data detection
 * 
 *  Typical SET response:
 * =====================================
 * TRH Code Version:  5.140
 * Sensor ID170    data rate:   1.0 (secs)  fan PWM duty cycle (%): 40   fan min RPM: -1
 * resolution: 12 bits
 * calibration coefficients:
 * Ta0 = -40.504280
 * Ta1 = 0.040956
 * Ta2 = -0.000000
 * Ha0 = -12.705780
 * Ha1 = 0.724077
 * Ha2 = -0.000885
 * Ha3 = 0.095170
 * Ha4 = 0.000509

 * ===================================
 * 
 *  TODO: Is the below note still true???
 *  NOTE: If fan is not present, then fan([0-9]+) simply becomes fan and Fa0 line is missing
 *        Also, Calibration Dates line may or may not be missing. 
 * 
 *        These anomalies are accounted for in the regex and by adjusting the desired value index
 *        when collecting metadata
 */

//input command format: cmd [value] [[value] [value] ...]

static string ENTER_EEPROM_MENU_CMD_RESP_SPEC("[[:space:]]+EEprom:");
static regex ENTER_EEPROM_MENU_CMD_RESP(ENTER_EEPROM_MENU_CMD_RESP_SPEC, std::regex_constants::extended);
static string EXIT_EEPROM_MENU_CMD_RESP_SPEC("Exit EEPROM LOADER - reset will take place");
static regex EXIT_EEPROM_MENU_CMD_RESP(EXIT_EEPROM_MENU_CMD_RESP_SPEC, std::regex_constants::extended);
static string SET_DATA_REGEX_SPEC(
    "TRH Code Version:  ([0-9]+[.][0-9]+)"
    "[[:space:]]+Sensor ID([0-9]+)    data rate:   ([0-9]+[.][0-9]+) \\(secs\\)  fan PWM duty cycle \\(%\\): ([0-9]+)   fan min RPM: (-*[0-9]+)"
    "[[:space:]]+resolution: ([0-9]+) bits" 
    "[[:space:]]+calibration coefficients:"
    "[[:space:]]+Ta0[[:blank:]]+=[[:blank:]]+(-*[0-9]+[.][0-9]+)"
    "[[:space:]]+Ta1[[:blank:]]+=[[:blank:]]+(-*[0-9]+[.][0-9]+)"
    "[[:space:]]+Ta2[[:blank:]]+=[[:blank:]]+(-*[0-9]+[.][0-9]+)"
    "[[:space:]]+Ha0[[:blank:]]+=[[:blank:]]+(-*[0-9]+[.][0-9]+)"
    "[[:space:]]+Ha1[[:blank:]]+=[[:blank:]]+(-*[0-9]+[.][0-9]+)"
    "[[:space:]]+Ha2[[:blank:]]+=[[:blank:]]+(-*[0-9]+[.][0-9]+)"
    "[[:space:]]+Ha3[[:blank:]]+=[[:blank:]]+(-*[0-9]+[.][0-9]+)"
    "[[:space:]]+Ha4[[:blank:]]+=[[:blank:]]+(-*[0-9]+[.][0-9]+)"
);
static regex SET_DATA(SET_DATA_REGEX_SPEC, std::regex_constants::extended); 
                                        //    | std::regex_constants::ECMAScript);

static const int TRH_FW_VERSION_IDX = 1;
static const int SENSOR_ID_IDX = 2;
static const int DATA_RATE_IDX = 3;
static const int FAN_DUTY_CYCLE_IDX = 4;
static const int FAN_MIN_RPM_IDX = 5;
static const int ADC_RESOLUTION_IDX = 6;
static const int TCAL_COEFF_0_IDX = 7;
static const int TCAL_COEFF_1_IDX = 8;
static const int TCAL_COEFF_2_IDX = 9;
static const int HCAL_COEFF_0_IDX = 10;
static const int HCAL_COEFF_1_IDX = 11;
static const int HCAL_COEFF_2_IDX = 12;
static const int HCAL_COEFF_3_IDX = 13;
static const int HCAL_COEFF_4_IDX = 14;

// Typical TRH output
//
// TRH w/fan
// TRH70 23.35 50.87 0 0 1578 94 0
static string TRH_OUTPUT_SPEC("TRH[0-9]+[[:blank:]]+[0-9]+[.][0-9]+[[:blank:]]+[0-9]+[.][0-9]+[[:blank:]]+[0-9]+[[:blank:]]+[0-9]+[[:blank:]]+[0-9]+[[:blank:]]+[0-9]+[[:blank:]]+[0-9]+");
static regex TRH_OUTPUT(TRH_OUTPUT_SPEC, std::regex_constants::extended);

static string INT_CMD_RESPONSE_SPEC("[[:space:]]*([0-9]+)");
static regex INT_CMD_RESPONSE(INT_CMD_RESPONSE_SPEC, std::regex_constants::extended);
static string FLOAT_CMD_RESPONSE_SPEC("[[:space:]]*([0-9]+[.][0-9]+)");
static regex FLOAT_CMD_RESPONSE(FLOAT_CMD_RESPONSE_SPEC, std::regex_constants::extended);

void cleanupUnprintables(char* buf, int buflen)
{
    DLOG(("NCAR_TRH.cc - cleanupUnprintables(): Changing unprintable chars to spaces..."));
    for (int i=0; i < buflen; ++i) {
        int ichar = (int)buf[i];
        if ((ichar < 32 && !(ichar == '\n' || ichar == '\r'))
            || ichar > 126) {
            buf[i] = ' ';
        }
    }
}

bool 
NCAR_TRH::
checkResponse()
{
    return sendAndCheckSensorCmd(ENTER_EEPROM_MENU_CMD);
}

void 
NCAR_TRH::
sendSensorCmd(int cmd, SensorCmdArg arg, bool /* resetNow */)
{
    assert(cmd < NUM_SENSOR_CMDS);
    std::string snsrCmd(cmdTable[cmd]);

    std::ostringstream argStr;

    switch (cmd) {
        // these commands all take an argument...
    	// these take integers...
        case SENSOR_ID_CMD:
        case DATA_RATE_CMD:
        case RESOLUTION_CMD:
        case FAN_DUTY_CYCLE_CMD:
        case FAN_MIN_RPM_CMD:
        case EEPROM_INIT_STATE_CMD:
        case TEMP_CAL_0_CMD:
        case TEMP_CAL_1_CMD:
        case TEMP_CAL_2_CMD:
        case HUMD_CAL_0_CMD:
        case HUMD_CAL_1_CMD:
        case HUMD_CAL_2_CMD:
        case HUMD_CAL_3_CMD:
        case HUMD_CAL_4_CMD:
            argStr << " " << arg.intArg << std::endl;
            break;

        // TODO: Figure out whether to use the commands to just get the data, or always 
        //       use them to set the data and rely on SET to get the data.

        // these do not take arguments...
		case EXIT_EEPROM_MENU_CMD:
		case ENTER_EEPROM_MENU_CMD:
        case FW_VERSION_CMD:
        case CLEAR_EEPROM_CMD:
        case DEFAULT_EEPROM_CMD:
        case SHOW_CMDS_CMD:
        case SHOW_SETTINGS_CMD:
            argStr << std::endl;
            break;

        default:
            throw n_u::InvalidParameterException("Invalid TRH Command Index");
            break;
    }

    DLOG(("NCAR_TRH::sendSensorCmd(): argStr: ") << argStr.str());
    // Append command string w/argStr, which may be blank, except for the return char, if any
    snsrCmd.append(argStr.str());
    DLOG(("NCAR_TRH::sendSensorCmd(): snsrCmd: ") << snsrCmd);

    // Write the command - assume the port is already open
    // The NCAR_TRH seems to not be able to keep up with a burst of data, so
    // give it some time between chars - i.e. ~80 words/min rate
    writePause(snsrCmd.c_str(), snsrCmd.length());
}

/*
 *  Metadata 
 */
static const string DATA_RATE_DESC("Data Rate");
static const string FAN_DUTY_CYCLE_DESC("Fan Duty Cycle");
static const string FAN_MIN_RPM_DESC("Fan Min RPM");
static const string ADC_RES_DESC("ADC Resolution");
static const string TA0_COEFF_DESC("Ta0");
static const string TA1_COEFF_DESC("Ta1");
static const string TA2_COEFF_DESC("Ta2");
static const string HA0_COEFF_DESC("Ha0");
static const string HA1_COEFF_DESC("Ha1");
static const string HA2_COEFF_DESC("Ha2");
static const string HA3_COEFF_DESC("Ha3");
static const string HA4_COEFF_DESC("Ha4");

void 
NCAR_TRH::
initCustomMetadata()
{
    addMetaDataItem(MetaDataItem(DATA_RATE_DESC, ""));
    addMetaDataItem(MetaDataItem(ADC_RES_DESC, ""));
    addMetaDataItem(MetaDataItem(TA0_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(TA1_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(TA2_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(HA0_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(HA1_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(HA2_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(HA3_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(HA4_COEFF_DESC, ""));
}

bool 
NCAR_TRH::
captureEepromMetaData(const char* buf)
{
    // TODO DLOG(("NCAR_TRH::captureEepromMetaData(): regex: ") << SENSOR_RESET_METADATA_REGEX_SPEC);
    DLOG(("NCAR_TRH::captureEepromMetaData(): matching:") << std::string(buf));
    cmatch results;
    bool regexFound = regex_search(buf, results, SET_DATA);
    if (regexFound && results[0].matched) {
        if (results[SENSOR_ID_IDX].matched) {
            setSerialNumber(results[SENSOR_ID_IDX].str());
        }
    
        if (results[TRH_FW_VERSION_IDX].matched) {
            setFwVersion(results[TRH_FW_VERSION_IDX].str());
        }
    
        if (results[DATA_RATE_IDX].matched) {
            updateMetaDataItem(MetaDataItem(DATA_RATE_DESC, results.str(DATA_RATE_IDX)));
        }
    
        if (results[FAN_DUTY_CYCLE_IDX].matched) {
            updateMetaDataItem(MetaDataItem(FAN_DUTY_CYCLE_DESC, results.str(FAN_DUTY_CYCLE_IDX)));
        }
    
        if (results[FAN_MIN_RPM_IDX].matched) {
            updateMetaDataItem(MetaDataItem(FAN_MIN_RPM_DESC, results.str(FAN_MIN_RPM_IDX)));
        }
    
        if (results[ADC_RESOLUTION_IDX].matched) {
            updateMetaDataItem(MetaDataItem(ADC_RES_DESC, results.str(ADC_RESOLUTION_IDX)));
        }
    
        if (results[TCAL_COEFF_0_IDX].matched) {
            updateMetaDataItem(MetaDataItem(TA0_COEFF_DESC, results.str(TCAL_COEFF_0_IDX)));
        }
    
        if (results[TCAL_COEFF_1_IDX].matched) {
            updateMetaDataItem(MetaDataItem(TA1_COEFF_DESC, results.str(TCAL_COEFF_1_IDX)));
        }
    
        if (results[TCAL_COEFF_2_IDX].matched) {
            updateMetaDataItem(MetaDataItem(TA2_COEFF_DESC, results.str(TCAL_COEFF_2_IDX)));
        }
    
        if (results[HCAL_COEFF_0_IDX].matched) {
            updateMetaDataItem(MetaDataItem(HA0_COEFF_DESC, results.str(HCAL_COEFF_0_IDX)));
        }
    
        if (results[HCAL_COEFF_1_IDX].matched) {
            updateMetaDataItem(MetaDataItem(HA1_COEFF_DESC, results.str(HCAL_COEFF_1_IDX)));
        }
    
        if (results[HCAL_COEFF_2_IDX].matched) {
            updateMetaDataItem(MetaDataItem(HA2_COEFF_DESC, results.str(HCAL_COEFF_2_IDX)));
        }
    
        if (results[HCAL_COEFF_3_IDX].matched) {
            updateMetaDataItem(MetaDataItem(HA3_COEFF_DESC, results.str(HCAL_COEFF_3_IDX)));
        }
    
        if (results[HCAL_COEFF_4_IDX].matched) {
            updateMetaDataItem(MetaDataItem(HA4_COEFF_DESC, results.str(HCAL_COEFF_4_IDX)));
        }
    }
    else {
        DLOG(("NCAR_TRH::captureEepromMetaData(): Didn't find overall match to string as expected."));
    }

    return regexFound;
}

bool 
NCAR_TRH::
checkCmdResponse(TRH_SENSOR_COMMANDS cmd, SensorCmdArg arg)
{
	bool responseOK = false;
	bool checkMatch = true;
    int BUF_SIZE = 1000;
    int selectTimeout = 4;
    bool checkForUnprintables = true;
    int retryTimeoutFactor = 3;

    switch (cmd) {
        case EXIT_EEPROM_MENU_CMD:
            checkForUnprintables = false;
            BUF_SIZE = 60;
            break;

        case ENTER_EEPROM_MENU_CMD:
            checkForUnprintables = false;
            // BUF_SIZE = 120;
            retryTimeoutFactor = 10;
            break;

        default:
            break;
    }

    auto_ptr<char> respBuf(new char[BUF_SIZE]);
    char* buf = respBuf.get();
    memset(buf, 0, BUF_SIZE);
    int numCharsRead = readEntireResponse(buf, BUF_SIZE-1, selectTimeout,
                                          checkForUnprintables, retryTimeoutFactor);
    // regular expression specific to the cmd
    regex matchStr;
    // sub match to compare against
    int compareMatch = 0;
    // string composed of the sub match chars
    string valStr = "";
    // string composed of the primary match
    string resultsStr = "";

    if (numCharsRead > 0) {
        cleanupUnprintables(buf, BUF_SIZE);
        
        DLOG(("NCAR_TRH::checkCmdRepsonse(): Number of chars read - %i", numCharsRead));
        DLOG(("NCAR_TRH::checkCmdRepsonse(): chars read - %s", buf));

		// get the matching regex
		switch (cmd) {
            case ENTER_EEPROM_MENU_CMD:
                matchStr = ENTER_EEPROM_MENU_CMD_RESP;
                break;

            case DATA_RATE_CMD:
            case FAN_DUTY_CYCLE_CMD:
            case FAN_MIN_RPM_CMD:
                matchStr = INT_CMD_RESPONSE;
                compareMatch = 1;
                break;

            case FW_VERSION_CMD:
            case TEMP_CAL_0_CMD:
            case TEMP_CAL_1_CMD:
            case TEMP_CAL_2_CMD:
            case HUMD_CAL_0_CMD:
            case HUMD_CAL_1_CMD:
            case HUMD_CAL_2_CMD:
            case HUMD_CAL_3_CMD:
            case HUMD_CAL_4_CMD:
                matchStr = FLOAT_CMD_RESPONSE;
                compareMatch = 1;
                break;

            case EXIT_EEPROM_MENU_CMD:
			    responseOK = handleEepromExit(buf+1, BUF_SIZE);
			    checkMatch = false;
                break;

            case SHOW_SETTINGS_CMD:
                responseOK = captureEepromMetaData(buf);
                checkMatch = false;
                break;

			default:
				break;
		}

		if (checkMatch) {
		    responseOK = _checkSensorCmdResponse(cmd, arg, matchStr, compareMatch, buf);
		}

        if (cmd != EXIT_EEPROM_MENU_CMD) {
            drainResponse();
        }
    }
	return responseOK;
}

bool 
NCAR_TRH::
_checkSensorCmdResponse(TRH_SENSOR_COMMANDS cmd, SensorCmdArg arg, 
                        const regex& matchStr, int matchGroup, 
                        const char* buf)
{
    bool responseOK = false;
    // regular expression specific to the cmd
    // sub match to compare against
    // string composed of the sub match chars
    string valStr = "";
    // string composed of the primary match
    string resultsStr = "";

//    DLOG(("NCAR_TRH::_checkSensorCmdResponse(): matching: ") << matchStr);
    cmatch results;
    bool regexFound = regex_search(buf, results, matchStr);
    if (regexFound && results[0].matched) {
        resultsStr = results.str(0);
        if (!arg.argIsNull && matchGroup > 0) {
            if (results[matchGroup].matched) {
                valStr = results.str(matchGroup);

                if (arg.argIsString) {
                    responseOK = (valStr == arg.strArg);
                }
                else {
                    std::ostringstream convertStream;
                    convertStream << arg.intArg;
                    responseOK = (valStr == convertStream.str());
                }
            }
            else {
                DLOG(("NCAR_TRH::_checkCmdResponse(): Didn't find matches to argument as expected."));
            }
        }
        else {
            responseOK = true;
        }
    }
    else {
        DLOG(("NCAR_TRH::_checkCmdResponse(): Didn't find overall match to string as expected."));
    }

    string cmdStr = cmdTable[cmd];
    int idx = cmdStr.find('\r');
    cmdStr[idx] = 0;

    if (arg.argIsNull) {
            DLOG(("NCAR_TRH::_checkCmdResponse(): Results of checking command w/NULL argument"));
            DLOG(("Overall match: ") << resultsStr);
            DLOG(("NCAR_TRH::_checkCmdResponse(): cmd: %s %s", cmdStr.c_str(), (responseOK ? "SUCCEEDED" : "FAILED")));
        } else if (arg.argIsString) {
            DLOG(("NCAR_TRH::_checkCmdResponse(): Results of checking command w/string argument"));
            DLOG(("Overall match: ") << resultsStr);
            DLOG(("NCAR_TRH::_checkCmdResponse(): cmd: %s: %s. expected: %s => saw: %s", 
                  cmdStr.c_str(), (responseOK ? "SUCCEEDED" : "FAILED"), arg.strArg.c_str(), 
                  valStr.c_str()));
        }
        else {
            DLOG(("NCAR_TRH::_checkCmdResponse(): Results of checking command w/integer argument"));
            DLOG(("Overall match: ") << resultsStr);
            DLOG(("NCAR_TRH::_checkCmdResponse(): cmd: %s: %s. expected: %i => saw: %s", 
                  cmdStr.c_str(), (responseOK ? "SUCCEEDED" : "FAILED"), arg.intArg, valStr.c_str()));
    }

    return responseOK;
}

bool 
NCAR_TRH::
sendAndCheckSensorCmd(TRH_SENSOR_COMMANDS cmd, SensorCmdArg arg)
{
    sendSensorCmd(cmd, arg);
    return checkCmdResponse(cmd, arg);
}

void 
NCAR_TRH::
sendScienceParameters() {
    bool desiredIsDefault = true;
    _scienceParametersOk = true;

    DLOG(("Check for whether the desired science parameters are the same as the default"));
    for (int i=0; i< NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if ((_desiredScienceParameters[i].cmd != DEFAULT_SCIENCE_PARAMETERS[i].cmd)
            || (_desiredScienceParameters[i].arg != DEFAULT_SCIENCE_PARAMETERS[i].arg)) {
            desiredIsDefault = false;
            break;
        }
    }

    if (desiredIsDefault) NLOG(("Base class did not modify the default science parameters for this TRH sensor"));
    else NLOG(("Base class modified the default science parameters for this TRH"));

    DLOG(("Sending science parameters"));
    for (int j=0; j<NUM_DEFAULT_SCIENCE_PARAMETERS && _scienceParametersOk; ++j) {
            _scienceParametersOk = _scienceParametersOk 
                                   && sendAndCheckSensorCmd(static_cast<TRH_SENSOR_COMMANDS>(_desiredScienceParameters[j].cmd), 
                                                            _desiredScienceParameters[j].arg);
    }
}

bool 
NCAR_TRH::
handleEepromExit(const char* buf, const int /* bufSize */)
{
    bool success = false;
    DLOG(("NCAR_TRH::handleEepromExit(): matching: ") << std::string(buf));
    cmatch results;
    bool regexFound = regex_search(buf, results, EXIT_EEPROM_MENU_CMD_RESP);
    if (regexFound && results[0].matched) {
        DLOG(("NCAR_TRH::handleEepromExit(): Found expected EEPROM menu exit message indicating TRH reset imminent..."));
        success = true;
    }
    else {
        DLOG(("NCAR_TRH::handleEepromExit(): Expected EEPROM menu exit message not found!!"));
    }

    return success;
}
CFG_MODE_STATUS 
NCAR_TRH::
enterConfigMode()
{
    return sendAndCheckSensorCmd(ENTER_EEPROM_MENU_CMD) ? ENTERED : NOT_ENTERED;
}

void 
NCAR_TRH::
updateDesiredScienceParameter(TRH_SENSOR_COMMANDS cmd, int arg) {
    for(int i=0; i<NUM_DEFAULT_SCIENCE_PARAMETERS; ++i) {
        if (cmd == _desiredScienceParameters[i].cmd) {
            _desiredScienceParameters[i].arg.intArg = arg;
            break;
        }
    }
}

void 
NCAR_TRH::
fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException)
{
    // let the base classes have first shot at it, since we only care about an autoconfig child element
    // however, any duplicate items in autoconfig will override any items in the base classes
    SerialSensor::fromDOMElement(node);
    // Pick up common autoconfig tag attributes...
    fromDOMElementAutoConfig(node);

    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) 
            continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();

        if (elname == "autoconfig") {
            DLOG(("NCAR_TRH::fromDOMElement(): autoconfig tag found."));

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
                DLOG(("NCAR_TRH:fromDOMElement(): attribute: ") << aname << " : " << upperAval);

                // start with science parameters, assuming SerialSensor took care of any overrides to 
                // the default port config.
                if (aname == "datarate") {
                    std::istringstream intValue(upperAval);
                    int dataRate = 1;
                    intValue >> dataRate;
                    if (intValue.fail() || !RANGE_CHECK_INC(1, dataRate, 60)) {
                        DLOG(("NCAR_TRH:fromDOMElement(): datarate attribute must be in the range 1-60: ") << upperAval);
                        throw n_u::InvalidParameterException(string("NCAR_TRH:") + getName(),
                                                                    aname, aval);
                    }

                    updateDesiredScienceParameter(DATA_RATE_CMD, dataRate);
                }
            }
        }
    }
}


void
NCAR_TRH::
updateMetaData()
{
    sendAndCheckSensorCmd(SHOW_SETTINGS_CMD);
}

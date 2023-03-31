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

#include <boost/regex.hpp>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/core/CalFile.h>
#include <nidas/core/AsciiSscanf.h>
#include <nidas/util/util.h>

#include <limits>

using boost::regex;
using boost::regex_replace;
using boost::cmatch;

using namespace nidas::dynld::isff;
using std::string;
using std::numeric_limits;
using std::list;
using std::vector;

using namespace nidas::core;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,NCAR_TRH)

/* 
 *  AutoConfig: TRH Commands
 */
enum nidas::dynld::isff::TRH_SENSOR_COMMANDS : unsigned short
{
    NULL_CMD,
    ENTER_EEPROM_MENU_CMD,
    FW_VERSION_CMD,
    SENSOR_ID_CMD,
    FAN_DUTY_CYCLE_CMD,
    FAN_MIN_RPM_CMD,
    EEPROM_INIT_STATE_CMD,
    TEMP_CAL_0_CMD,
    TEMP_CAL_1_CMD,
    TEMP_CAL_2_CMD,
    HUMD_CAL_0_CMD,
    HUMD_CAL_1_CMD,
    HUMD_CAL_2_CMD,
    HUMD_CAL_3_CMD,
    HUMD_CAL_4_CMD,
    CLEAR_EEPROM_CMD,
    DEFAULT_EEPROM_CMD,
    SHOW_CMDS_CMD,
    SHOW_SETTINGS_CMD,
    EXIT_EEPROM_MENU_CMD,
    NUM_SENSOR_CMDS,
};


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
    WordSpec(8, Parity::NONE, 1),
};

// TRH instruments only use RS232
static const int NUM_PORT_TYPES = 1;
static const PortType SENSOR_PORT_TYPES[NUM_PORT_TYPES] = {RS232};

static const PortConfig DEFAULT_PORT_CONFIG(9600, 8, Parity::NONE, 1, RS232);

// default message parameters for the TRH
static MessageConfig defaultMessageConfig(0, "\n", true);

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
    _compute_order()
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

    setManufacturer("NCAR");
    setModel("TRH");
    initCustomMetadata();
    setAutoConfigSupported();
}

NCAR_TRH::~NCAR_TRH()
{
    delete _raw_t_handler;
    delete _raw_rh_handler;
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
static const char* SENSOR_ID_CMD_STR =          "SID";  // set the instrument ID
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
static const char* SHOW_SETTINGS_CMD_STR =      "PAR";  // print out all values which can be assigned
static const char* EXIT_EEPROM_MENU_CMD_STR =   "EXT";  // Exit EEPROM menu and reset

static const char* cmdTable[NUM_SENSOR_CMDS] =
{
    0,
    ENTER_EEPROM_MENU_CMD_STR, 
    FW_VERSION_CMD_STR,
    SENSOR_ID_CMD_STR,  
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
    EXIT_EEPROM_MENU_CMD_STR
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

static regex ENTER_EEPROM_MENU_CMD_RESP("[[:space:]]+EEprom:");
static regex EXIT_EEPROM_MENU_CMD_RESP("Exit EEPROM LOADER - reset will take place");


/**
 * @brief Replace type specifiers with a regex pattern to match them.
 *
 * The specifiers are %f for floats, %d for integers, and %v for version
 * strings.
 *
 * The float pattern should match floats in fixed decimal or scientific notation
 * form, with or without a leading sign, and with or without a decimal point.
 * Not all strings matched by the pattern will be valid floats, but it should
 * match all strings which look like they were supposed to be floats.
 *
 * The integer pattern matches a sequence of digits with or without a leading
 * sign character.
 *
 * The version pattern is for any sequence of numbers with interleaved dots,
 * but it must start with at least one digit.
 */
std::string
replace_specifiers(const std::string& s)
{
    static const regex fprx("%f");
    static const regex intrx("%d");
    static const regex vrx("%v");

    auto interp = regex_replace(s, vrx, "\\\\d+[\\\\d.]*");
    interp = regex_replace(interp, intrx, "[+-]?\\\\d+");
    interp = regex_replace(interp, fprx, "[+-]?\\\\d+\\\\.?\\\\d*[eE]?[+-]?\\\\d*");
    return interp;
}

const std::string codeversion{"codeversion"};
const std::string boardversion{"boardversion"};
const std::string trhid{"trhid"};
const std::string fandutycycle{"fandutycycle"};
const std::string fanminrpm{"fanminrpm"};
const std::string Ta0{"Ta0"};
const std::string Ta1{"Ta1"};
const std::string Ta2{"Ta2"};
const std::string Ha0{"Ha0"};
const std::string Ha1{"Ha1"};
const std::string Ha2{"Ha2"};
const std::string Ha3{"Ha3"};
const std::string Ha4{"Ha4"};


const regex trh_parameters_rx(replace_specifiers(
"TRH Code Version:\\s+(?<"+codeversion+">%v),\\s+"
"Board Version: (?<"+boardversion+">%v)\\s+"
"Sensor ID(?<"+trhid+">%d)\\s+"
"fan PWM duty cycle \\(%\\):\\s+(?<"+fandutycycle+">%f)\\s+"
"fan min RPM: (?<"+fanminrpm+">%f)\\s+"
"SHT85 ID \\S+\\s+"
"calibration coefficients:\\s+"
"Ta0 = (?<"+Ta0+">%f)\\s+"
"Ta1 = (?<"+Ta1+">%f)\\s+"
"Ta2 = (?<"+Ta2+">%f)\\s+"
"Ha0 = (?<"+Ha0+">%f)\\s+"
"Ha1 = (?<"+Ha1+">%f)\\s+"
"Ha2 = (?<"+Ha2+">%f)\\s+"
"Ha3 = (?<"+Ha3+">%f)\\s+"
"Ha4 = (?<"+Ha4+">%f)\\s+"
), boost::regex_constants::ECMAScript);


// Typical TRH output
//
// TRH w/fan
// TRH70 23.35 50.87 0 0 1578 94 0
static regex TRH_OUTPUT(
    "TRH[0-9]+[[:blank:]]+"
    "[0-9]+[.][0-9]+[[:blank:]]+"
    "[0-9]+[.][0-9]+[[:blank:]]+"
    "[0-9]+[[:blank:]]+"
    "[0-9]+[[:blank:]]+"
    "[0-9]+[[:blank:]]+"
    "[0-9]+[[:blank:]]+"
    "[0-9]+");

static regex INT_CMD_RESPONSE("[[:space:]]*([0-9]+)");
static regex FLOAT_CMD_RESPONSE("[[:space:]]*([0-9]+[.][0-9]+)");

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
static const string FAN_DUTY_CYCLE_DESC("Fan Duty Cycle");
static const string FAN_MIN_RPM_DESC("Fan Min RPM");
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
    addMetaDataItem(MetaDataItem(FAN_DUTY_CYCLE_DESC, ""), false);
    addMetaDataItem(MetaDataItem(FAN_MIN_RPM_DESC, ""), false);
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
    DLOG(("NCAR_TRH::captureEepromMetaData(): matching:\n")
         << n_u::addBackslashSequences(buf)
         << "\n...against...\n"
         << trh_parameters_rx);
    cmatch results;
    bool regexFound = regex_search(buf, results, trh_parameters_rx);
    if (! regexFound) {
        DLOG(("NCAR_TRH::captureEepromMetaData(): Didn't find overall match to string as expected."));
        return regexFound;
    }

    setSerialNumber(results[trhid].str());
    setFwVersion(results[codeversion].str());
    updateMetaDataItem(MetaDataItem(FAN_DUTY_CYCLE_DESC, results[fandutycycle].str()), false);
    updateMetaDataItem(MetaDataItem(FAN_MIN_RPM_DESC, results[fanminrpm].str()), false);
    updateMetaDataItem(MetaDataItem(TA0_COEFF_DESC, results[Ta0].str()));
    updateMetaDataItem(MetaDataItem(TA1_COEFF_DESC, results[Ta1].str()));
    updateMetaDataItem(MetaDataItem(TA2_COEFF_DESC, results[Ta2].str()));
    updateMetaDataItem(MetaDataItem(HA0_COEFF_DESC, results[Ha0].str()));
    updateMetaDataItem(MetaDataItem(HA1_COEFF_DESC, results[Ha1].str()));
    updateMetaDataItem(MetaDataItem(HA2_COEFF_DESC, results[Ha2].str()));
    updateMetaDataItem(MetaDataItem(HA3_COEFF_DESC, results[Ha3].str()));
    updateMetaDataItem(MetaDataItem(HA4_COEFF_DESC, results[Ha4].str()));
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

    std::vector<char> respBuf(BUF_SIZE, 0);
    char* buf = &(respBuf[0]);
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

                    // updateDesiredScienceParameter(DATA_RATE_CMD, dataRate);
                }
            }
        }
    }
}


void
NCAR_TRH::
updateMetaData()
{
    // This is called by SerialSensor::doAutoConfig() from within
    // SerialSensor::open(), but after checkResponse() is called.
    // checkResponse() puts the TRH in menu mode, so that is not needed here,
    // but menu mode still needs to be exited.  If there were science parameters
    // to set, then those would be done next and the EXT could happen after
    // that, but there no longer are any science parameters to set. SerialSensor
    // will call exitConfigMode() when it calls sendInitString(), so the
    // implementation of that method in this class should be enough.  But it's
    // not. Some callers like trh_load_cal may only want to update meta data and
    // then resume measurements, so go ahead and explicitly exit menu mode here.
    sendAndCheckSensorCmd(SHOW_SETTINGS_CMD);
    sendAndCheckSensorCmd(EXIT_EEPROM_MENU_CMD);
    // exitConfigMode();
}


void
NCAR_TRH::
exitConfigMode()
{
    // sendAndCheckSensorCmd(EXIT_EEPROM_MENU_CMD);
}


bool
NCAR_TRH::
enterMenuMode()
{
    return enterConfigMode() == ENTERED;
}


// This is the protected method declared by the SerialSensor class, so the
// public entry point is enterMenuMode().
CFG_MODE_STATUS 
NCAR_TRH::
enterConfigMode()
{
    return sendAndCheckSensorCmd(ENTER_EEPROM_MENU_CMD) ? ENTERED : NOT_ENTERED;
}

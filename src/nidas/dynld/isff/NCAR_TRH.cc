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
    SensorCmdData((int)SENSOR_EEPROM_MENU_CMD, SensorCmdArg()),
    SensorCmdData(SENSOR_EEPROM_RATE_CMD, SensorCmdArg(1)),
    SensorCmdData(SENSOR_EEPROM_EXIT_CMD, SensorCmdArg()),
    // do this one last after exiting the EEPROM menu.
    SensorCmdData(SENSOR_SET_OUTPUT_MODE_CMD, SensorCmdArg(BOTH))
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
    _outputModeState(ILLEGAL),
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
    setSerialNumber("See custom metadata");
    setCalDate("See custom metadata");
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

static const char* SENSOR_RESET_CMD_STR = "\x12"; // ctrl-r
static const char* SENSOR_TOGGLE_CAL_OUTPUT_CMD_STR = "Oc"; // calibrated output message
static const char* SENSOR_TOGGLE_RAW_OUTPUT_CMD_STR = "Or"; // raw output message
static const char* SENSOR_ENTER_CAL_MODE_CMD_STR = "Ob";    // cal output messages
static const char* SENSOR_EEPROM_MENU_CMD_STR = "\n";     // ctrl-u
static const char* SENSOR_EEPROM_RATE_CMD_STR = "RAT";      // 1-60 seconds between sample messages
static const char* SENSOR_EEPROM_MENU_EXIT_RESP_CMD_STR = "EXT"; // Exit EEPROM menu and reset

static const char* cmdTable[NUM_SENSOR_CMDS] =
{
    0,
    SENSOR_RESET_CMD_STR,
    SENSOR_TOGGLE_CAL_OUTPUT_CMD_STR,
    SENSOR_TOGGLE_RAW_OUTPUT_CMD_STR,
    SENSOR_ENTER_CAL_MODE_CMD_STR,
    SENSOR_EEPROM_MENU_CMD_STR,
    SENSOR_EEPROM_RATE_CMD_STR,
    SENSOR_EEPROM_MENU_EXIT_RESP_CMD_STR 
};

/* 
 *  AutoConfig: TRH mode/data detection
 * 
 *  Typical reset response:
 * =====================================
 *   Sensor ID29   I2C ADD: 10   data rate: 1 (secs)  fan(0) max current: 80 (ma)
 * Calibration Dates: T - 030317, RH - 032117
 * resolution: 12 bits      1 sec MOTE: off
 * calibration coefficients:
 * Ta0 = -1.709815E+1
 * Ta1 =  2.899750E-2
 * Ta2 = -2.330196E-7
 * Ha0 =  6.774113E-1
 * Ha1 =  6.018195E-1
 * Ha2 = -4.784788E-4
 * Ha3 =  4.768972E-2
 * Ha4 =  1.067404E-3
 * Fa0 =  3.222650E-1
 * ===================================
 * 
 *  NOTE: If fan is not present, then fan([0-9]+) simply becomes fan and Fa0 line is missing
 *        Also, Calibration Dates line may or may not be missing. 
 * 
 *        These anomalies are accounted for in the regex and by adjusting the desired value index
 *        when collecting metadata
 */

                                           //input command format: cmd [value] [[value] [value] ...]
static string EEPROM_MENU_ENTERED_RESP_SPEC("[[:space:]]+UNKNOWN Command![[:space:]]+EEprom:");
static regex EEPROM_MENU_ENTERED_RESP(EEPROM_MENU_ENTERED_RESP_SPEC, std::regex_constants::extended);
static string EEPROM_MENU_EXIT_RESP_SPEC("Exit EEPROM LOADER - reset will take place");
static regex EEPROM_MENU_EXIT_RESP(EEPROM_MENU_EXIT_RESP_SPEC, std::regex_constants::extended);
static string SENSOR_RESET_METADATA_REGEX_SPEC(
    "[[:space:]]+Sensor ID([0-9]+)   I2C ADD: ([0-9]+)   data rate: ([0-9]+) \\(secs\\)  fan(\\(([0-9]+)\\)){0,1} max current: ([0-9]+) \\(ma\\)"
    "([[:space:]]+Calibration Dates: T - ([0-9]+), RH - ([0-9]+))*"
    "[[:space:]]+resolution: ([0-9]+) bits[[:blank:]]+1 sec MOTE: (off|on)" 
    "[[:space:]]+calibration coefficients:"
    "[[:space:]]+Ta0 =[[:blank:]]+(-*[0-9]+[.][0-9]+E[+-][0-9]+)"
    "[[:space:]]+Ta1 =[[:blank:]]+(-*[0-9]+[.][0-9]+E[+-][0-9]+)"
    "[[:space:]]+Ta2 =[[:blank:]]+(-*[0-9]+[.][0-9]+E[+-][0-9]+)"
    "[[:space:]]+Ha0 =[[:blank:]]+(-*[0-9]+[.][0-9]+E[+-][0-9]+)"
    "[[:space:]]+Ha1 =[[:blank:]]+(-*[0-9]+[.][0-9]+E[+-][0-9]+)"
    "[[:space:]]+Ha2 =[[:blank:]]+(-*[0-9]+[.][0-9]+E[+-][0-9]+)"
    "[[:space:]]+Ha3 =[[:blank:]]+(-*[0-9]+[.][0-9]+E[+-][0-9]+)"
    "[[:space:]]+Ha4 =[[:blank:]]+(-*[0-9]+[.][0-9]+E[+-][0-9]+)"
    "([[:space:]]+Fa0 =[[:blank:]]+(-*[0-9]+[.][0-9]+E[+-][0-9]+))*"
);
static regex SENSOR_RESET_METADATA(SENSOR_RESET_METADATA_REGEX_SPEC, std::regex_constants::extended);

// Typical TRH output
//
// TRH w/fan
// TRH70 23.35 50.87 0 0 1578 94 0
// 
// Bare TRH module
// TRH116 23.27 50.47 1584 96 0
static string CAL_OUTPUT_ONLY_ENABLED_SPEC("TRH[0-9]+[[:blank:]]+[0-9]+[.][0-9]+[[:blank:]]+[0-9]+[.][0-9]+([[:blank:]]+[0-9]+[[:blank:]]+[0-9]+){0,1}");
static regex CAL_OUTPUT_ONLY_ENABLED(CAL_OUTPUT_ONLY_ENABLED_SPEC, std::regex_constants::extended);
static string RAW_OUTPUT_ONLY_ENABLED_SPEC("TRH[0-9]+[[:blank:]]+[0-9]+[[:blank:]]+[0-9]+[[:blank:]]+[0-9]+");
static regex RAW_OUTPUT_ONLY_ENABLED(RAW_OUTPUT_ONLY_ENABLED_SPEC, std::regex_constants::extended);
static string CAL_AND_RAW_OUTPUT_ENABLED_SPEC("TRH[0-9]+[[:blank:]]+[0-9]+[.][0-9]+[[:blank:]]+[0-9]+[.][0-9]+([[:blank:]]+[0-9]+[[:blank:]]+[0-9]+){0,1}[[:blank:]]+[0-9]+[[:blank:]]+[0-9]+[[:blank:]]+[0-9]+");
static regex CAL_AND_RAW_OUTPUT_ENABLED(CAL_AND_RAW_OUTPUT_ENABLED_SPEC, std::regex_constants::extended);
static string NEITHER_OUTPUT_ENABLED_SPEC("TRH[0-9]+[[:blank:]]*");
static regex NEITHER_OUTPUT_ENABLED(NEITHER_OUTPUT_ENABLED_SPEC, std::regex_constants::extended);
static const int SENSOR_ID_IDX = 1;
static const int I2C_ADD_IDX   = 2;
static const int DATA_RATE_IDX = 3;
static const int FAN_CURRENT_AVAILABLE = 4;
static const int IFAN_IDX = 5;
static const int IFAN_MAX_IDX = 6;
static const int CAL_DATA_AVAILABLE = 7;
static const int TEMP_CAL_DATE_IDX = 8;
static const int REL_HUM_CAL_DATE_IDX = 9;
static const int ADC_RESOLUTION_IDX = 10;
static const int MOTE_1_SEC_IDX = 11;
static const int TCAL_COEFF_0_IDX = 12;
static const int TCAL_COEFF_1_IDX = 13;
static const int TCAL_COEFF_2_IDX = 14;
static const int HCAL_COEFF_0_IDX = 15;
static const int HCAL_COEFF_1_IDX = 16;
static const int HCAL_COEFF_2_IDX = 17;
static const int HCAL_COEFF_3_IDX = 18;
static const int HCAL_COEFF_4_IDX = 19;
static const int FCAL_COEFF_0_IDX = 20;

static string DATA_RATE_ACCEPTED_RESPONSE_SPEC("WRITE: add:12 = ([0-9]{1,2})");
static regex DATA_RATE_ACCEPTED_RESPONSE(DATA_RATE_ACCEPTED_RESPONSE_SPEC, std::regex_constants::extended);
static string EEPROM_MENU_EXIT_RESP_RESPONSE_SPEC("[[:space:]]+Exit EEPROM LOADER - reset will take place");
static regex EEPROM_MENU_EXIT_RESP_RESPONSE(EEPROM_MENU_EXIT_RESP_RESPONSE_SPEC, std::regex_constants::extended);

void cleanupEmbeddedNulls(char* buf, int buflen)
{
    DLOG(("NCAR_TRH::checkCmdRepsonse(): Changing embedded nulls to spaces..."));
    for (int i=0; i < buflen; ++i) {
        if ((int)buf[i] == 0) {
            buf[i] = ' ';
        }
    }
}

bool 
NCAR_TRH::
checkResponse()
{
    return sendAndCheckSensorCmd(SENSOR_RESET_CMD);
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
		case SENSOR_EEPROM_RATE_CMD:
            argStr << " " << arg.intArg << std::endl;
            break;

        // these do not take arguments...
		case SENSOR_EEPROM_EXIT_CMD:
            argStr << std::endl;
            break;

		case SENSOR_RESET_CMD:
		case SENSOR_EEPROM_MENU_CMD:
		case SENSOR_CAL_MODE_OUTPUT_CMD:
		case SENSOR_TOGGLE_CAL_OUTPUT_CMD:
		case SENSOR_TOGGLE_RAW_OUTPUT_CMD:
        default:
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
static const string SENSOR_ID_DESC("Sensr ID");
static const string I2C_ADDR_DESC("I2C Addr");
static const string DATA_RATE_DESC("Data Rate");
static const string IFAN_DESC("Ifan");
static const string IFAN_MAX_DESC("Ifan(max)");
static const string TEMP_CAL_DATE_DESC("Temp Cal Date");
static const string REL_HUM_CAL_DATE_DESC("Rel. Hum. Cal Date");
static const string ADC_RES_DESC("ADC Resolution");
static const string MOTE_1_SEC_DESC("1 Sec Mote");
static const string TA0_COEFF_DESC("Ta0");
static const string TA1_COEFF_DESC("Ta1");
static const string TA2_COEFF_DESC("Ta2");
static const string HA0_COEFF_DESC("Ha0");
static const string HA1_COEFF_DESC("Ha1");
static const string HA2_COEFF_DESC("Ha2");
static const string HA3_COEFF_DESC("Ha3");
static const string HA4_COEFF_DESC("Ha4");
static const string FA0_COEFF_DESC("Fa0");

void 
NCAR_TRH::
initCustomMetadata()
{
    addMetaDataItem(MetaDataItem(SENSOR_ID_DESC, ""));
    addMetaDataItem(MetaDataItem(I2C_ADDR_DESC, ""));
    addMetaDataItem(MetaDataItem(DATA_RATE_DESC, ""));
    addMetaDataItem(MetaDataItem(IFAN_DESC, ""));
    addMetaDataItem(MetaDataItem(IFAN_MAX_DESC, ""));
    addMetaDataItem(MetaDataItem(TEMP_CAL_DATE_DESC, ""));
    addMetaDataItem(MetaDataItem(REL_HUM_CAL_DATE_DESC, ""));
    addMetaDataItem(MetaDataItem(ADC_RES_DESC, ""));
    addMetaDataItem(MetaDataItem(MOTE_1_SEC_DESC, ""));
    addMetaDataItem(MetaDataItem(TA0_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(TA1_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(TA2_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(HA0_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(HA1_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(HA2_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(HA3_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(HA4_COEFF_DESC, ""));
    addMetaDataItem(MetaDataItem(FA0_COEFF_DESC, ""));
}

bool 
NCAR_TRH::
captureResetMetaData(const char* buf)
{
    // TODO DLOG(("NCAR_TRH::captureResetMetaData(): regex: ") << SENSOR_RESET_METADATA_REGEX_SPEC);
    DLOG(("NCAR_TRH::captureResetMetaData(): matching:") << std::string(buf));
    cmatch results;
    bool regexFound = regex_search(buf, results, SENSOR_RESET_METADATA);
    if (regexFound && results[0].matched) {
        if (results[SENSOR_ID_IDX].matched) {
            updateMetaDataItem(MetaDataItem(SENSOR_ID_DESC, results.str(SENSOR_ID_IDX)));
        }
    
        if (results[I2C_ADD_IDX].matched) {
            updateMetaDataItem(MetaDataItem(I2C_ADDR_DESC, results.str(I2C_ADD_IDX)));
        }
    
        if (results[DATA_RATE_IDX].matched) {
            updateMetaDataItem(MetaDataItem(DATA_RATE_DESC, results.str(DATA_RATE_IDX)));
        }
    
        if (results[IFAN_IDX].matched) {
            updateMetaDataItem(MetaDataItem(IFAN_DESC, results.str(IFAN_IDX)));
        }
    
        if (results[IFAN_MAX_IDX].matched) {
            updateMetaDataItem(MetaDataItem(IFAN_MAX_DESC, results.str(IFAN_MAX_IDX)));
        }
    
        // yes, so collect the data
        if (results[TEMP_CAL_DATE_IDX].matched) {
            updateMetaDataItem(MetaDataItem(TEMP_CAL_DATE_DESC, results.str(TEMP_CAL_DATE_IDX)));
        }
    
        if (results[REL_HUM_CAL_DATE_IDX].matched) {
            updateMetaDataItem(MetaDataItem(REL_HUM_CAL_DATE_DESC, results.str(REL_HUM_CAL_DATE_IDX)));
        }
        if (results[ADC_RESOLUTION_IDX].matched) {
            updateMetaDataItem(MetaDataItem(ADC_RES_DESC, results.str(ADC_RESOLUTION_IDX)));
        }
    
        if (results[MOTE_1_SEC_IDX].matched) {
            updateMetaDataItem(MetaDataItem(MOTE_1_SEC_DESC, results.str(MOTE_1_SEC_IDX)));
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

        if (results[FCAL_COEFF_0_IDX].matched) {
            updateMetaDataItem(MetaDataItem(FA0_COEFF_DESC, results.str(FCAL_COEFF_0_IDX)));
        }
    }
    else {
        DLOG(("NCAR_TRH::captureResetMetaData(): Didn't find overall match to string as expected."));
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
        case SENSOR_RESET_CMD:
            checkForUnprintables = false;
            BUF_SIZE = 355;
            break;
        case SENSOR_EEPROM_EXIT_CMD:
            checkForUnprintables = false;
            BUF_SIZE = 60;
            break;

        case SENSOR_EEPROM_MENU_CMD:
            checkForUnprintables = false;
            BUF_SIZE = 120;
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
        cleanupEmbeddedNulls(buf, BUF_SIZE);
        
        DLOG(("NCAR_TRH::checkCmdRepsonse(): Number of chars read - %i", numCharsRead));
        DLOG(("NCAR_TRH::checkCmdRepsonse(): chars read - %s", buf));

		// get the matching regex
		switch (cmd) {
            case SENSOR_EEPROM_MENU_CMD:
                matchStr = EEPROM_MENU_ENTERED_RESP;
                break;
            case SENSOR_EEPROM_RATE_CMD:
                matchStr = DATA_RATE_ACCEPTED_RESPONSE;
                compareMatch = 1;
                break;
            case SENSOR_RESET_CMD:
			    responseOK = captureResetMetaData(buf+1);
			    checkMatch = false;
                break;
            case SENSOR_EEPROM_EXIT_CMD:
			    responseOK = handleEepromExit(buf+1, BUF_SIZE);
			    checkMatch = false;
                break;

            // Don't check the output results of these commands as this has to be done 
            // while the sensor is delivering data, and will be done by another means.
            case SENSOR_TOGGLE_RAW_OUTPUT_CMD:
            case SENSOR_CAL_MODE_OUTPUT_CMD:
                checkMatch = false;
                responseOK = true;
			default:
				break;
		}

		if (checkMatch) {
		    responseOK = _checkSensorCmdResponse(cmd, arg, matchStr, compareMatch, buf);
		}

        if (cmd != SENSOR_RESET_CMD && cmd != SENSOR_EEPROM_EXIT_CMD) {
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
        if (_desiredScienceParameters[j].cmd == SENSOR_SET_OUTPUT_MODE_CMD) {
            // altho we did this in checking for config mode, something in the science parameters
            // resets this to BOTH. So we have to double check.
            (void)checkOutputModeState();
            _scienceParametersOk = _scienceParametersOk 
                                   && setOutputMode(static_cast<TRH_OUTPUT_MODE_STATE>(_desiredScienceParameters[j].arg.intArg));
        }
        else {
            _scienceParametersOk = _scienceParametersOk 
                                   && sendAndCheckSensorCmd(static_cast<TRH_SENSOR_COMMANDS>(_desiredScienceParameters[j].cmd), 
                                                            _desiredScienceParameters[j].arg);
        }
    }
}

bool 
NCAR_TRH::
handleEepromExit(const char* buf, const int /* bufSize */)
{
    bool success = false;
//    DLOG(("NCAR_TRH::handleEepromExit(): regex: ") << EEPROM_MENU_EXIT_RESP_RESPONSE);
    DLOG(("NCAR_TRH::handleEepromExit(): matching: ") << std::string(buf));
    cmatch results;
    bool regexFound = regex_search(buf, results, EEPROM_MENU_EXIT_RESP_RESPONSE);
    if (regexFound && results[0].matched) {
        DLOG(("NCAR_TRH::handleEepromExit(): Found expected EEPROM menu exit message indicating TRH reset imminent...\n"
              "                              Now get reset metadata"));
        sleep(1);
        success = checkCmdResponse(SENSOR_RESET_CMD, SensorCmdArg());
    }
    else {
        DLOG(("NCAR_TRH::handleEepromExit(): Expected EEPROM menu exit message not found!!"));
    }

    return success;
}
bool 
NCAR_TRH::
setOutputMode(const TRH_OUTPUT_MODE_STATE desiredState)
{
    switch (desiredState) {
        case BOTH:
            if (_outputModeState != BOTH) {
                if (_outputModeState == CAL_ONLY) {
                    sendSensorCmd(SENSOR_TOGGLE_RAW_OUTPUT_CMD);
                }
                else if (_outputModeState == RAW_ONLY) {
                    sendSensorCmd(SENSOR_TOGGLE_CAL_OUTPUT_CMD);
                }
                else if (_outputModeState == NEITHER) {
                    sendSensorCmd(SENSOR_TOGGLE_RAW_OUTPUT_CMD);
                    sendSensorCmd(SENSOR_TOGGLE_CAL_OUTPUT_CMD);
                }
            }
            break;
        
        case CAL_ONLY:
            if (_outputModeState != CAL_ONLY) {
                if (_outputModeState == RAW_ONLY) {
                    sendSensorCmd(SENSOR_TOGGLE_CAL_OUTPUT_CMD);
                    sendSensorCmd(SENSOR_TOGGLE_RAW_OUTPUT_CMD);
                }
                else if (_outputModeState == NEITHER) {
                    sendSensorCmd(SENSOR_TOGGLE_CAL_OUTPUT_CMD);
                }
            }
            break;

        case RAW_ONLY:
            if (_outputModeState != RAW_ONLY) {
                if (_outputModeState == CAL_ONLY) {
                    sendSensorCmd(SENSOR_TOGGLE_CAL_OUTPUT_CMD);
                    sendSensorCmd(SENSOR_TOGGLE_RAW_OUTPUT_CMD);
                }
                else if (_outputModeState == NEITHER) {
                    sendSensorCmd(SENSOR_TOGGLE_RAW_OUTPUT_CMD);
                }
            }
            break;

        case NEITHER:
            if (_outputModeState != NEITHER) {
                if (_outputModeState == CAL_ONLY) {
                    sendSensorCmd(SENSOR_TOGGLE_CAL_OUTPUT_CMD);
                }
                else if (_outputModeState == RAW_ONLY) {
                    sendSensorCmd(SENSOR_TOGGLE_RAW_OUTPUT_CMD);
                }
                else {
                    sendSensorCmd(SENSOR_TOGGLE_CAL_OUTPUT_CMD);
                    sendSensorCmd(SENSOR_TOGGLE_RAW_OUTPUT_CMD);
                }
            }
            break;

        default:
            DLOG(("NCAR_TRH::setOutputMode(): Illegal output mode: ") << desiredState);
            break;
    }

    sleep(2); // wait for it to take effect.

    checkOutputModeState();
    if (_outputModeState != desiredState) {
        DLOG(("NCAR_TRH::setOutputMode(): Failed to set output mode to: ") << outputMode2Str(desiredState));
       return false;
    }

    return true;
}

TRH_OUTPUT_MODE_STATE 
NCAR_TRH::
checkOutputModeState()
{
    TRH_OUTPUT_MODE_STATE status = ILLEGAL;

    // Hijacking this method to capture the current output mode
    static const int BUF_SIZE = 75; // > 2x output line length
    int selectTimeout = 2000;
    bool checkForUnprintables = false;
    int retryTimeoutFactor = 5;

    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);
    int numCharsRead = readEntireResponse(respBuf, BUF_SIZE-1, selectTimeout,
                                          checkForUnprintables, retryTimeoutFactor);

    // string composed of the primary match
    string resultsStr = "";

    if (numCharsRead > 0) {
        cleanupEmbeddedNulls(respBuf, BUF_SIZE);
        char* buf = respBuf;
        DLOG(("NCAR_TRH::checkOutputModeState(): Number of chars read - %i", numCharsRead));
        DLOG(("NCAR_TRH::checkOutputModeState(): chars read - %s", buf));

        cmatch results;
        bool regexFound = regex_search(buf, results, CAL_AND_RAW_OUTPUT_ENABLED);
        if (regexFound && results[0].matched) {
            DLOG(("NCAR_TRH::checkOutputModeState(): CAL and RAW output mode is enabled"));
            status = BOTH;
        }
        else {
            regexFound = regex_search(buf, results, CAL_OUTPUT_ONLY_ENABLED);
            if (regexFound && results[0].matched) {
            DLOG(("NCAR_TRH::checkOutputModeState(): CAL output mode only is enabled"));
                status = CAL_ONLY;
            }
            else {
                regexFound = regex_search(buf, results, RAW_OUTPUT_ONLY_ENABLED);
                if (regexFound && results[0].matched) {
                    DLOG(("NCAR_TRH::checkOutputModeState(): RAW output mode only is enabled"));
                    status = RAW_ONLY;
                }
                else {
                    regexFound = regex_search(buf, results, NEITHER_OUTPUT_ENABLED);
                    if (regexFound && results[0].matched) {
                        DLOG(("NCAR_TRH::checkOutputModeState(): Neither CAL nor RAW output mode is enabled"));
                        status = NEITHER;
                    }
                    else {
                        DLOG(("NCAR_TRH::checkOutputModeState(): No regex search succeeded."));
                    }
                }
            }
        }
    }

    _outputModeState = status;

    return _outputModeState;
}

CFG_MODE_STATUS 
NCAR_TRH::
enterConfigMode()
{
    return checkOutputModeState() != ILLEGAL ? ENTERED : NOT_ENTERED;
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
                if (aname == "output_mode") {
                    if (upperAval == "NEITHER") {
                        updateDesiredScienceParameter(SENSOR_SET_OUTPUT_MODE_CMD, NEITHER);
                    }
                    else if (upperAval == "CAL") {
                        updateDesiredScienceParameter(SENSOR_SET_OUTPUT_MODE_CMD, CAL_ONLY);
                    }
                    else if (upperAval == "RAW") {
                        updateDesiredScienceParameter(SENSOR_SET_OUTPUT_MODE_CMD, RAW_ONLY);
                    }
                    else if (upperAval == "BOTH") {
                        updateDesiredScienceParameter(SENSOR_SET_OUTPUT_MODE_CMD, BOTH);
                    }
                    else
                        throw n_u::InvalidParameterException(
                            string("NCAR_TRH:") + getName(), aname, aval);
                }
                else if (aname == "datarate") {
                    std::istringstream intValue(upperAval);
                    int dataRate = 1;
                    intValue >> dataRate;
                    if (intValue.fail() || !RANGE_CHECK_INC(1, dataRate, 60)) {
                        DLOG(("NCAR_TRH:fromDOMElement(): datarate attribute must be in the range 1-60: ") << upperAval);
                        throw n_u::InvalidParameterException(string("NCAR_TRH:") + getName(),
                                                                    aname, aval);
                    }

                    updateDesiredScienceParameter(SENSOR_EEPROM_RATE_CMD, dataRate);
                }
            }
        }
    }
}

string 
NCAR_TRH::
outputMode2Str(const TRH_OUTPUT_MODE_STATE state)
{
    static const char* modeStrTable[] =
    {
        "NEITHER",
        "CAL_ONLY",
        "RAW_ONLY",
        "BOTH",
        "ILLEGAL",
    };

    return string(modeStrTable[state]);
}

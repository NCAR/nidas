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
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/core/AsciiSscanf.h>

#include <sstream>
#include <limits>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,PTB210)

namespace nidas { namespace dynld { namespace isff {

const char* PTB210::DEFAULT_PORT_SERIAL_WORD_CONFIG = "";
int         PTB210::DEFAULT_PORT_SERIAL_BAUD_CONFIG = 9600;
PORT_TYPES  PTB210::DEFAULT_PORT_SERIAL_MODE_CONFIG = RS485_HALF;

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
const char* PTB210::SENSOR_GET_CONFIG_CMD_STR = ".?\r\n";

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
    SENSOR_INC_UNIT_CMD_STR,
    SENSOR_CORRECTION_ON_CMD_STR,
    SENSOR_CORRECTION_OFF_CMD_STR,
    // No way to set the calibration points, so no need to set the Cal date.
    // SENSOR_SET_CAL_DATE_CMD_STR,
    SENSOR_TERM_ON_CMD_STR,
    SENSOR_TERM_OFF_CMD_STR,
};

// NOTE: list sensor bauds from highest to lowest as the higher 
//       ones are the most likely
const int PTB210::SENSOR_BAUDS[NUM_SENSOR_BAUDS] = {19200, 9600, 4800, 2400, 1200};

PTB210::PTB210()
{
}

PTB210::~PTB210()
{
}

void PTB210::sendSensorCmd(PTB_COMMANDS cmd, void* arg)
{
    switch (cmd) {
        case DEFAULT_SENSOR_INIT_CMD:
        case SENSOR_RESET_CMD:
        case SENSOR_SERIAL_BAUD_CMD:
        case SENSOR_SERIAL_EVEN_WORD_CMD:
        case SENSOR_SERIAL_ODD_WORD_CMD:
        case SENSOR_SERIAL_NO_WORD_CMD:
        case SENSOR_MEAS_RATE_CMD:
        case SENSOR_NUM_SAMP_AVG_CMD:
        case SENSOR_PRESS_MIN_CMD:
        case SENSOR_PRESS_MAX_CMD:
        case SENSOR_SINGLE_SAMP_CMD:
        case SENSOR_START_CONT_SAMP_CMD:
        case SENSOR_STOP_CONT_SAMP_CMD:
        case SENSOR_POWER_DOWN_CMD:
        case SENSOR_POWER_UP_CMD:
        case SENSOR_SAMP_UNIT_CMD:
        case SENSOR_INC_UNIT_CMD:
        case SENSOR_CORRECTION_ON_CMD:
        case SENSOR_CORRECTION_OFF_CMD:
        // No way to set the calibration points, so no need to set the Cal date.
        // case SENSOR_SET_CAL_DATE_CMD:
        case SENSOR_TERM_ON_CMD:
        case SENSOR_TERM_OFF_CMD:
            break;

        default:
            throw;
            break;
    }
}


// TODO - munge this for PTB210 or nuke it altogether
void PTB210::validate() throw(n_u::InvalidParameterException)
{
    nidas::core::SerialSensor::validate();

    list<SampleTag*>& tags = getSampleTags();
    list<SampleTag*>::const_iterator ti = tags.begin();
    for ( ; ti != tags.end(); ++ti) {
        SampleTag* tag = *ti;
        const vector<Variable*>& vars = tag->getVariables();
        vector<Variable*>::const_iterator vi = vars.begin();
        for (unsigned int i = 0; vi != vars.end(); ++vi,i++) {
            Variable* var = *vi;
            if (var->getName().substr(0,4) == "P") {
                _PIndex = i;
                _minP = var->getMinValue();
                _maxP = var->getMaxValue();
                var->setMinValue(-numeric_limits<float>::max());
                var->setMaxValue(numeric_limits<float>::max());
            }
        }
    }
}

// pretty sure I don't need this...
bool PTB210::process(const Sample* samp, std::list<const Sample*>& results) throw()
{

    nidas::core::SerialSensor::process(samp,results);

    if (results.empty()) return false;

    const Sample* csamp = results.front();
    unsigned int slen = csamp->getDataLength();

    if (slen > _PIndex) {
        float ifan = csamp->getDataValue(_PIndex);

        // flag T,RH if Ifan is less than _minIfan
        if (ifan < _minP || ifan > _maxP) {

            SampleT<float>* news = getSample<float>(slen);
            news->setTimeTag(csamp->getTimeTag());
            news->setId(csamp->getId());

            float* nfptr = news->getDataPtr();

            unsigned int i;
            // flag any values other then Ifan
            for (i = 0; i < slen; i++) {
                if (i != _PIndex) nfptr[i] = floatNAN;
                else nfptr[i] = csamp->getDataValue(i);
            }

            csamp->freeReference();

            results.front() = news;
        }
    }
    return true;
}

}}} //namespace nidas { namespace dynld { namespace isff {

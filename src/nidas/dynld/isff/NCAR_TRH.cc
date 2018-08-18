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
#include <nidas/core/AsciiSscanf.h>

#include <sstream>
#include <limits>

using namespace nidas::dynld::isff;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,NCAR_TRH)

NCAR_TRH::NCAR_TRH():
    _ifanIndex(99),
    _minIfan(-numeric_limits<float>::max()),
    _maxIfan(numeric_limits<float>::max()),
    _Ta(3),
    _Ha(5)
{
}

NCAR_TRH::~NCAR_TRH()
{
}

void NCAR_TRH::validate() throw(n_u::InvalidParameterException)
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
            if (var->getName().substr(0,4) == "Ifan") {
                _ifanIndex = i;
                _minIfan = var->getMinValue();
                _maxIfan = var->getMaxValue();
                var->setMinValue(-numeric_limits<float>::max());
                var->setMaxValue(numeric_limits<float>::max());
            }
        }
    }
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

    results.push_back(outs);

    // Apply any variable conversions.  This replaces the call to
    // applyConversions() in the base class process() method, because we
    // need to detect and handle raw conversions.
    float* fp = outs->getDataPtr();
    const vector<Variable*>& vars = stag->getVariables();
    for (unsigned int iv = 0; iv < vars.size(); iv++)
    {
        Variable* var = vars[iv];
        fp = var->convert(outs->getTimeTag(), fp);
    }

    ifanFilter(results);
    return true;
}

void
NCAR_TRH::
ifanFilter(std::list<const Sample*>& results)
{
    const Sample* csamp = results.front();
    unsigned int slen = csamp->getDataLength();

    if (slen > _ifanIndex)
    {
        float ifan = csamp->getDataValue(_ifanIndex);

        // flag T,RH if Ifan is less than _minIfan
        if (ifan < _minIfan || ifan > _maxIfan) {

            SampleT<float>* news = getSample<float>(slen);
            news->setTimeTag(csamp->getTimeTag());
            news->setId(csamp->getId());

            float* nfptr = news->getDataPtr();

            unsigned int i;
            // flag any values other then Ifan
            for (i = 0; i < slen; i++) {
                if (i != _ifanIndex) nfptr[i] = floatNAN;
                else nfptr[i] = csamp->getDataValue(i);
            }

            csamp->freeReference();

            results.front() = news;
        }
    }
}


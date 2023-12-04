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
#include <functional>

using namespace nidas::dynld::isff;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,NCAR_TRH)


NCAR_TRH::NCAR_TRH():
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
    using namespace std::placeholders;
    _raw_t_handler = makeCalFileHandler
        (std::bind(&NCAR_TRH::handleRawT, this, _1));
    _raw_rh_handler = makeCalFileHandler
        (std::bind(&NCAR_TRH::handleRawRH, this, _1));
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


void NCAR_TRH::validate()
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
process(const Sample* samp, std::list<const Sample*>& results)
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


// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#include "Wind2D.h"
#include <nidas/core/SampleTag.h>
#include <nidas/core/Sample.h>
#include <nidas/core/Variable.h>
#include <nidas/core/AsciiSscanf.h>
#include <nidas/util/Logger.h>
#include <nidas/util/util.h>

#include <sstream>
#include <cmath>
#include <map>

using namespace nidas::dynld::isff;
using namespace nidas::dynld;
using namespace nidas::core;

using std::vector;
using std::ostringstream;
using std::string;
using std::map;
using std::list;

using nidas::util::derive_spd_dir_from_uv;
using nidas::util::derive_uv_from_spd_dir;
using nidas::util::InvalidParameterException;
using nidas::util::LogContext;

NIDAS_CREATOR_FUNCTION_NS(isff,Wind2D)

Wind2D::Wind2D():
    _speedName("Spd"),_dirName("Dir"),_uName("U"),_vName("V"),
    _speedIndex(-1),_dirIndex(-1),_uIndex(-1),_vIndex(-1),
    _outlen(0),_wind_sample_id(0),
    _dirConverter(0),_speedConverter(0),
    _orienter()
{
}

Wind2D::~Wind2D()
{
}

namespace
{
    bool
    match_prefix(const std::string& vname, const std::string& prefix)
    {
        return vname.find(prefix) == 0;
    }
}


void Wind2D::validate()
{
    SerialSensor::validate();

    const std::list<SampleTag*>& tags = getSampleTags();

    if (tags.size() > 1)
    {
        DLOG(("")
             << "Wind2D(" << getName() << "): "
             << "only the first sample will be processed for wind variables.");
    }
    std::list<SampleTag*>::const_iterator ti = tags.begin();

    for ( ; ti != tags.end() && !_wind_sample_id; ++ti) {
        SampleTag* stag = *ti;

        // For now, assume that only the first sample has wind variables to
        // be handled.
        if (!_wind_sample_id)
        {
            _wind_sample_id = stag->getId();
        }

        // Check the variable names to determine which is wind speed,
        // direction, u or v.  Rather than impose a policy on variable names,
        // the variable names are matched against prefixes.  The prefixes
        // default to U, V, Dir, and Spd, but can be changed with parameters.
        const vector<Variable*>& vars = stag->getVariables();
        vector<Variable*>::const_iterator vi = vars.begin();
        for (int i = 0; vi != vars.end(); i++,++vi) {
            Variable* var = *vi;
            const string& vname = var->getName();
            if (match_prefix(vname, getUName()))
            {
                _uIndex = i;
            }
            else if (match_prefix(vname, getVName()))
            {
                _vIndex = i;
            }
            else if (match_prefix(vname, getSpeedName()))
            {
                _speedIndex = i;
                _speedConverter = var->getConverter();
                DLOG(("spd converter is ") << (_speedConverter ? _speedConverter->toString() : "null"));
            }
            else if (match_prefix(vname, getDirName()))
            {
                _dirIndex = i;
                _dirConverter = var->getConverter();
                DLOG(("dir converter is ") << (_dirConverter ? _dirConverter->toString() : "null"));
            }
        }
        if (_speedIndex < 0 || _dirIndex < 0)
            throw InvalidParameterException(getName() +
              " Wind2D cannot find speed or direction variables");

        _outlen = stag->getVariables().size();
    }
}

void Wind2D::validateSscanfs()
{
    const std::list<AsciiSscanf*>& sscanfers = getScanfers();
    std::list<AsciiSscanf*>::const_iterator si = sscanfers.begin();

    for ( ; si != sscanfers.end(); ++si) {
        AsciiSscanf* sscanf = *si;
        const SampleTag* tag = sscanf->getSampleTag();

        // If this is not the scanner for the wind sample, skip it.
        if (tag->getId() != _wind_sample_id)
        {
            continue;
        }
        DLOG(("Wind2D(") << getName()
             << "): validating sscanfs for wind sample "
             << GET_DSM_ID(_wind_sample_id) << ','
             << GET_SHORT_ID(_wind_sample_id));
        int nexpected = tag->getVariables().size();
        int nscanned = sscanf->getNumberOfFields();

        if (_uIndex >= nscanned) nexpected--;
        if (_vIndex >= nscanned) nexpected--;
        if (_speedIndex >= nscanned) nexpected--;
        if (_dirIndex >= nscanned) nexpected--;

        if (nscanned != nexpected) {
            ostringstream ost;
            ost << "number of scanf fields (" << nscanned <<
                ") is not the number expected (" << nexpected << ')';
            throw InvalidParameterException(getName(),"scanfFormat",ost.str());
        }
    }
}

bool Wind2D::process(const Sample* samp,
    std::list<const Sample*>& results) throw()
{
    // The idea here is to duplicate SerialSensor::process(samp,results),
    // except don't call applyConversions(), since we might first need to
    // orient the sonic before adjusting wind direction with a variable
    // conversion.
    //
    // Since orientation happens in u,v space, u,v are resolved first, either
    // as parsed from the message or by deriving them from the parsed spd and
    // direction.  Conversions (such as correcting direction by applying an
    // offset) can be applied to any of the sample variables, but u, v are
    // always derived from spd, dir after conversions, on the assumption that
    // only direction offsets will ever be applied.  In other words, it is
    // impossible to "calibrate" u and v, even if they are parsed directly
    // from the raw message and used to derive speed and direction.

    // \/ \/ \/ \/ \/ \/ Copied from CharacterSensor::process()
    SampleTag* stag = 0;
    SampleT<float>* outs = searchSampleScanners(samp, &stag);
    if (!outs)
    {
        return false;
    }

    // Apply any time tag adjustments.
    adjustTimeTag(stag, outs);

    // conversions skipped here and deferred until after orientation

    results.push_back(outs);
    // /\ /\ /\ /\ /\ /\ Copied from CharacterSensor::process()

    // This appears to require that all four variables are always defined
    // in a sample tag, except I'm not sure why that's necessary.
    if (results.size() != 1 || _speedIndex < 0 ||
        _dirIndex < 0 || _uIndex < 0 || _vIndex < 0)
    {
        return true;
    }

    // result from base class parsing of ASCII, without any corrections from
    // any cal files or converters
    const Sample* csamp = results.front();
    unsigned int slen = csamp->getDataLength();
    bool iswindsample = (csamp->getId() == _wind_sample_id);

    static LogContext lp(LOG_DEBUG);
    if (lp.active())
    {
        lp.log()
            << "Wind2d(" << getName() << "): "
            << (iswindsample ? "" : "NOT ")
            << "processing wind for sample id="
            << GET_DSM_ID(csamp->getId()) << ','
            << GET_SHORT_ID(csamp->getId());
    }
    if (!iswindsample)
    {
        return true;
    }

    // Get the measurement into U and V first, either as parsed directly from
    // the message or by deriving from the parsed speed and direction.
    float u = floatNAN;
    float v = floatNAN;
    float spd = floatNAN;
    float dir = floatNAN;

    if (_speedIndex < _uIndex)
    {
        // speed and dir parsed from sample, so nothing to do if not there.
        if ((signed) slen <= _speedIndex) return true;
        if ((signed) slen <= _dirIndex) return true;

        // At one point this code took U and V as they were, if parsed, but
        // there's really no point in keeping track of that.  One of the pairs
        // has to be derived from the other eventually.  In this case, derive
        // u and v from spd and dir.
        spd = csamp->getDataValue(_speedIndex);
        dir = csamp->getDataValue(_dirIndex);
        derive_uv_from_spd_dir(u, v, spd, dir);
    }
    else
    {
        // u and v parsed from sample, nothing to do if not there
        if ((signed) slen <= _uIndex) return true;
        if ((signed) slen <= _vIndex) return true;

        // take u, v as parsed.  no point computing spd, dir until
        // after orientation applied.
        u = csamp->getDataValue(_uIndex);
        v = csamp->getDataValue(_vIndex);
    }

    // Now apply any orientation settings, and re-derive speed and direction,
    // in case they were not derived above or else u, v were adjusted.
    _orienter.applyOrientation2D(&u, &v);
    derive_spd_dir_from_uv(spd, dir, u, v);

    // We now have u, v, dir, spd in "instrument-oriented-space", so put the
    // values in a new sample and apply conversions.
    SampleT<float>* news = getSample<float>(_outlen);
    news->setTimeTag(csamp->getTimeTag());
    news->setId(csamp->getId());

    float* nfptr = news->getDataPtr();

    unsigned int i;
    for (i = 0; i < slen && i < _outlen; i++)
        nfptr[i] = csamp->getDataValue(i);
    for ( ; i < _outlen; i++)
        nfptr[i] = floatNAN;

    nfptr[_dirIndex] = dir;
    nfptr[_speedIndex] = spd;
    nfptr[_uIndex] = u;
    nfptr[_vIndex] = v;

    csamp->freeReference();
    results.front() = news;

    // this is where direction might be corrected by a calibration file
    applyConversions(stag, news);

    // ...so update u and v to make sure they're consistent.
    dir = nfptr[_dirIndex];
    spd = nfptr[_speedIndex];
    derive_uv_from_spd_dir(u, v, spd, dir);

    // and update the values in the sample one last time
    nfptr[_dirIndex] = dir;
    nfptr[_speedIndex] = spd;
    nfptr[_uIndex] = u;
    nfptr[_vIndex] = v;

    return true;
}

void Wind2D::fromDOMElement(const xercesc::DOMElement* node)
{
    DLOG(("Wind2D::fromDOMElement()"));
    SerialSensor::fromDOMElement(node);

    typedef void (Wind2D::*set_param_t)(const string&);
    map<string, set_param_t> paramset =
    {
        { "speed", &Wind2D::setSpeedName },
        { "dir", &Wind2D::setDirName },
        { "u", &Wind2D::setUName },
        { "v", &Wind2D::setVName },
    };

    // Handle our specific parameters.
    const list<const Parameter*>& params = getParameters();

    for (auto pi = params.begin(); pi != params.end(); ++pi)
    {
        const Parameter* param = *pi;
        const std::string& pname = param->getName();

        if (!_orienter.handleParameter(param, getName()) &&
            paramset.find(pname) != paramset.end())
        {
            if (param->getLength() != 1)
            {
                throw InvalidParameterException(getName(),
                    "parameter", string("bad length for ") + pname);
            }
            // invoke setXXX member function
            (this->*paramset[pname])(param->getStringValue(0));
        }
    }
}

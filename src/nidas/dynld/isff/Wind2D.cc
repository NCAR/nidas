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

#include <sstream>

using namespace nidas::dynld::isff;
using namespace nidas::dynld;
using namespace nidas::core;

using std::vector;
using std::ostringstream;
using std::string;

namespace n_u = nidas::util;

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

void Wind2D::validate() throw(n_u::InvalidParameterException)
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

        // check the variable names to determine which
        // is wind speed, direction, u or v.
        // Rather than impose a policy on variable names,
        // then names can be specified by the user.
        const vector<Variable*>& vars = stag->getVariables();
        vector<Variable*>::const_iterator vi = vars.begin();
        for (int i = 0; vi != vars.end(); i++,++vi) {
            Variable* var = *vi;
            const string& vname = var->getName();
            if (vname.length() >= getUName().length() &&
                    vname.substr(0,getUName().length()) == getUName())
                _uIndex = i;
            else if (vname.length() >= getVName().length() &&
                    vname.substr(0,getVName().length()) == getVName())
                _vIndex = i;
            else if (vname.length() >= getSpeedName().length() &&
                    vname.substr(0,getSpeedName().length()) == getSpeedName()) {
                _speedIndex = i;
                _speedConverter = var->getConverter();
            }
            else if (vname.length() >= getDirName().length() &&
                    vname.substr(0,getDirName().length()) == getDirName()) {
                _dirIndex = i;
                _dirConverter = var->getConverter();
            }
        }
        if (_speedIndex < 0 || _dirIndex < 0)
            throw n_u::InvalidParameterException(getName() +
              " Wind2D cannot find speed or direction variables");

        _outlen = stag->getVariables().size();
    }
}

void Wind2D::validateSscanfs() throw(n_u::InvalidParameterException)
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
            throw n_u::InvalidParameterException(getName(),"scanfFormat",ost.str());
        }
    }
}

bool Wind2D::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{
    // The idea here was to duplicate SerialSensor::process(samp,results),
    // except don't call applyConversions(), since we might first need to
    // orient the sonic before applying any adjustments to wind direction.
    // However, delaying the call to applyConversions() until after the
    // (u,v)<->(spd,dir) directions causes occasional spikes in the data, perhaps due to some memory error somewhere caused by
    // a broken assumption about sample data makeup.
    //
    // If orientations of 2D sonics is ever implemented here, then this
    // will need to be sorted out.  Probably the derivations need to be
    // broken up, so that conversions are applied to the sampled pair
    // before deriving the derived pair.  Or, perhaps applyConversions()
    // needs to be careful not to convert variables which are actually
    // derived.  Or maybe it would still be ok to insert the orientation
    // call before applyConversions(), so long as applyConversions() comes
    // before derivations.

    // \/ \/ \/ \/ \/ \/ Copied from CharacterSensor::process()
    SampleTag* stag = 0;
    SampleT<float>* outs = searchSampleScanners(samp, &stag);
    if (!outs)
    {
        return false;
    }

    // Apply any time tag adjustments.
    adjustTimeTag(stag, outs);

    // Apply any variable conversions.  Note this has to happen after the
    // time is adjusted, since the calibrations are keyed by time.
    applyConversions(stag, outs);

    results.push_back(outs);
    // /\ /\ /\ /\ /\ /\ Copied from CharacterSensor::process()

    // This appears to require that all four variables are always defined
    // in a sample tag, except I'm not sure why that's necessary.
    if (results.size() != 1 || _speedIndex < 0 ||
        _dirIndex < 0 || _uIndex < 0 || _vIndex < 0)
    {
    	return true;
    }

    // result from base class parsing of ASCII, and correction of any cal file
    const Sample* csamp = results.front();
    unsigned int slen = csamp->getDataLength();
    bool iswindsample = (csamp->getId() == _wind_sample_id);

    static n_u::LogContext lp(LOG_DEBUG);
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

    if (_speedIndex < _uIndex) {   
        // speed and dir parsed from sample, u and v derived

        // if speed and dir not in this sample, nothing left to do.
        if ((signed) slen <= _speedIndex) return true;
        if ((signed) slen <= _dirIndex) return true;

        // If U or V have been parsed, don't derive them.
        if ((signed) slen > _vIndex &&
            (!std::isnan(csamp->getDataValue(_uIndex)) ||
             !std::isnan(csamp->getDataValue(_vIndex)))) return true;

        float spd = csamp->getDataValue(_speedIndex);
        // dir has had cal file applied
        float dir = fmod(csamp->getDataValue(_dirIndex),360.0);
        if (dir < 0.0) dir += 360.0;

        // derive U,V from Spd,Dir
        float u = -spd * ::sin(dir * M_PI / 180.0);
        float v = -spd * ::cos(dir * M_PI / 180.0);

        SampleT<float>* news = getSample<float>(_outlen);
        news->setTimeTag(csamp->getTimeTag());
        news->setId(csamp->getId());

        float* nfptr = news->getDataPtr();

        unsigned int i;
        for (i = 0; i < slen && i < _outlen; i++) nfptr[i] = csamp->getDataValue(i);
        for ( ; i < _outlen; i++) nfptr[i] = floatNAN;

        nfptr[_dirIndex] = dir;  // overwrite direction, corrected by mod 360
        nfptr[_uIndex] = u;
        nfptr[_vIndex] = v;

        csamp->freeReference();

        results.front() = news;
    }
    else {
        // u and v parsed from sample, speed and dir derived

        // if u or v not parsed, nothing left to do
        if ((signed) slen <= _uIndex) return true;
        if ((signed) slen <= _vIndex) return true;

        // If dir or speed have been parsed, don't derive them.
        if (((signed)slen > _dirIndex && (signed)slen > _speedIndex) &&
            (!std::isnan(csamp->getDataValue(_speedIndex)) ||
             !std::isnan(csamp->getDataValue(_dirIndex)))) return true;

        float u = csamp->getDataValue(_uIndex);
        float v = csamp->getDataValue(_vIndex);
        float dir = n_u::dirFromUV(u, v);
        float spd = ::sqrt(u*u + v*v);
        bool redoUV = false;
        if (_dirConverter) {
            // correction is in degrees
            dir = _dirConverter->convert(csamp->getTimeTag(),dir);
            redoUV = true;
        }
        if (_speedConverter) {
            spd = _speedConverter->convert(csamp->getTimeTag(),spd);
            redoUV = true;
        }
        if (redoUV) {
            // recompute U,V from Spd,Dir
            u = -spd * ::sin(dir * M_PI / 180.0);
            v = -spd * ::cos(dir * M_PI / 180.0);
        }
        if (dir < 0.0) dir += 360.0;

        SampleT<float>* news = getSample<float>(_outlen);
        news->setTimeTag(csamp->getTimeTag());
        news->setId(csamp->getId());

        float* nfptr = news->getDataPtr();

        unsigned int i;
        for (i = 0; i < slen && i < _outlen; i++) nfptr[i] = csamp->getDataValue(i);
        for ( ; i < _outlen; i++) nfptr[i] = floatNAN;

        nfptr[_dirIndex] = dir;
        nfptr[_speedIndex] = spd;
        nfptr[_uIndex] = u;
        nfptr[_vIndex] = v;

        csamp->freeReference();

        results.front() = news;
    }

    return true;
}

void Wind2D::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    SerialSensor::fromDOMElement(node);

    static struct ParamSet {
        const char* name;	// parameter name
        void (Wind2D::* setFunc)(const string&);
        			// ptr to setXXX member function
				// for setting parameter.
    } paramSet[] = {
	{ "speed",		&Wind2D::setSpeedName },
	{ "dir",		&Wind2D::setDirName },
	{ "u",			&Wind2D::setUName },
	{ "v",			&Wind2D::setVName },
    };

    for (unsigned int i = 0; i < sizeof(paramSet) / sizeof(paramSet[0]); i++) {
	const Parameter* param = getParameter(paramSet[i].name);
	if (!param) continue;
	if (param->getLength() != 1) 
	    throw n_u::InvalidParameterException(getName(),
		"parameter", string("bad length for ") + paramSet[i].name);
	// invoke setXXX member function
        if (_orienter.handleParameter(param, getName()))
        {
	    throw n_u::InvalidParameterException(getName(),
		"parameter", string("Wind2D sensors do not yet support"
                                    " orientation changes: ") + paramSet[i].name);
        }
        else
        {
            (this->*paramSet[i].setFunc)(param->getStringValue(0));
        }
    }
}

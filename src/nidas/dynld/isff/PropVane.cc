// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/isff/PropVane.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>

#include <sstream>

using namespace nidas::dynld::isff;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,PropVane)

PropVane::PropVane():
    _speedName("Spd"),_dirName("Dir"),_uName("U"),_vName("V"),
    _speedIndex(-1),_dirIndex(-1),_uIndex(-1),_vIndex(-1),
    _outlen(0),_dirConverter(0),_speedConverter(0)
{
}

PropVane::~PropVane()
{
}

void PropVane::addSampleTag(SampleTag* stag)
    throw(n_u::InvalidParameterException)
{
    if (getSampleTags().size() > 0)
        throw n_u::InvalidParameterException(getName() +
		" can only create one sample");

    DSMSerialSensor::addSampleTag(stag);

    // check the variable names to determine which
    // is wind speed, direction, u or v.
    // Rather than impose a policy on variable names,
    // then names can be specified by the user.
    VariableIterator vi = stag->getVariableIterator();
    for (int i = 0; vi.hasNext(); i++) {
	const Variable* var = vi.next();
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
	  " PropVane cannot find speed or direction variables");

    _outlen = stag->getVariables().size();
}

bool PropVane::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{

    std::list<const Sample*> vane;
    DSMSerialSensor::process(samp,results);

    if (results.empty()) return false;

    if (results.size() != 1 || _speedIndex < 0 || _dirIndex < 0 || _uIndex < 0 || _vIndex < 0)
    	return true;

    // result from base class parsing of ASCII, and correction of any cal file
    const Sample* csamp = results.front();
    unsigned int slen = csamp->getDataLength();

    if (_speedIndex < _uIndex) {   
        // speed and dir parsed from sample, u and v derived
        if ((signed) slen > _vIndex) return true;  // all values parsed
        if ((signed) slen <= _speedIndex) return true;
        if ((signed) slen <= _dirIndex) return true;

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
        if ((signed) slen > _speedIndex && (signed)slen > _dirIndex) return true;
        if ((signed) slen <= _uIndex) return true;
        if ((signed) slen <= _vIndex) return true;

        float u = csamp->getDataValue(_uIndex);
        float v = csamp->getDataValue(_vIndex);
        float dir = ::atan2(-u,-v) * 180.0 / M_PI;  // convert to degrees 
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

void PropVane::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    DSMSerialSensor::fromDOMElement(node);

    static struct ParamSet {
        const char* name;	// parameter name
        void (PropVane::* setFunc)(const string&);
        			// ptr to setXXX member function
				// for setting parameter.
    } paramSet[] = {
	{ "speed",		&PropVane::setSpeedName },
	{ "dir",		&PropVane::setDirName },
	{ "u",			&PropVane::setUName },
	{ "v",			&PropVane::setVName },
    };

    for (unsigned int i = 0; i < sizeof(paramSet) / sizeof(paramSet[0]); i++) {
	const Parameter* param = getParameter(paramSet[i].name);
	if (!param) continue;
	if (param->getLength() != 1) 
	    throw n_u::InvalidParameterException(getName(),
		"parameter", string("bad length for ") + paramSet[i].name);
	// invoke setXXX member function
	(this->*paramSet[i].setFunc)(param->getStringValue(0));
    }
}

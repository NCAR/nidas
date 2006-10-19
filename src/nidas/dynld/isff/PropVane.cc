/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-03 14:24:50 -0700 (Fri, 03 Feb 2006) $

    $LastChangedRevision: 3262 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/SonicAnemometer.cc $

*/

#include <nidas/dynld/isff/PropVane.h>

#include <sstream>

using namespace nidas::dynld::isff;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,PropVane)

PropVane::PropVane():
	speedName("Spd"),dirName("Dir"),uName("U"),vName("V"),
	speedIndex(-1),dirIndex(-1),uIndex(-1),vIndex(-1)
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
	    uIndex = i;
	else if (vname.length() >= getVName().length() &&
		vname.substr(0,getVName().length()) == getVName())
	    vIndex = i;
	else if (vname.length() >= getSpeedName().length() &&
		vname.substr(0,getSpeedName().length()) == getSpeedName())
	    speedIndex = i;
	else if (vname.length() >= getDirName().length() &&
		vname.substr(0,getDirName().length()) == getDirName())
	    dirIndex = i;
    }
    if (speedIndex < 0 || dirIndex < 0)
	throw n_u::InvalidParameterException(getName() +
	  " PropVane cannot find speed or direction variables");

    outlen = stag->getVariables().size();
}

bool PropVane::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{

    std::list<const Sample*> vane;
    DSMSerialSensor::process(samp,results);

    if (results.size() == 0) return false;

    if (results.size() != 1 || speedIndex < 0 || dirIndex < 0 || uIndex < 0 || vIndex < 0)
    	return true;

    // derive U,V from Spd,Dir
    const SampleT<float>* fsamp = static_cast<const SampleT<float>*>(results.front());

    int slen = (int)fsamp->getDataLength();
    const float* fptr = fsamp->getConstDataPtr();

    if (slen <= speedIndex) return true;
    if (slen <= dirIndex) return true;

    float spd = fptr[speedIndex];
    float dir = fptr[dirIndex];

    float u = -spd * ::sin(dir * M_PI / 180.0);
    float v = -spd * ::cos(dir * M_PI / 180.0);

    SampleT<float>* news = getSample<float>(outlen);
    news->setTimeTag(fsamp->getTimeTag());
    news->setId(fsamp->getId());

    float* nfptr = news->getDataPtr();

    int i;
    for (i = 0; i < slen; i++) nfptr[i] = fptr[i];
    for ( ; i < (int)outlen; i++) nfptr[i] = floatNAN;
    nfptr[uIndex] = u;
    nfptr[vIndex] = v;

    fsamp->freeReference();

    results.front() = news;

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

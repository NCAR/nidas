// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/isff/CS_Krypton.h>
#include <nidas/core/CalFile.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Logger.h>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,CS_Krypton)

CS_Krypton::CS_Krypton():
    _Kw(-0.150),_V0(5000.0),_logV0(::log(_V0)),
    _pathLength(1.3), _bias(0.0),_pathLengthKw(_pathLength * _Kw),
    _calFile(0),_calTime(0)
{
    setUnits("g/m^3");
}

CS_Krypton::CS_Krypton(const CS_Krypton& x):
    VariableConverter(x),
    _Kw(x._Kw),_V0(x._V0),_logV0(::log(_V0)),
    _pathLength(x._pathLength), _bias(x._bias),
    _pathLengthKw(_pathLength * _Kw),
    _calFile(0),_calTime(0)
{
    setUnits(x.getUnits());
}

CS_Krypton* CS_Krypton::clone() const
{
    return new CS_Krypton(*this);
}

CS_Krypton::~CS_Krypton()
{
    delete _calFile;
}

void CS_Krypton::setCalFile(CalFile* val)
{
    _calFile = val;
}

CalFile* CS_Krypton::getCalFile()
{
    return _calFile;
}

std::string CS_Krypton::toString()
{
    return "CS_Krypton::toString() unsupported";
}

void CS_Krypton::fromString(const std::string&) 
    	throw(n_u::InvalidParameterException)
{
    throw n_u::InvalidParameterException(
    	"CS_Krypton::fromString() not supported yet");
}

void CS_Krypton::readCalFile(dsm_time_t t)
{
    if (_calFile) {
        while(t >= _calTime) {
            float d[5];
            try {
                int n = _calFile->readData(d,sizeof d/sizeof(d[0]));
                if (n > 0) setKw(d[0]);
                if (n > 1) setV0(d[1]);
                if (n > 2) setPathLength(d[2]);
                if (n > 3) setBias(d[3]);
                _calTime = _calFile->readTime().toUsecs();
            }
            catch(const n_u::EOFException& e)
            {
                _calTime = LONG_LONG_MAX;
            }
            catch(const n_u::IOException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    _calFile->getCurrentFileName().c_str(),e.what());
                setKw(floatNAN);
                setV0(floatNAN);
                setPathLength(floatNAN);
                setBias(floatNAN);
                _calTime = LONG_LONG_MAX;
            }
            catch(const n_u::ParseException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    _calFile->getCurrentFileName().c_str(),e.what());
                setKw(floatNAN);
                setV0(floatNAN);
                setBias(floatNAN);
                setPathLength(floatNAN);
                _calTime = LONG_LONG_MAX;
            }
        }
    }
}

double CS_Krypton::convert(dsm_time_t t,double volts)
{
    readCalFile(t);

    // convert to millivolts
    volts *= 1000.0;

    if (volts < 0.1) volts = 0.1;

    double h2o = (::log(volts) - _logV0) / _pathLengthKw - _bias;
    if (h2o < 0.0) h2o = 0.0;
    return h2o;
}
    	

void CS_Krypton::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    VariableConverter::fromDOMElement(node);

    static struct ParamSet {
        const char* name;	// parameter name
        void (CS_Krypton::* setFunc)(float);
        			// ptr to setXXX member function
				// for setting parameter.
    } paramSet[] = {
	{ "Kw",		&CS_Krypton::setKw },
	{ "V0",		&CS_Krypton::setV0 },
	{ "PathLength",	&CS_Krypton::setPathLength },
	{ "Bias",	&CS_Krypton::setBias },
    };

    for (unsigned int i = 0; i < sizeof(paramSet) / sizeof(paramSet[0]); i++) {
	const Parameter* param = getParameter(paramSet[i].name);
	if (!param) continue;
	if (param->getLength() != 1) 
	    throw n_u::InvalidParameterException("CS_Krypton",
		"parameter", string("bad length for ") + paramSet[i].name);
	// invoke setXXX member function
	(this->*paramSet[i].setFunc)(param->getNumericValue(0));
    }
}

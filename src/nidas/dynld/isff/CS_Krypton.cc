// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
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

#include "CS_Krypton.h"
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
    _calFile(0)
{
    setUnits("g/m^3");
}

CS_Krypton::CS_Krypton(const CS_Krypton& x):
    VariableConverter(x),
    _Kw(x._Kw),_V0(x._V0),_logV0(::log(_V0)),
    _pathLength(x._pathLength), _bias(x._bias),
    _pathLengthKw(_pathLength * _Kw),
    _calFile(0)
{
    setUnits(x.getUnits());
}

CS_Krypton& CS_Krypton::operator=(const CS_Krypton& rhs)
{
    if (&rhs != this) {
        *(VariableConverter*)this = rhs;
        setKw(rhs.getKw());
        setV0(rhs.getV0());
        setPathLength(rhs.getPathLength());
        setBias(rhs.getBias());

        if (rhs._calFile) _calFile = new CalFile(*rhs._calFile);
    }
    return *this;
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

std::string CS_Krypton::toString() const
{
    return "CS_Krypton::toString() unsupported";
}

void CS_Krypton::fromString(const std::string&) 
    	throw(n_u::InvalidParameterException)
{
    throw n_u::InvalidParameterException(
    	"CS_Krypton::fromString() not supported yet");
}

void CS_Krypton::readCalFile(dsm_time_t t) throw()
{
    if (_calFile) {
        while(t >= _calFile->nextTime().toUsecs()) {
            float d[5];
            try {
                n_u::UTime calTime;
                int n = _calFile->readCF(calTime, d,sizeof d/sizeof(d[0]));
                if (n > 0) setKw(d[0]);
                if (n > 1) setV0(d[1]);
                if (n > 2) setPathLength(d[2]);
                if (n > 3) setBias(d[3]);
            }
            catch(const n_u::EOFException& e)
            {
            }
            catch(const n_u::IOException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    _calFile->getCurrentFileName().c_str(),e.what());
                setKw(floatNAN);
                setV0(floatNAN);
                setPathLength(floatNAN);
                setBias(floatNAN);
                delete _calFile;
                _calFile = 0;
                break;
            }
            catch(const n_u::ParseException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    _calFile->getCurrentFileName().c_str(),e.what());
                setKw(floatNAN);
                setV0(floatNAN);
                setBias(floatNAN);
                setPathLength(floatNAN);
                delete _calFile;
                _calFile = 0;
                break;
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

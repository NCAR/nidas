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
    _pathLength(1.3), _bias(0.0),_pathLengthKw(_pathLength * _Kw)
{
    setUnits("g/m^3");
}

CS_Krypton* CS_Krypton::clone() const
{
    return new CS_Krypton(*this);
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


void
CS_Krypton::
parseFields(CalFile* cf)
{
    float d[5];
    int n = cf->getFields(0, sizeof d/sizeof(d[0]), d);
    if (n > 0) setKw(d[0]);
    if (n > 1) setV0(d[1]);
    if (n > 2) setPathLength(d[2]);
    if (n > 3) setBias(d[3]);
}


void
CS_Krypton::
reset()
{
    setKw(floatNAN);
    setV0(floatNAN);
    setPathLength(floatNAN);
    setBias(floatNAN);
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

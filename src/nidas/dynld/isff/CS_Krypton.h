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

#ifndef NIDAS_DYNLD_ISFF_CS_KRYPTON_H
#define NIDAS_DYNLD_ISFF_CS_KRYPTON_H

#include <nidas/core/VariableConverter.h>

#include <cmath>

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::core;

/**
 * A data converter from voltage to water vapor density in gm/m^3
 * for a Campbell Scientfic krypton hygrometer.
 */
class CS_Krypton: public VariableConverter
{
public:

    CS_Krypton();

    CS_Krypton(const CS_Krypton&);

    CS_Krypton& operator=(const CS_Krypton&);

    ~CS_Krypton();

    CS_Krypton* clone() const;

    void setCalFile(CalFile* val);

    CalFile* getCalFile();

    const CalFile* getCalFile() const
    {
        return _calFile;
    }

    /**
     * @param val Kw parameter from sensor calibration.
     */
    void setKw(float val)
    {
        _Kw = val;
	_pathLengthKw = _pathLength * _Kw;
    }

    float getKw() const
    {
        return _Kw;
    }

    /**
     * @param val V0 value in millivolts.
     */
    void setV0(float val)
    {
        _V0 = val;
	_logV0 = ::log(_V0);
    }

    float getV0() const
    {
        return _V0;
    }

    /**
     * @param val Pathlength of sensor, in cm.
     */
    void setPathLength(float val)
    {
        _pathLength = val;
	_pathLengthKw = _pathLength * _Kw;
    }

    float getPathLength() const
    {
        return _pathLength;
    }

    /**
     * @param val Bias (g/m^3) to be removed from data values.
     */
    void setBias(float val)
    {
        _bias = val;
    }

    float getBias() const
    {
        return _bias;
    }

    void readCalFile(dsm_time_t t) throw();

    /**
     * Convert a voltage to water vapor density in g/m^3.
     */
    double convert(dsm_time_t t, double volts);

    std::string toString() const;

    void fromString(const std::string&) 
    	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

private:

    float _Kw;

    float _V0;

    float _logV0;

    float _pathLength;

    float _bias;

    float _pathLengthKw;

    CalFile* _calFile;

};

}}}	// namespace nidas namespace dynld namespace isff

#endif

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

#ifndef NIDAS_DYNLD_ISFF_PROPVANE_H
#define NIDAS_DYNLD_ISFF_PROPVANE_H

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/core/VariableConverter.h>

namespace nidas { namespace dynld { namespace isff {

/**
 * A serial prop vane wind sensor.  These typically report
 * wind speed and direction at about 1 sample/sec.
 * The process method of this class derives the
 * U and V vector components of the wind.
 */
class PropVane: public nidas::dynld::DSMSerialSensor
{
public:

    PropVane();

    ~PropVane();

    void addSampleTag(SampleTag* stag)
            throw(nidas::util::InvalidParameterException);

    void validateSscanfs() throw(nidas::util::InvalidParameterException);

    const std::string& getSpeedName() const { return _speedName; }

    void setSpeedName(const std::string& val) { _speedName = val; }

    const std::string& getDirName() const { return _dirName; }

    void setDirName(const std::string& val) { _dirName = val; }

    const std::string& getUName() const { return _uName; }

    void setUName(const std::string& val) { _uName = val; }

    const std::string& getVName() const { return _vName; }

    void setVName(const std::string& val) { _vName = val; }

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

private:

    std::string _speedName;

    std::string _dirName;

    std::string _uName;

    std::string _vName;

    /**
     * Index of wind speed in output sample.
     */
    int _speedIndex;

    /**
     * Index of wind direction in output sample.
     */
    int _dirIndex;

    /**
     * Index of wind u component in output sample.
     */
    int _uIndex;

    /**
     * Index of wind v component in output sample.
     */
    int _vIndex;

    /**
     * Length of output sample.
     */
    unsigned int _outlen;

    /**
     * If direction is derived from U,V, user may want a
     * correction applied to it.
     */
    nidas::core::VariableConverter* _dirConverter;

    /**
     * If speed is derived from U,V, user may want a
     * correction applied to it.
     */
    nidas::core::VariableConverter* _speedConverter;

    // no copying
    PropVane(const PropVane& x);

    // no assignment
    PropVane& operator=(const PropVane& x);


};

}}}	// namespace nidas namespace dynld namespace isff

#endif

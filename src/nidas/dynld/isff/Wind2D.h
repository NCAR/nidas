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

#ifndef NIDAS_DYNLD_ISFF_WIND2D_H
#define NIDAS_DYNLD_ISFF_WIND2D_H

#include <nidas/core/SerialSensor.h>
#include "WindOrienter.h"

namespace nidas {

namespace core {
    class VariableConverter;
}

/**
 * 2D anemometer, including prop vanes and sonics,
 * which report either wind speed and direction
 * or U and V.  The process method of this
 * class can derive U and V from speed and direction,
 * or conversely, speed and direction from U and V.
 * The varibles Spd,Dir or U,V which are mentioned first
 * in the configuration are assumed to be read from
 * the serial stream, and the others are then derived.
 *
 * This class can also apply 2D rotations, if an
 * an offset for the wind directon is specifed as
 * a calibration parameter.
 */
namespace dynld { namespace isff {

class Wind2D: public nidas::core::SerialSensor
{
public:

    Wind2D();

    ~Wind2D();

    void validate() throw(nidas::util::InvalidParameterException);

    void validateSscanfs() throw(nidas::util::InvalidParameterException);

    const std::string& getSpeedName() const { return _speedName; }

    void setSpeedName(const std::string& val) { _speedName = val; }

    const std::string& getDirName() const { return _dirName; }

    void setDirName(const std::string& val) { _dirName = val; }

    const std::string& getUName() const { return _uName; }

    void setUName(const std::string& val) { _uName = val; }

    const std::string& getVName() const { return _vName; }

    void setVName(const std::string& val) { _vName = val; }

    bool process(const nidas::core::Sample* samp,
        std::list<const nidas::core::Sample*>& results) throw();

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
     * Store the ID of the sample containing the wind variables.  By
     * default this is the first sample tag in the sensor.  Only one sample
     * can contain wind variables.
     **/
    nidas::core::dsm_sample_id_t _wind_sample_id;

    /**
     * A correction can be applied to the wind direction,
     * which is the common situation when the aneometer
     * is not aligned to report direction with respect to 
     * North. If the direction is derived from U,V, then
     * U,V are re-computed from the speed and corrected direction.
     */
    nidas::core::VariableConverter* _dirConverter;

    /**
     * A correction can be applied to the wind speed.
     * If the speed is derived from U,V, then
     * U,V are re-computed from the direction and corrected speed.
     */
    nidas::core::VariableConverter* _speedConverter;

    typedef nidas::dynld::isff::WindOrienter WindOrienter;

    WindOrienter _orienter;

    // no copying
    Wind2D(const Wind2D& x);

    // no assignment
    Wind2D& operator=(const Wind2D& x);

};

}}}	// namespace nidas namespace dynld namespace isff

#endif

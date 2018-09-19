// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2018, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DNYLD_ISFF_WINDORIENTER_H
#define NIDAS_DNYLD_ISFF_WINDORIENTER_H

#include <string>

namespace nidas {

namespace dynld { namespace isff {

/**
 * A class for rotating winds according to different orientations of the
 * wind sensor.
 */
class WindOrienter
{
public:

    WindOrienter();

    /**
     * Apply orientation changes to the wind components.
     **/
    void
    applyOrientation(float* uvwt);

    /**
     * Parse the orientation parameter and set the vectors which translate
     * the axes and signs of the wind sensor components.  The parameter
     * must be one string: 'normal' (default), 'down', 'lefthanded',
     * 'flipped' or 'horizontal'.  Throws InvalidParameterException if the
     * string cannot be parsed.  Pass @p name to include an identifying
     * name in log messages about which sensor is being oriented.
     **/
    void
    setOrientation(const std::string& orientation, const std::string& name="");

protected:

    /**
     * Is the sonic oriented in a unusual way, e.g. upside-down, etc?
     */
    bool _unusualOrientation;

    /**
     * Index transform vector for wind components.
     * Used for unusual sonic orientations, as when the sonic
     * is hanging down, when the usual sonic w axis becomes the
     * new u axis, u becomes w and v becomes -v.
     */
    int _tx[3];

    /**
     * Wind component sign conversion. Also used for unusual sonic
     * orientations, as when the sonic is hanging down, and the sign
     * of v is flipped.
     */
    int _sx[3];

};

}}}	// namespace nidas namespace dynld namespace isff

#endif // NIDAS_DNYLD_ISFF_WINDORIENTER_H

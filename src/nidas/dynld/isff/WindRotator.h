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

#ifndef NIDAS_DNYLD_ISFF_WINDROTATOR_H
#define NIDAS_DNYLD_ISFF_WINDROTATOR_H

namespace nidas { namespace dynld { namespace isff {

/**
 * Rotate a (U,V) 2D wind vector by an angle.
 * Typically used to correct winds for anemometer orientation,
 * rotating U,V from instrument coordinates to
 * geographic coordinates, where +U is wind to the east,
 * and +V is to the north.
 */
class WindRotator {
public:

    WindRotator();

    double getAngleDegrees() const;

    void setAngleDegrees(double val);

    void rotate(float* up, float* vp) const;

private:

    double _angle;

    double _sinAngle;

    double _cosAngle;
};


}}}	// namespace nidas namespace dynld namespace isff

#endif

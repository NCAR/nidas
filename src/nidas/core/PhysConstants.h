// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_PHYSCONSTANTS_H
#define NIDAS_CORE_PHYSCONSTANTS_H

namespace nidas { namespace core {

const double MS_PER_KNOT = 1852.0 / 3600.0;

/*
 * 6894.76 Pa/psi * 1 mbar/100Pa = 68.9476 mbar/psi
 */
const float MBAR_PER_PSI = 68.9476;

const float MBAR_PER_KPA = 10.0;

const float KELVIN_AT_0C = 273.15;

}}	// namespace nidas namespace core

#endif


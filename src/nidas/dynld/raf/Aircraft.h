// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_RAF_AIRCRAFT_H
#define NIDAS_DYNLD_RAF_AIRCRAFT_H

#include <nidas/core/Site.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Aircraft is a sub-class of a measurement Site.
 * A Site contains a collection of Parameters, so most any
 * Parameter specific to an Aircraft can be supported.
 */

class Aircraft : public Site {
public:
    Aircraft();

    virtual ~Aircraft();

    /**
     * Get/Set tail number of this aircraft.
     */
    std::string getTailNumber() const;

    void setTailNumber(const std::string& val);

};

}}}	// namespace nidas namespace dynld namespace raf

#endif

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

#include <nidas/dynld/raf/Aircraft.h>

#include <iostream>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

NIDAS_CREATOR_FUNCTION_NS(raf,Aircraft)

Aircraft::Aircraft()
{
    /*
     * Do we want DSMSensor::process methods at this site to apply
     * variable conversions? This can be changed with the
     * "applyCals" boolean parameter in the XML.  Let it default
     * to false, the original behaviour, until things are settled 
     * in this area.
     */
    _applyCals = false;
}

Aircraft::~Aircraft()
{
}

/*
 * An example of an Aircraft attribute that is maintained as a
 * Site parameter.
 */
string Aircraft::getTailNumber() const {
    const Parameter* tail = getParameter("tailNumber");
    if (tail && tail->getLength() > 0) return tail->getStringValue(0);
    else return "unknown";
}

void Aircraft::setTailNumber(const string& val) {
    ParameterT<string>* param = new ParameterT<string>;
    param->setName("tailNumber");
    param->setValue(val);
    addParameter(param);
}


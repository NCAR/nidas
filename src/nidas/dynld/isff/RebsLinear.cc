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

#include <nidas/dynld/isff/RebsLinear.h>
#include <nidas/core/CalFile.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Logger.h>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,RebsLinear)

RebsLinear::RebsLinear(): Polynomial()
{
    float tmpcoefs[] = { 0.0, 1.0, 0.0, 1.0 };
    setCoefficients(tmpcoefs,sizeof(tmpcoefs)/sizeof(tmpcoefs[0]));
}

RebsLinear* RebsLinear::clone() const
{
    return new RebsLinear(*this);
}

string RebsLinear::toString()
{
    const std::vector<float>& coefs = getCoefficients();

    ostringstream ost;
    ost << "rebslinear ";
    if (getCalFile()) 
        ost << "calfile=" << getCalFile()->getPath() << ' ' << getCalFile()->getFile() << endl;
    else {
        ost << "coefs=";
        for (unsigned int i = 0; i < NUM_COEFS; i++) {
            if (i < coefs.size()) ost << coefs[i] << ' ';
            else ost << coefs[i] << "nan";
        }
    }
    ost << " units=\"" << getUnits() << "\"" << endl;
    return ost.str();
}

float RebsLinear::convert(dsm_time_t t,float val)
{
    readCalFile(t);

    unsigned int n;
    const float* coefs = getCoefficients(n);

    if (val < 0) {
        if (n > SLOPE_NEG)
            return val * coefs[SLOPE_NEG] + coefs[INTCP_NEG];
    }
    else if (n > SLOPE_POS)
            return val * coefs[SLOPE_POS] + coefs[INTCP_POS];
    return floatNAN;
}


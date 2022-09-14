/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
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


#include "WindOrienter.h"

#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/Logger.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/Project.h>

using namespace nidas::dynld::isff;
using nidas::core::Parameter;
using nidas::core::Project;
using namespace std;

namespace n_u = nidas::util;

WindOrienter::
WindOrienter() :
  _unusualOrientation(false)
{
    /* index and sign transform for usual sonic orientation.
     * Normal orientation, no component change: 0 to 0, 1 to 1 and 2 to 2,
     * with no sign change. */
    for (int i = 0; i < 3; i++) {
        _tx[i] = i;
        _sx[i] = 1;
    }
}


void
WindOrienter::
setOrientation(const std::string& orientation, const std::string& name)
{
    /* _tx and _sx are used in the calculation of a transformed wind
     * vector as follows:
     *
     * for i = 0,1,2
     *     dout[i] = _sx[i] * wind_in[_tx[i]]
     * where:
     *  dout[0,1,2] are the new, transformed U,V,W
     *  wind_in[0,1,2] are the original U,V,W in raw sonic coordinates
     *
     *  When the sonic is in the normal orientation, +w is upwards
     *  approximately w.r.t gravity, and +u is wind into the sonic array.
     */
    DLOG(("") << name << " setting orientation to " << orientation);
    if (orientation == "normal")
    {
        _tx[0] = 0;
        _tx[1] = 1;
        _tx[2] = 2;
        _sx[0] = 1;
        _sx[1] = 1;
        _sx[2] = 1;
        _unusualOrientation = false;
    }
    else if (orientation == "down")
    {
        /* For flow-distortion experiments, the sonic may be mounted 
         * pointing down. This is a 90 degree "down" rotation about the
         * sonic v axis, followed by a 180 deg rotation about the sonic u axis,
         * flipping the sign of v.  Transform the components so that the
         * new +w is upwards wrt gravity.
         * new    raw sonic
         * u      w
         * v      -v
         * w      u
         */
        _tx[0] = 2;     // new u is raw sonic w
        _tx[1] = 1;     // v is raw sonic -v
        _tx[2] = 0;     // new w is raw sonic u
        _sx[0] = 1;
        _sx[1] = -1;    // v is -v
        _sx[2] = 1;
        _unusualOrientation = true;
    }
    else if (orientation == "lefthanded")
    {
        /* If wind direction is measured counterclockwise, convert to 
         * clockwise (dir = 360 - dir). This is done by negating the v 
         * component.
         * new    raw sonic
         * u      u
         * v      -v
         * w      w
         */
        _tx[0] = 0;
        _tx[1] = 1;
        _tx[2] = 2;
        _sx[0] = 1;
        _sx[1] = -1; //v is -v
        _sx[2] = 1;
        _unusualOrientation = true;
    }
    else if (orientation == "flipped")
    {
        /* Sonic flipped over, a 180 deg rotation about sonic u axis.
         * Change sign on v,w:
         * new    raw sonic
         * u      u
         * v      -v
         * w      -w
         */
        _tx[0] = 0;
        _tx[1] = 1;
        _tx[2] = 2;
        _sx[0] = 1;
        _sx[1] = -1;
        _sx[2] = -1;
        _unusualOrientation = true;
    }
    else if (orientation == "horizontal")
    {
        /* Sonic flipped on its side. For CSAT3, the labelled face of  the
         * "junction box" faces up.
         * Looking "out" from the tower in the -u direction, this is a 90 deg CC
         * rotation about the u axis, so no change to u,
         * new w is sonic v (sonic v points up), new v is sonic -w.
         * new    raw sonic
         * u      u
         * v      -w
         * w      v
         */
        _tx[0] = 0;
        _tx[1] = 2;
        _tx[2] = 1;
        _sx[0] = 1;
        _sx[1] = -1;
        _sx[2] = 1;
        _unusualOrientation = true;
    }
    else
    {
        throw n_u::InvalidParameterException
            (name, "orientation parameter",
             "must be one string: 'normal' (default), 'down', 'lefthanded', "
             "'flipped' or 'horizontal'");
    }
    float before[3] = { 1.0, 2.0, 3.0 };
    float after[3] = { 1.0, 2.0, 3.0 };
    
    applyOrientation(after);
    DLOG(("sonic wind orientation will convert (%g,%g,%g) to (%g,%g,%g)",
          before[0], before[1], before[2], after[0], after[1], after[2]));
}


void
WindOrienter::
applyOrientation(float* uvwt)
{
    if (_unusualOrientation)
    {
        float dn[3];
        for (int i = 0; i < 3; i++)
        {
            dn[i] = _sx[i] * uvwt[_tx[i]];
        }
        memcpy(uvwt, dn, sizeof(dn));
    }
}

bool
WindOrienter::
applyOrientation2D(float* u, float* v)
{
    if (_unusualOrientation)
    {
        float dn[3] = { *u, *v, 0 };
        applyOrientation(dn);
        *u = dn[0];
        *v = dn[1];
        return true;
    }
    return false;
}

bool
WindOrienter::
handleParameter(const Parameter* parameter, const std::string& name)
{
    if (parameter->getName() == "orientation") {
        if (parameter->getType() == Parameter::STRING_PARAM &&
            parameter->getLength() == 1)
        {
            const Project* project = Project::getInstance();
            setOrientation
                (project->expandString(parameter->getStringValue(0)),
                name);
            return true;
        }
        else
        {
            throw n_u::InvalidParameterException
                (name, parameter->getName(),
                 "must be a string parameter of length 1");
        }
    }
    return false;
}

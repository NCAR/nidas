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

#ifndef NIDAS_DNYLD_ISFF_WINDTILTER_H
#define NIDAS_DNYLD_ISFF_WINDTILTER_H

#include <math.h>  // for M_PI

namespace nidas { namespace dynld { namespace isff {

/**
 * WindTilter is used to apply a 3d rotation matrix to a wind vector.
 *
 * WindTilter generates a matrix for rotating 3d wind
 * vectors from an orthogonal sonic coordinate system to a new coordinate
 * system whose w axis is defined by two angles, lean and leanaz.  It is
 * commonly used to rotate sonic wind data to mean flow
 * coordinates, whose w axis is normal to the plane of the observed flow.
 *
 * lean is the angle in radians between the flow normal and the
 * sonic w axis (0 <= lean <= 180)
 *
 * leanaz is the clock angle in radians from the sonic u axis to the
 * projection of the flow normal in the sonic uv plane, positive
 * toward the sonic v axis. (0 <= leanaz < 360)
 *
 * The unit axes of sonic coordinates:		Us, Vs, Ws
 * The unix axes of the flow coordiate system:	Uf, Vf, Wf
 *
 * This is primarily a rotation of the W axis. The Uf axis
 * remains in the plane of Us and an "up" direction.
 *
 * In sonic (aka instrument coordinates) the Wf axis is
 *      sin(lean) * cos(leanaz)
 *      sin(lean) * sin(leanaz)
 *      cos(lean)
 *
 * Once you have the Wf axis defining the Uf,Vf plane, there is some
 * uncertainty about where to put the Uf axis on that plane.
 *
 * If Uf is the intersection of the normal plane with the old Us,Ws 
 * plane, then
 * Uf = Vs X Wf		cross product of Vs and Wf
 *    = (0 1 0) X Wf	In sonic coords Vs is just (0 1 0)
 *			Then normalize Uf.
 * The above method is used when UP_IS_SONIC_W == true
 *
 * If Uf is the intersection of the flow normal plane with the
 * Wf,Us plane, then
 *   
 *  Uf = (Wf X Us) X Wf 	(normalized)
 *  
 * The above method is used when UP_IS_SONIC_W == false
 *
 * Right now the default value of UP_IS_SONIC_W is false.
 *
 * I suppose if you had your sonic on the side
 * of a steep hill you may want "up" to be the sonic W axis.
 *
 * **********************************************************************
 * How to determine the flow normal from sonic wind data.
 *
 * The value of W in flow coordinates (Wf) is the dot product of
 * the corrected sonic wind vector (us-uoff,vs-voff,ws-woff) with the
 * Wf unit vector, in sonic coordinates.
 * uoff, voff and woff are the sonic biases.
 *
 * Wf = (us-uoff) * sin(lean)cos(leanaz) + (vs-voff)*sin(lean)sin(leanaz)
 *  + (ws-woff) * cos(lean)
 *
 * An averaged Wf is by definition zero, which gives an equation
 * for wsbar in terms of usbar and vsbar.
 *
 * wsbar = woff - (usbar-uoff)*tan(lean)cos(leanaz)
 *	 - (vsbar-voff)*tan(lean)sin(leanaz)
 *
 * Therefore wsbar should look like a linear function of the corrected
 * and averaged U and V components from the sonic, (uscor=usbar-uoff):
 *
 *  wsbar = b1 + b2 * uscor + b3 * vscor
 *
 * One can determine these b coefficients with a minimum variance
 * fit, and
 *
 *  b1 = woff
 *  b2 = -tan(lean)cos(leanaz)
 *  b3 = -tan(lean)sin(leanaz)
 *
 * lean and leanaz are therefore:
 * lean = atan(sqrt(b2*b2 + b3*b3))
 * leanaz = atan2(-b3,-b2)
 *
 * @version $Revision$
 * @author  $Author$
 */
class WindTilter {
public:

    WindTilter();

    double getLeanDegrees() const
    {
        return _lean * 180.0 / M_PI;
    }

    void setLeanDegrees(double val)
    {
        _lean = val * M_PI / 180.0;
        computeMatrix();
    }

    double getLeanAzimuthDegrees() const
    {
        return _leanaz * 180.0 / M_PI;
    }

    void setLeanAzimuthDegrees(double val)
    {
        _leanaz = val * M_PI / 180.;
        if (!_identity) computeMatrix();
    }

    bool isIdentity() const
    {
        return _identity;
    }

    void rotate(float*u, float*v, float*w) const;

private:

    void computeMatrix();

    double _lean;

    double _leanaz;

    bool _identity;

    double _mat[3][3];

    bool UP_IS_SONIC_W;
};


}}}	// namespace nidas namespace dynld namespace isff

#endif

// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_DYNLD_PAROSCI_202BG_CALIBRATION_h
#define NIDAS_DYNLD_PAROSCI_202BG_CALIBRATION_h

#include <nidas/core/Sample.h>
#include <nidas/core/CalFile.h>
#include <nidas/util/Exception.h>

using namespace nidas::core;

namespace nidas { namespace dynld {

/**
 * Support for reading a calibration file for a ParoScientific 202BG sensor.
 */
class ParoSci_202BG_Calibration {

public:

    ParoSci_202BG_Calibration();

    void readCalFile(nidas::core::CalFile* cf, dsm_time_t tt)
        throw(nidas::util::Exception);

    void setU0(float val) { _U0 = val; }

    void setYs(float v0, float v1, float v2, float v3)
    {
        _Y[0] = v0; _Y[1] = v1; _Y[2] = v2; _Y[3] = v3;
    }

    void setCs(float v0, float v1, float v2)
    {
        _C[0] = v0; _C[1] = v1; _C[2] = v2;
    }

    void setDs(float v0, float v1)
    {
        _D[0] = v0; _D[1] = v1;
    }

    void setTs(float v0, float v1, float v2, float v3, float v4)
    {
        _T[0] = v0; _T[1] = v1; _T[2] = v2; _T[3] = v3; _T[4] = v4;
    }

    void setCommonModeCoefs(float v0, float v1, float v2, float v3)
    {
        _a[0] = v0; _a[1] = v1; _b = v2; _P0 = v3;
    }

    float getU0() const { return _U0; }

    double computeTemperature(double usec);

    double computePressure(double tper, double pper);

    double correctPressure(double pgauge, double pstatic);

private:


    float _U0;

    float _Y[4];

    float _C[3];

    float _D[2];

    float _T[5];

    float _a[2];
    
    float _b;

    float _P0;

};

}}	// namespace nidas namespace dynld

#endif

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
/*

 Taken from the Honeywell Engineering Specification A.4.1 spec. no.
 EB7022597 cage code 55939 "Air Data Computer"    (pages A-53..79).

 */
#include <nidas/dynld/raf/ADC_HW_EB7022597.h>

using namespace nidas::dynld::raf;

NIDAS_CREATOR_FUNCTION_NS(raf,ADC_HW_EB7022597);

double ADC_HW_EB7022597::processLabel(const int data, sampleType *stype)
{
    int sign = 1;

    // Default to single precision. If some label needs to be
    // DOUBLE_ST, change it in the appropriate case.
    *stype = FLOAT_ST;

    //err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned int)0xffffff00) );

    switch (data & 0xff) {

    case 0203:  // BNR - pressure alt         (ft)
    case 0204:  // BNR - baro corr alt #1     (ft)
    case 0220:  // BNR - baro corr alt #2     (ft)
    case 0252:  // BNR - baro corr alt #4     (ft)
        if ((data & SSM) != SSM) break;
        return (data<<3>>13) * 0.5 * FT_MTR;

    case 0205:  // BNR - mach                 (mach)
        if ((data & SSM) != SSM) break;
        return (data<<3>>13) * 1.5625e-5;

    case 0206:  // BNR - computed air speed   (knot)
    case 0207:  // BNR - max oper. speed      (knot)
        if ((data & SSM) != SSM) break;
        return (data<<3>>13) * 3.90625e-3 * KTS_MS;

    case 0210:  // BNR - true air speed       (knot)
        if ((data & SSM) != SSM) break;
        return (data<<3>>13) * 7.8125e-3 * KTS_MS;

    case 0211:  // BNR - total air pressure   (deg_C)
    case 0213:  // BNR - static air pressure  (deg_C)
    case 0215:  // BNR - impact pressure      (mbar)
        if ((data & SSM) != SSM) break;
        return (data<<3>>13) * 1.953125e-3;

    case 0212:  // BNR - altitude rate        (ft/min)
        if ((data & SSM) != SSM) break;
        return (data<<3>>13) * 0.125 * FPM_MPS;

    case 0217:  // BNR - static pressure      (inHg)
        if ((data & SSM) != SSM) break;
        return (data<<3>>13) * 2.44140625e-4 * INHG_MBAR;

    case 0234:  // BCD - baro correction #1   (mbar)
    case 0236:  // BCD - baro correction #2   (mbar)
        if (((data & SSM) == NCD) || ((data & SSM) == TST)) break;
        if ((data & SSM) == SSM) sign = -1;
        return (
                ((data & (0x7<<26)) >> 26) * 1000.0 +
                ((data & (0xf<<22)) >> 22) * 100.0 +
                ((data & (0xf<<18)) >> 18) * 10.0 +
                ((data & (0xf<<14)) >> 14) * 1.0 +
                ((data & (0xf<<10)) >> 10) * 0.1
               ) * sign;

    case 0235:  // BCD - baro correction #1   (inHg)
    case 0237:  // BCD - baro correction #2   (inHg)
        if (((data & SSM) == NCD) || ((data & SSM) == TST)) break;
        if ((data & SSM) == SSM) sign = -1;
        return (
                ((data & (0x7<<26)) >> 26) * 10.0 +
                ((data & (0xf<<22)) >> 22) * 1.0 +
                ((data & (0xf<<18)) >> 18) * 0.1 +
                ((data & (0xf<<14)) >> 14) * 0.01 +
                ((data & (0xf<<10)) >> 10) * 0.001
               ) * sign * INHG_MBAR;

    case 0242:  // BNR - total pressure       (mbar)
        if ((data & SSM) != SSM) break;
        return (data<<3>>13) *  7.8125e-3;

    case 0244:  // BNR - norm angle of attack (ratio)
        if ((data & SSM) != SSM) break;
        return (data<<3>>13) * 7.62939453125e-6;

    case 0270:  // DIS - discrete #1          ()
        return (data>>17) & 0x7;  // Air_Traffic_Control Select, Overspeed, Weight On Wheels
    case 0271:  // DIS - discrete #2          ()
    case 0350:  // DIS - maintence data #1    ()
    case 0351:  // DIS - maintence data #2    ()
    case 0352:  // DIS - maintence data #3    ()
    case 0353:  // DIS - maintence data #4    ()
    case 0371:  // DIS - equipment_id         ()
    default:
        // unrecognized label type, return raw data
        *stype = UINT32_ST;
        return (data<<3>>13);
        break;
    }
    return doubleNAN;
}

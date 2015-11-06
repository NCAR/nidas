// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2012, Copyright University Corporation for Atmospheric Research
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
 EB7032239.

 */

#include <nidas/dynld/raf/LSZ_HW_EB7032239.h>

using namespace nidas::dynld::raf;

NIDAS_CREATOR_FUNCTION_NS(raf, LSZ_HW_EB7032239);

double LSZ_HW_EB7032239::processLabel(const int data, sampleType *stype)
{
    int label = data & 0xff;
    int nTargets, mode;

    // Default to single precision. If some label needs to be
    // DOUBLE_ST, change it in the appropriate case.
    *stype = FLOAT_ST;

    if (label >= 0100 && label <= 0164) // This is the case for lightning cell.
    {

    }
    else
    switch (label)
    {
        case 0001:  // BNR - Preamble word.  Page 2.
            nTargets = (data>>23) & 0x3f;
            mode = (data>>8) & 0x7fff;
            return mode;

        default:
            // unrecognized label type, return raw data
            *stype = UINT32_ST;
            return (data<<3>>13);
            break;
    }
    return doubleNAN;
}

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
#ifndef NIDAS_DYNLD_RAF_GPS_HW_HG2021GB02_H
#define NIDAS_DYNLD_RAF_GPS_HW_HG2021GB02_H

#include <nidas/dynld/raf/DSMArincSensor.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * ARINC IRS label processor.
 *
 * Taken from the Honeywell Installation Manual PN HG2021GB02/GD02
 * Table 207 "GNSSU ARINC 429 Output Data" (GPS)  (pages 217-219).
 */
class GPS_HW_HG2021GB02 : public DSMArincSensor {

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    GPS_HW_HG2021GB02() : 
        Pseudo_Range_sign(floatNAN),
        SV_Position_X_sign(floatNAN),
        SV_Position_Y_sign(floatNAN),
        SV_Position_Z_sign(floatNAN),
        GPS_Latitude_sign(floatNAN),
        GPS_Longitude_sign(floatNAN),
        _lat110(doubleNAN),
        _lon111(doubleNAN){

#ifdef DEBUG
            err("");
#endif
        }

    /** Process all labels from this instrument. */
    double processLabel(const int data, sampleType*);

private:

    /** Mutli-label values' sign is based on the first label. */
    float Pseudo_Range_sign;
    float SV_Position_X_sign;
    float SV_Position_Y_sign;
    float SV_Position_Z_sign;
    float GPS_Latitude_sign;
    float GPS_Longitude_sign;

    double _lat110;
    double _lon111;
};

}}}	// namespace nidas namespace dynld namespace raf

#endif

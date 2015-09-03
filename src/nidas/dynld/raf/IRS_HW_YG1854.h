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
#ifndef NIDAS_DYNLD_RAF_IRS_HW_YG1854_H
#define NIDAS_DYNLD_RAF_IRS_HW_YG1854_H

#include <nidas/dynld/raf/Arinc_IRS.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * ARINC IRS label processor.
 *
 * Taken from the Honeywell installation manual for the
 * YG1854 LASERREF SM IRS/GPIRS        (pages 640-650).
 */
class IRS_HW_YG1854 : public Arinc_IRS {

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    IRS_HW_YG1854() {
#ifdef DEBUG
        err("");
#endif
    }

    /** Process all labels from this instrument. */
    double processLabel(const int data, sampleType*);
};

}}}	// namespace nidas namespace dynld namespace raf

#endif

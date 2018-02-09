// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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

#ifndef _nidas_dynld_raf_pip_image_h_
#define _nidas_dynld_raf_pip_image_h_

#include <nidas/core/SerialSensor.h>
#include <nidas/util/EndianConverter.h>
#include <nidas/util/InvalidParameterException.h>


namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/*
 * Sensor class to receive PIP images 
 */
class PIP_Image : public SerialSensor
{
public:
    PIP_Image();
    ~PIP_Image();

    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();

    /**
     * Return bits-per-slice; same as the number of diodes in the probe.
     */
    virtual int NumberOfDiodes() const { return 64; }

protected:

    // Probe produces Big Endian.
    static const nidas::util::EndianConverter * bigEndian;

    // Tap2D value sent back from driver has little endian ntap value
    static const nidas::util::EndianConverter * littleEndian;

private:

};

}}}
 
#endif

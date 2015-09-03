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

#ifndef _nidas_dynld_raf_2d32_usb_h_
#define _nidas_dynld_raf_2d32_usb_h_

#include <nidas/dynld/raf/TwoD_USB.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Sensor class for the standard PMS2D probes where the data signals
 * are converted to USB by a converter box.  These probes have the
 * standard 32 diodes.
 */
class TwoD32_USB : public TwoD_USB 
{
public:
    TwoD32_USB();
    ~TwoD32_USB();

    bool process(const Sample * samp, std::list < const Sample * >&results)
        throw();

    virtual int NumberOfDiodes() const { return 32; }


protected:
    bool processImage(const Sample * samp, std::list < const Sample * >&results,
					int stype) throw();

    static const unsigned char _overldString[];
    static const unsigned char _blankString[];

};  //endof_class

}}}                     // namespace nidas namespace dynld namespace raf
#endif

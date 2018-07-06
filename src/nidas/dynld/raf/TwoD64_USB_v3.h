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

#ifndef _nidas_dynld_raf_2d64_usb_v3_h_
#define _nidas_dynld_raf_2d64_usb_v3_h_

#include "TwoD64_USB.h"

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Class for the USB Fast-2DC.  This probe has a USB 2.0 interface
 * built in.  This probe has 64 diodes instead of the standard 32.
 * This is for the second version produced by Josh Carnes in 2018.
 * With faster electronics, 33Mhz clock and upgraded housekeeping
 * packet.
 */
class TwoD64_USB_v3 : public TwoD64_USB
{
public:
    TwoD64_USB_v3();
    ~TwoD64_USB_v3();

    virtual int TASToTap2D(void * t2d, float tas);

    virtual float Tap2DToTAS(const Tap2D * t2d) const;
    
    void validate() throw(nidas::util::InvalidParameterException);

private:
    
    size_t _nvars;

protected:
    virtual void init_parameters()
        throw(nidas::util::InvalidParameterException);

    /**
     * Process the Shadow-OR sample from the probe.
     */
   virtual bool processSOR(const Sample * samp, std::list < const Sample * >&results)
        throw();
};

}}}       // namespace nidas namespace dynld namespace raf
#endif

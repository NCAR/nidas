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

#ifndef _nidas_dynld_raf_phips_udp_h_
#define _nidas_dynld_raf_phips_udp_h_

#include <nidas/dynld/raf/UDPSocketSensor.h>


namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * PHIPS - HALO UDP Sensor.  This would be able to use the generic ascii class
 * except we need to parse a histogram where values are out of sequence.
 */
class PHIPS_UDP : public UDPSocketSensor
{

public:
    PHIPS_UDP();
    ~PHIPS_UDP();

    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();


protected:

    float scanValue(const char *cp);

};

}}}                     // namespace nidas namespace dynld namespace raf
#endif

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

#ifndef _nidas_dynld_raf_arinc_status_h_
#define _nidas_dynld_raf_arinc_status_h_

#include <nidas/dynld/UDPSocketSensor.h>
#include "AltaEnet.h"


namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;


/**
 * ARINC over UDP, data received from the Alta ARINC to Ethernet appliance.
 * This is for the status which comes from arinc_ctrl to different UDP port.
 * We only broke this out into a separate class so we could get some real-
 * status on the status page, otherwise this could be generic UDPSensor.
 */
class UDPArincStatus : public UDPSocketSensor
{

public:
    UDPArincStatus();
    virtual ~UDPArincStatus();

    virtual Sample* nextSample()
    {
        Sample *samp = UDPSocketSensor::nextSample();
/*
        if (samp) {
            const char *input = (const char *)samp->getConstVoidDataPtr();
            extractStatus(input, samp->getDataByteLength());
        }
*/
        return samp;
    }

    void printStatus(std::ostream& ostr) throw();


protected:

    void extractStatus(const char *msg, int len);

    /**
     * This contains the status of config verification between what we read
     * off the device and what is in the XML.  These are then used in
     * printStatus for the status page.  nextSample() is over-ridden in order
     * to get the PPS status.
     */
    std::map<std::string, int> configStatus;


};

}}}                     // namespace nidas namespace dynld namespace raf
#endif

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

// Verbose log messages are now compiled by default, but they can only be
// enabled by explicitly enabling verbose logging in a LogConfig, such as
// with a command-line option.
#define SLICE_DEBUG

#include <nidas/linux/usbtwod/usbtwod.h>
#include "TwoD64_USB_v3.h"
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/Variable.h>

#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

#include <asm/ioctls.h>
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;
using nidas::util::endlog;

NIDAS_CREATOR_FUNCTION_NS(raf, TwoD64_USB_v3)


TwoD64_USB_v3::TwoD64_USB_v3()
{
     _probeClockRate=33;                    //Default for v3 is 33 MHZ
     _timeWordMask=0x000003ffffffffffLL;    //Default for v3 is 42 bits
     _dofMask=0x10;
}

TwoD64_USB_v3::~TwoD64_USB_v3()
{
}

void TwoD64_USB_v3::init_parameters()
    throw(n_u::InvalidParameterException)
{
    TwoD_USB::init_parameters();
    /* Look for a sample tag with id=2. This is assumed to be
     * the shadowOR sample.  Check its rate.
     */
    float sorRate = 0.0;
    list<SampleTag *>& tags = getSampleTags();
    list<SampleTag *>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        const SampleTag * tag = *si;
        Variable & var = ((SampleTag *)tag)->getVariable(2);

        if (var.getName().compare(0, 5, "SHDOR") == 0) {
            sorRate = tag->getRate();
            _sorID = tag->getId();
        }
    }
    if (sorRate <= 0.0) throw n_u::InvalidParameterException(getName(),
        "sample","shadow OR sample rate not found");
}

int TwoD64_USB_v3::TASToTap2D(void * t2d, float tas)
{
    if (tas < DefaultTrueAirspeed)
        tas = DefaultTrueAirspeed;

    t2d = (Tap2D_v3 * )t2d;
    unsigned short * p = (unsigned short * )t2d;
    p[0]=(unsigned int)(tas*10.0);
    p[1]=(unsigned int)getResolutionMicron();
    return 0;
}

float TwoD64_USB_v3::Tap2DToTAS(const Tap2D * t2d) const
{
    unsigned short * p = (unsigned short * )t2d;
    return (float)p[0]/10.0;
}

bool TwoD64_USB_v3::processSOR(const Sample * samp,
                           list < const Sample * >&results) throw()
{
    const unsigned char * cp = (const unsigned char*) samp->getConstVoidDataPtr();
    unsigned int slen = samp->getDataByteLength();

    char buff[256];
    memcpy(buff, cp, slen);
    buff[slen] = 0;

cout << "V3::processSOR [" << buff << "]\n";
    //Until we decide how to code this
    return false;



    return true;
}


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

#include "TwoD_House.h"
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,TwoD_House)

// More for documentation than anything.
const size_t TwoD_House::V15_INDX = 0;
const size_t TwoD_House::TMP_INDX = 1;
const size_t TwoD_House::EE1_INDX = 4;
const size_t TwoD_House::EE32_INDX = 5;
const size_t TwoD_House::VN15_INDX = 6;
const size_t TwoD_House::V5_INDX = 7;


TwoD_House::~TwoD_House()
{
}

TwoD_House::TwoD_House() : DSMSerialSensor(), _noutValues(7)
{
  ::memset(_houseKeeping, 0, sizeof(_houseKeeping));
}


bool TwoD_House::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    const char * input = (const char *) samp->getConstVoidDataPtr();
    const char * eoinput = input + samp->getDataByteLength();

    unsigned int shado[5];
    unsigned int tag;
    unsigned int hkp;

    // skip over leading NULs
    for (; input < eoinput && *input == '\0'; input++);
    if (input == eoinput) return false;

    int nf = sscanf(input, "%x %x %x %x %x %x %x",
	&shado[0], &shado[1], &shado[2], &shado[3], &shado[4], &tag, &hkp);

    float shadow_or = 0.0;
    for (int iout = 0; iout < 5 && iout < nf; ++iout)
        shadow_or += shado[iout];

    // Push the housekeeping.  They are not all decoded/available every sample,
    // they come round robin.
    if (nf > 6 && tag < sizeof(_houseKeeping)/sizeof(_houseKeeping[0]))
       _houseKeeping[tag] = hkp;

    SampleT<float> * outs = getSample<float>(_noutValues);
    float * dout = outs->getDataPtr();
    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);

    *dout++ = _houseKeeping[V15_INDX];
    *dout++ = _houseKeeping[TMP_INDX];
    *dout++ = _houseKeeping[EE1_INDX] * 0.001;
    *dout++ = _houseKeeping[EE32_INDX] * 0.001;
    *dout++ = _houseKeeping[VN15_INDX];
    *dout++ = _houseKeeping[V5_INDX];

    *dout++ = shadow_or;

    // check for overflow
    assert((dout - outs->getDataPtr()) == _noutValues);

    results.push_back(outs);
    return true;
}

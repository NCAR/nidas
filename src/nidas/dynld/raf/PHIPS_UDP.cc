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

#include "PHIPS_UDP.h"

#include <nidas/util/Logger.h>


using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,PHIPS_UDP)

PHIPS_UDP::PHIPS_UDP() : _previousTotal(0)
{
  _previousCam[0] = _previousCam[1] = 00;
}

PHIPS_UDP::~PHIPS_UDP()
{
}

float PHIPS_UDP::scanValue(const char *input)
{
    float f1;

    if (input && sscanf(input,"%f", &f1) == 1)
        return f1;

    return floatNAN;
}

bool PHIPS_UDP::process(const Sample * samp,
                           list < const Sample * >&results) throw()
{
    const char *input = (const char*) samp->getConstVoidDataPtr();
    char sep = ',';

    if (!strncmp(input, "PHIPS-SCT,", 10))
    {
        // 3 scalars and 20 bin histogram == 23.
        SampleT<float> * outs = getSample<float>(23);
        outs->setTimeTag(samp->getTimeTag());
        outs->setId(getId() + 1);
        float * dout = outs->getDataPtr();

        input += 10;
        const char *cp = ::strchr(input, sep); // skip date/time stamp.
        if (cp) cp++;
        *dout++ = scanValue(cp);    // Sequence

        cp = ::strchr(cp, sep);
        if (cp) cp++;
        int val = scanValue(cp);    // Total
        *dout++ = val - _previousTotal;
        _previousTotal = val;

        cp = ::strchr(cp, sep);
        if (cp) cp++;
        *dout++ = scanValue(cp);    // Trigger

    
        // however there are 32 channels in the raw data, some will be
        // summed, and some not used.
        int channels[32];

        for (int i = 0; i < 32; ++i)
        {
            cp = ::strchr(cp, sep);
            if (cp) cp++;
            channels[i] = (int)scanValue(cp);
        }

        *dout++ = channels[24] + channels[25];
        *dout++ = channels[19];
        *dout++ = channels[16];
        *dout++ = channels[28] + channels[29];
        *dout++ = channels[30] + channels[31];
        *dout++ = channels[10];
        *dout++ = channels[7];
        *dout++ = channels[5];
        *dout++ = channels[3];
        *dout++ = channels[1];
        *dout++ = channels[2];
        *dout++ = channels[4];
        *dout++ = channels[6];
        *dout++ = channels[8];
        *dout++ = channels[9];
        *dout++ = channels[11];
        *dout++ = channels[12];
        *dout++ = channels[13];
        *dout++ = channels[14];
        *dout++ = channels[22];

        results.push_back(outs);
        return true;
    }

    if (!strncmp(input, "PHIPS-CAM,", 10))
    {
        // Two cameras.
        int seq, camera;

        input += 10;
        const char *cp = ::strchr(input, sep); // skip date/time stamp.
        if (cp) cp++;
        // Camera file name format.  2 Cameras, so get camera number
        //   (e.g. C1 below), and image number.
        // PhipsData_20171101-2111_30887666167900_000001_C1.png
        if (sscanf(input, "PhipsData_%*d-%*d_%*d_%d_C%d.png", &seq, &camera) == 2)
        {
            SampleT<float> * outs = getSample<float>(2);
            outs->setTimeTag(samp->getTimeTag());
            outs->setId(getId() + 2);
            float * dout = outs->getDataPtr();

            --camera;
            dout[camera] = seq - _previousCam[camera];
            _previousCam[camera] = seq;
            results.push_back(outs);
            return true;
        }
    }

    return false;
}


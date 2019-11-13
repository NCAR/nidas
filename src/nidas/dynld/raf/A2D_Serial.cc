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

#include "A2D_Serial.h"

#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>



using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;
using nidas::util::LogScheme;


NIDAS_CREATOR_FUNCTION_NS(raf, A2D_Serial)

A2D_Serial::A2D_Serial() : _hz_counter(0), _sampleRate(0), _deltaT(0), _shortPacketCnt(0), _badCkSumCnt(0)
{
}

A2D_Serial::~A2D_Serial()
{
}


void A2D_Serial::open(int flags) throw(n_u::IOException)
{
    SerialSensor::open(flags);

}

void A2D_Serial::init() throw(n_u::InvalidParameterException)
{
    CharacterSensor::init();

    // Determine number of floats we will recieve (_noutValues)
    list<SampleTag*>& stags = getSampleTags();
    if (stags.size() != 2)
        throw n_u::InvalidParameterException(getName(),"sample",
              "must be two <sample> tags for this sensor");


    _sampleRate = stags.back()->getRate();
    _deltaT = (int)rint(USECS_PER_SEC / _sampleRate);
}

bool A2D_Serial::checkCkSum(const Sample * samp)
{
    bool rc = false;
    const unsigned char * input = (unsigned char *) samp->getConstVoidDataPtr();
    unsigned nbytes = samp->getDataByteLength();

    unsigned commaCnt = 0, pos = 0;
    uint16_t cksum = 0;
    for (pos = 0; pos < nbytes && commaCnt < 5; ++pos)
    {
        if (input[pos] == ',') ++commaCnt;
        if (commaCnt < 5) cksum += input[pos];
    }

    if (commaCnt == 5)  // CheckSum is after the 5th comma, make sure we got there.
    {
        unsigned int ckSumSent;
        sscanf((const char *)&input[pos], "%x", &ckSumSent);
        cksum &= 0x00FF;
        rc = (cksum == ckSumSent);
        if (rc == false)
            WLOG(("%s: bad SerialAnalog checksum at ", getName().c_str()) <<
                n_u::UTime(samp->getTimeTag()).format(true,"%Y %m %d %H:%M:%S.%3f") <<
                ", #bad=" << ++_badCkSumCnt);
    }
    else
    {
        WLOG(("%s: short SerialAnalog packet at ",getName().c_str()) <<
                n_u::UTime(samp->getTimeTag()).format(true,"%Y %m %d %H:%M:%S.%3f") << ", #bad=" << ++_shortPacketCnt);
    }

    return rc;
}

bool A2D_Serial::process(const Sample * samp,
                           list < const Sample * >&results) throw()
{
    if (checkCkSum(samp) == false) return false;

    // Decode the data with the standard ascii scanner.
    bool rc = SerialSensor::process(samp, results);
    if (results.empty()) return false;

    size_t twentyPer = _sampleRate / 4;    // # of samples that makes 25%
    list<const Sample *>::const_iterator it = results.begin();
    for (; it != results.end(); ++it)
    {
        Sample * nco_samp = const_cast<Sample *>(*it);

        // Skip housekeeping; sample id 1.
        if ((nco_samp->getId() - getId()) == 1) {
            continue;
        }

        size_t usec = nco_samp->getTimeTag() % USECS_PER_SEC;
        float *values = (float *)nco_samp->getVoidDataPtr();
        _hz_counter = (int)values[0];

        // late samples whose timetag is in next sec are actually from previous second
        if (_hz_counter > (_sampleRate-twentyPer) && usec < twentyPer * _deltaT) usec += USECS_PER_SEC;

        // Reverse sitution if DSM clock is a bit slow
        if (_hz_counter < twentyPer && usec > (_sampleRate-twentyPer) * _deltaT) usec -= USECS_PER_SEC;

        dsm_time_t timeoffix = nco_samp->getTimeTag() - usec + (_hz_counter * _deltaT);
        nco_samp->setTimeTag(timeoffix);
    }

    return rc;
}

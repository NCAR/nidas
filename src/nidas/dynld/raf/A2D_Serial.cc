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

A2D_Serial::A2D_Serial() : _havePPS(false), _sampleRate(0), _deltaT(0), _shortPacketCnt(0), _badCkSumCnt(0), _largeTimeStampOffset(0)
{
}

A2D_Serial::~A2D_Serial()
{
}


void A2D_Serial::open(int flags) throw(n_u::IOException)
{
    SerialSensor::open(flags);

    readConfig();
}

void A2D_Serial::readConfig() throw(n_u::IOException)
{
    int nsamp = 0;
    bool done = false;

    write("#PCFG\n", 6);        // request config.

    // read with a timeout in milliseconds. Throws n_u::IOTimeoutException
    while (!done) {
        try {
            readBuffer(1 * MSECS_PER_SEC);

            // process all samples in buffer
            for (Sample* samp = nextSample(); samp; samp = nextSample()) {

                distributeRaw(samp);        // send it on to the clients

                nsamp++;
                const char* msg = (const char*) samp->getConstVoidDataPtr();
                if (strstr(msg, "!EOC")) done = true;

            }
            if (nsamp > 50) {
                WLOG(("%s: A2D_Serial open(): expected !EOC, not received",
                            getName().c_str()));
                done = true;
            }
        }
        catch (const n_u::IOTimeoutException& e) {
            throw e;
        }
    }

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
    const char * input = (char *) samp->getConstVoidDataPtr();

    if (input[0] == 'H')    // Header packet has no checksum
        return true;

    int nbytes = samp->getDataByteLength();
    char data[nbytes+1];
    ::memcpy(data, input, nbytes);
    data[nbytes] = 0;

    char *pos = ::strrchr(data, ',');
    if (pos == 0)
    {
        WLOG(("%s: short SerialAnalog packet at ",getName().c_str()) <<
            n_u::UTime(samp->getTimeTag()).format(true,"%Y %m %d %H:%M:%S.%3f") << ", #bad=" << ++_shortPacketCnt);
        return false;   // No comma's?  Can't be valid.
    }

    // Generate a checksum
    uint16_t cksum = 0;
    nbytes = pos - data; // sum through last comma
    for (int i = 0; i < nbytes; ++i) cksum += data[i];
    cksum &= 0x00FF;
    ++pos; // move past comma

    // Extract and compare with checksum sent.
    unsigned int ckSumSent;
    sscanf((const char *)pos, "%x", &ckSumSent);
    rc = (cksum == ckSumSent);
    if (rc == false)
            WLOG(("%s: bad SerialAnalog checksum at ", getName().c_str()) <<
                n_u::UTime(samp->getTimeTag()).format(true,"%Y %m %d %H:%M:%S.%3f") <<
                ", #bad=" << ++_badCkSumCnt);

    return rc;
}

bool A2D_Serial::process(const Sample * samp,
                           list < const Sample * >&results) throw()
{
    if (checkCkSum(samp) == false) return false;

    // Decode the data with the standard ascii scanner.
    bool rc = SerialSensor::process(samp, results);
    if (results.empty()) return false;

    list<const Sample *>::const_iterator it = results.begin();
    for (; it != results.end(); ++it)
    {
        Sample * nco_samp = const_cast<Sample *>(*it);
        float *values = (float *)nco_samp->getVoidDataPtr();
        // Skip housekeeping; sample id 1.
        if ((nco_samp->getId() - getId()) == 1) {
            if (((int)values[2] & 0x03) < 2)
                _havePPS = false;
            else
                _havePPS = true;

            continue;
        }

        if (_havePPS == false)  // Use DSM timestamp if no PPS (i.e. do nothing).
            continue;


        // extract sample counter (e.g. 0 - 100 for 100hz data).
        int hz_counter = (int)values[0];

        /* these two variables are microsecond offsets from the start of the second.
         * - usec is the microseconds from the DSM timestamp.
         * - offset is the manufactured usec we want to use.
         * If everything is working correctly then the diff of these two should
         * be a few milliseconds.  But chrony/ntp on the DSM could drift some
         * or the DSM could be burdened with other work, then the diff might
         * creep up into 10's of milliseconds.
         */
        int offset = hz_counter * _deltaT;
        int usec = nco_samp->getTimeTag() % USECS_PER_SEC;


        if (abs(usec - offset) > 900000) // 900 msec - adjust samples in wrong second
        {
            // late samples whose timetag is in next sec are actually from previous second
            if (usec > 900000) offset += USECS_PER_SEC;

            // Reverse sitution if analog clock is a bit slow
            if (usec < 100000) offset -= USECS_PER_SEC;
        }

        dsm_time_t timeoffix = nco_samp->getTimeTag() - usec + offset;
        nco_samp->setTimeTag(timeoffix);
    }

    return rc;
}

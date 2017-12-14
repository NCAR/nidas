// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2012, Copyright University Corporation for Atmospheric Research
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
/* 
 * SidsNetSensor
 * 
 */

#include "SidsNetSensor.h"
#include <nidas/util/Logger.h>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

const n_u::EndianConverter* SidsNetSensor::_fromLittle = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN);
const n_u::EndianConverter* SidsNetSensor::_fromBig = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_BIG_ENDIAN);

NIDAS_CREATOR_FUNCTION_NS(raf,SidsNetSensor)

SidsNetSensor::SidsNetSensor() :
    CharacterSensor(),
    _size_dist_H(0), _size_dist_W(0), _inter_arrival_T(0), _rejected(0),
    _prevTime(0), _prevTimeWord(0), _totalRecords(0), _totalParticles(0),
    _rejected1D_Cntr(0), _overSizeCount(0), _misAligned(0),
    _recordsPerSecond(0), _histoEndTime(0), _nextraValues(1)
{

}


/*---------------------------------------------------------------------------*/
/* Stuff that is necessary when post-processing.
 */
void SidsNetSensor::init() throw(n_u::InvalidParameterException)
{
    DSMSensor::init();

    delete [] _size_dist_H;
    delete [] _size_dist_W;
    delete [] _inter_arrival_T;
    _size_dist_H = new unsigned int[HEIGHT_SIZE];
    _size_dist_W = new unsigned int[WIDTH_SIZE];
    _inter_arrival_T = new unsigned int[IAT_SIZE];
    clearData();
}

/*---------------------------------------------------------------------------*/
SidsNetSensor::~SidsNetSensor()
{
    delete [] _size_dist_H;
    delete [] _size_dist_W;
    delete [] _inter_arrival_T;

    if (_totalRecords > 0) {
        std::cerr << "Total number of SIDS records = " << _totalRecords << std::endl;
        std::cerr << "Total number of SIDS particles detected = " << _totalParticles << std::endl;
        std::cerr << "Number of rejected particles for SIDS = " << _rejected1D_Cntr << std::endl;
        std::cerr << "SIDS over-sized particle count = " << _overSizeCount << std::endl;
        std::cerr << "Number of misaligned sync words = " << _misAligned << std::endl;
    }
}

/*---------------------------------------------------------------------------*/
bool SidsNetSensor::process(const Sample *samp,list<const Sample *>& results) throw()
{
    unsigned int slen = samp->getDataByteLength();
    const unsigned char *indata = (const unsigned char *)samp->getConstVoidDataPtr();
    const unsigned char *eodata = indata + slen;

    _totalRecords++;
    _recordsPerSecond++;

    dsm_time_t startTime = _prevTime;
    _prevTime = samp->getTimeTag();

    if (startTime == 0) return false;

    long long firstTimeWord = 0;        // First timing word in this record.
    while (indata < eodata)
    {
        unsigned char c = *indata++;

        /* 10 bytes per particle for raw data (first one is above).
         * 1 byte sync
         * 1 byte particle width (50ns per count), this is a time in transit.
         * 2 bytes particle height
         * 5 bytes time stamp.
         * 1 byte reject DOF
         */
        if ( indata + 9 < eodata && c == SIDS_SYNC_WORD )
        {
            Particle p;
            _totalParticles++;

            p.width = *indata++;
            p.height = _fromBig->uint16Value(indata);
            indata += sizeof(uint16_t);

            unsigned long long thisTimeWord = 0;
            ::memcpy(((char *)&thisTimeWord)+3, indata, 5);
            thisTimeWord = _fromBig->int64Value(thisTimeWord);
            p.iat = thisTimeWord - _prevTimeWord;
            indata += 5;
            _rejected += *indata++;

/*
  std::cout << n_u::UTime(samp->getTimeTag()).format(true,"%H:%M:%S.%3f") << ", "
    << thisTimeWord/20 << ", " << p.iat/20 << ", "
    << _rejected << ", " << p.width << ", " << p.height <<std::endl;
*/
            _prevTimeWord = thisTimeWord;

            p.height /= (65536 / HEIGHT_SIZE);

            if (firstTimeWord == 0)
                firstTimeWord = thisTimeWord;

            // Approx microseconds since start of record.
            long long thisParticleTime = startTime + (thisTimeWord - firstTimeWord) / 20; //20Mhz clock


            // If we have crossed the end of the histogram period, send existing
            // data and reset.  Don't create samples too far in the future, say
            // 5 seconds.  @TODO look into what is wrong, or why this offset is needed.
            if (thisParticleTime <= samp->getTimeTag()+5000000)
               createSamples(thisParticleTime, results);

            countParticle(p);
        }
    }

    createSamples(samp->getTimeTag(), results);
    return !results.empty();
}

/*---------------------------------------------------------------------------*/
bool SidsNetSensor::acceptThisParticle(const Particle& /*p*/) const
{
//    if (p.height <= 0 || p.height >= HEIGHT_SIZE || p.width <= 0 || p.width >= WIDTH_SIZE)
//        return false;

    return true;
}

/*---------------------------------------------------------------------------*/
void SidsNetSensor::countParticle(const Particle& p)
{
    if (acceptThisParticle(p))
    {
        // interarrival time histogram in log spacing.
        if (p.iat > 0) {
            int iat = (int)(log10((double)p.iat) * 10.0); // interarrival time, scale 0 to 70 bins...
            _inter_arrival_T[std::min(iat, 99)]++;
        }

        _size_dist_H[p.height]++;
        _size_dist_W[p.width]++;
    }
    else {
        _rejected1D_Cntr++;
    }
}

/*---------------------------------------------------------------------------*/
void SidsNetSensor::createSamples(dsm_time_t nextTimeTag, list <const Sample *>& results)
    throw()
{
    int nvalues;
    SampleT <float> *outs;
    float *dout;

    if (nextTimeTag < _histoEndTime) return;
    if (_histoEndTime == 0) {
        _histoEndTime = nextTimeTag + USECS_PER_SEC - (int)(nextTimeTag % USECS_PER_SEC);
        return;
    }

    // Sample 2 is the 1D entire-in data.
    nvalues = (HEIGHT_SIZE + WIDTH_SIZE + IAT_SIZE) + _nextraValues;
    outs = getSample < float >(nvalues);

    // time tag is the start of the histogram
    outs->setTimeTag(_histoEndTime - USECS_PER_SEC);
    outs->setId(getId() + 1);

    dout = outs->getDataPtr();
    for (unsigned int i = 0; i < HEIGHT_SIZE; ++i)
        *dout++ = (float)_size_dist_H[i];
    for (unsigned int i = 0; i < WIDTH_SIZE; ++i)
        *dout++ = (float)_size_dist_W[i];
    for (unsigned int i = 0; i < IAT_SIZE; ++i)
        *dout++ = (float)_inter_arrival_T[i];

    *dout++ = _rejected;
    if (_nextraValues > 1)
        *dout++ = _recordsPerSecond;

    results.push_back(outs);

    clearData();

    // end time of next histogram
    _histoEndTime += USECS_PER_SEC;
    if (_histoEndTime <= nextTimeTag)
        _histoEndTime = nextTimeTag + USECS_PER_SEC - (int)(nextTimeTag % USECS_PER_SEC);
}

/*---------------------------------------------------------------------------*/
void SidsNetSensor::clearData()
{
    ::memset(_size_dist_H, 0, HEIGHT_SIZE*sizeof(unsigned int));
    ::memset(_size_dist_W, 0, WIDTH_SIZE*sizeof(unsigned int));
    ::memset(_inter_arrival_T, 0, IAT_SIZE*sizeof(unsigned int));

    _rejected = 0;
    _recordsPerSecond = 0;
}

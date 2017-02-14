// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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
 * Watlow
 * 
 */

#include "Watlow.h"
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/Site.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <iostream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

const n_u::EndianConverter* Watlow::_fromLittle = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN);

NIDAS_CREATOR_FUNCTION_NS(raf,Watlow)

Watlow::Watlow() :
        CharacterSensor(),_unmatchedSamples(0),_outOfSequenceSamples(0),_beam(0)
{
    for (int i = 0; i < nBeams; ++i)
    {
        _prevSeqNum[i] = 0;
        _saveSamps[i] = 0;
    }
}


Watlow::~Watlow()
{
  std::cerr << "Watlow: Number of unmatched packets = " << _unmatchedSamples << std::endl;
  std::cerr << "Watlow: Number of out of sequence samples = " << _outOfSequenceSamples << std::endl;
}

bool Watlow::process(const Sample* samp,list<const Sample*>& results) throw()
{


//Get pointers to the places storing the data 
SampleT<float>* outs1 = getSample<float>(_noutValues);
SampleT<float>* outs2 = getSample<float>(_noutValues);
SampleT<float>* outs3 = getSample<float>(_noutValues);

//Do we want the time tag in outs?
//dsm_time_t ttag = samp->getTimeTag();

//Does this work? And which library is it from? Find it, c/p it to make a hop 1.5 floats function
float * dout = outs1 ->getDataPtr();
printf("%f\n",samp);
samp += 0b11000;//to skip over float and a half values
printf("%f\n",samp);

for (int i =0;i<8;i++)
{
    *dout++ = *samp++;
    printf("dout %f", *dout); 
}

//do I need to check crc word? Just skipping over it.
*samp++

samp += 0b11000;
float * dout2 = outs2->getDataPtr();
*dout2 = *samp++;
*samp++;

samp += 0b11000;
float * dout3 = outs3->getDataPtr();
for (int i =0, i<4, i++)
{
    *dout3 = *samp++
}


    results.push_back(outs1);
    results.push_back(outs2);
    results.push_back(outs3);
 
    return true;




















    unsigned int len = samp->getDataByteLength();

    // This code assumes the binary data matches the endian-ness
    // of the processing machine: i.e. little endian.
    // If that is not the case, we'll have to use EndianConverters,
    // or otherwise flip the bytes.


        uint32_t *ptr = (uint32_t *)samp->getConstVoidDataPtr();

        _saveSamps[_beam] = samp;
        samp->holdReference();
        return false;

    const Sample* saved;

    // allocate sample, spectra size plus one for sequence number
    SampleT<float> * outs = getSample<float>(LAMS_SPECTRA_SIZE+1);
//    outs->setTimeTag(saved->getTimeTag());
//    outs->setId(getId() + _beam + 1);  

    float *dout = outs->getDataPtr();
    int iout;

    // extract data from a lamsPort structure
    // read data from saved samp
    const char* indata = (const char *)saved->getConstVoidDataPtr();
    const char* eindata = indata + saved->getDataByteLength();

    // uint32_t syncWord = _fromLittle->uint32Value(indata);
    indata += sizeof(uint32_t);

    uint32_t seqNum = _fromLittle->uint32Value(indata);
    indata += sizeof(uint32_t);
    *dout++ = (float)seqNum;

    if (_prevSeqNum[_beam] + 1 != seqNum)
    {
        WLOG(("Watlow: missing data, beam %d; prev seq=%d, this seq=%d", _beam, _prevSeqNum[_beam], seqNum));
        _outOfSequenceSamples++;
    }
    _prevSeqNum[_beam] = seqNum;

    for (iout = 0; iout < LAMS_SPECTRA_SIZE && indata + sizeof(uint32_t) <= eindata; iout++) {
        *dout++ = (float) _fromLittle->uint32Value(indata);
        indata += sizeof(uint32_t);
    }

    // read data from second packet of spectra
    // currently there is no sync word or sequence number on
    // the second, short packet.
    if (indata < eindata) {
        // 4 byte value split between packets
        char splitvalue[sizeof(uint32_t)];

        // bytes left in first packet
        unsigned int nb = eindata - indata;
        memcpy(splitvalue,indata,nb);

        // bytes to read from second packet
        nb = sizeof(uint32_t) - nb;
        indata = (const char*)samp->getConstVoidDataPtr();
        eindata = indata + samp->getDataByteLength();

        if (samp->getDataByteLength() > nb) {
            memcpy(splitvalue+nb,indata,nb);
            *dout++ = (float) _fromLittle->uint32Value(splitvalue);
            iout++;
            indata += nb;
        }
    }
    else {
        indata = (const char*)samp->getConstVoidDataPtr();
        eindata = indata + samp->getDataByteLength();
    }

    for ( ; iout < LAMS_SPECTRA_SIZE && indata < eindata + sizeof(uint32_t); iout++) {
        *dout++ = (float) _fromLittle->uint32Value(indata);
        indata += sizeof(uint32_t);
    }

    for ( ; iout < LAMS_SPECTRA_SIZE; iout++)
        *dout++ = floatNAN;

    saved->freeReference();
    _saveSamps[_beam] = 0;
    results.push_back(outs);

    return true;
}

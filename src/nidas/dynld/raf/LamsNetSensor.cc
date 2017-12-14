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
 * LamsNetSensor
 * 
 */

#include "LamsNetSensor.h"
#include <nidas/util/Logger.h>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

const n_u::EndianConverter* LamsNetSensor::_fromLittle = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN);

NIDAS_CREATOR_FUNCTION_NS(raf,LamsNetSensor)

LamsNetSensor::LamsNetSensor() :
        CharacterSensor(),_unmatchedSamples(0),_outOfSequenceSamples(0),_beam(0)
{
    for (int i = 0; i < nBeams; ++i)
    {
        _prevSeqNum[i] = 0;
        _saveSamps[i] = 0;
    }
}


LamsNetSensor::~LamsNetSensor()
{
  std::cerr << "LamsNetSensor: Number of unmatched packets = " << _unmatchedSamples << std::endl;
  std::cerr << "LamsNetSensor: Number of out of sequence samples = " << _outOfSequenceSamples << std::endl;
}

bool LamsNetSensor::process(const Sample* samp,list<const Sample*>& results) throw()
{
    unsigned int len = samp->getDataByteLength();

    // This code assumes the LAMS binary data matches the endian-ness
    // of the processing machine: i.e. little endian.
    // If that is not the case, we'll have to use EndianConverters,
    // or otherwise flip the bytes.


    /*
     * first version of the LAMS network packets:
     * First packet=1450 bytes (note: not divisible by 4)
     *      1450 = 4 bytes sync + 4 byte sequence + 360 * 4-byte spectra values + 2 bytes left over
     * Second packet=606 bytes
     *      606 = 2 bytes + 151 * 4-byte spectra values.
     * total spectral values:  360 + 151 + 1 (assembled from leftovers) = 512
     */

    if (len > 1440)
    {	// First half of data.

        uint32_t *ptr = (uint32_t *)samp->getConstVoidDataPtr();

        if (ptr[0] == 0x11111111)
            _beam = 0;
        else
        if (ptr[0] == 0x33333333)
            _beam = 1;
        else
        if (ptr[0] == 0x77777777)
            _beam = 2;
        else
        if (ptr[0] == 0x55555555)
            _beam = 3;
        else
        {
            WLOG(("%s: invalid beam identifier = %d.\n", getName().c_str(), ptr[0]));
            _beam = 0;
        }

        if (_saveSamps[_beam])
        {
            _saveSamps[_beam]->freeReference();
            if (!(_unmatchedSamples++ % 100))
                WLOG(("%s: missing second half of record, #bad=%zd", getName().c_str(), _unmatchedSamples));
        }
        _saveSamps[_beam] = samp;
        samp->holdReference();
        return false;
    }

    const Sample* saved;
    if ((saved = _saveSamps[_beam]) == 0)
    {
        if (!(_unmatchedSamples++ % 100))
            WLOG(("%s: missing first half of record, #bad=%zd", getName().c_str(), _unmatchedSamples));
        return false;
    }

    // allocate sample, spectra size plus one for sequence number
    SampleT<float> * outs = getSample<float>(LAMS_SPECTRA_SIZE+1);
    outs->setTimeTag(saved->getTimeTag());
    outs->setId(getId() + _beam + 1);  

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
        WLOG(("%s: missing data, beam %d; prev seq=%d, this seq=%d", getName().c_str(), _beam, _prevSeqNum[_beam], seqNum));
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

// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/* 
 * LamsNetSensor
 * Copyright 2007-2011 UCAR, NCAR, All Rights Reserved
 * 
 *   Revisions:
 *     $LastChangedRevision:  $
 *     $LastChangedDate:  $
 *     $LastChangedBy:  $
 *     $HeadURL: http://svn/svn/nidas/trunk/src/nidas/dynid/LamsNetSensor.cc $
 */

#include <nidas/dynld/raf/LamsNetSensor.h>
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

const n_u::EndianConverter* LamsNetSensor::_fromLittle = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN);

NIDAS_CREATOR_FUNCTION_NS(raf,LamsNetSensor)

LamsNetSensor::LamsNetSensor() :
        CharacterSensor(),_unmatchedSamples(0),_outOfSequenceSamples(0)
{
    for (int i = 0; i < nBeams; ++i)
        _saveSamps[i] = 0;
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


    int beam = 0;	// This needs to be based off the sync word

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
        if (_saveSamps[beam])
        {
            _saveSamps[beam]->freeReference();
            if (!(_unmatchedSamples++ % 100))
                WLOG(("LamsNetSensor: missing second half of record, #bad=%zd", _unmatchedSamples));
        }
        _saveSamps[beam] = samp;
        samp->holdReference();
        return false;
    }

    const Sample* saved;
    if ((saved = _saveSamps[beam]) == 0)
    {
        if (!(_unmatchedSamples++ % 100))
            WLOG(("LamsNetSensor: missing first half of record, #bad=%zd", _unmatchedSamples));
        return false;
    }

    // allocate sample
    SampleT<float> * outs = getSample<float>(LAMS_SPECTRA_SIZE);
    outs->setTimeTag(saved->getTimeTag());
    outs->setId(getId() + 1);  

    float *dout = outs->getDataPtr();
    int iout;

    // extract data from a lamsPort structure
    // read data from saved samp
    const char* indata = (const char *)saved->getConstVoidDataPtr();
    const char* eindata = indata + saved->getDataByteLength();

    uint32_t syncWord = _fromLittle->uint32Value(indata);
    indata += sizeof(uint32_t);

    uint32_t seqNum = _fromLittle->uint32Value(indata);
    indata += sizeof(uint32_t);

    if (_prevSeqNum[0] + 1 != seqNum)
    {
        WLOG(("LamsNetSensor: missing data; prev seq=%d, this seq=%d", _prevSeqNum[beam], seqNum));
        _outOfSequenceSamples++;
    }
    _prevSeqNum[beam] = seqNum;

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
    _saveSamps[beam] = 0;
    results.push_back(outs);

    return true;
}

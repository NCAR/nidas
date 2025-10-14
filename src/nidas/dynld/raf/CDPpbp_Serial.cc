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

#include "CDPpbp_Serial.h"
#include <nidas/core/PhysConstants.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/Variable.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, CDPpbp_Serial)


CDPpbp_Serial::CDPpbp_Serial() : CDP_Serial()
{
  _nPbP = 256;
}


bool CDPpbp_Serial::process(const Sample* samp, list<const Sample*>& results)
{
    if (! appendDataAndFindGood(samp))
        return false;

    /*
     * Copy the good record into our CDP_blk struct.
     */
    CDPpbp_blk inRec;

    ::memcpy(&inRec, _waitingData, packetLen() - 2);
    ::memcpy(&inRec.chksum, _waitingData + packetLen() - 2, 2);

    /*
     * Shift the remaining data in _waitingData to the head of the line
     */
    _nWaitingData -= packetLen();
    ::memmove(_waitingData, _waitingData + packetLen(), _nWaitingData);

    /*
     * Create the output stuff
     */
    SampleT<float>* outs = getSample<float>(_noutValues);

    dsm_time_t ttag = samp->getTimeTag();
    outs->setTimeTag(ttag);
    outs->setId(getId() + 1);

    float * dout = outs->getDataPtr();
    float value;
    const float * dend = dout + _noutValues;
    unsigned int ivar = 0;

    // these values must correspond to the sequence of
    // <variable> tags in the <sample> for this sensor.
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.cabinChan[FLSR_CUR_INDX]) * (76.3 / 1250),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.cabinChan[FLSR_PWR_INDX]) * (5.0 / 4095),ivar++);

    value = UnpackDMT_UShort(inRec.cabinChan[FWB_TMP_INDX]);
    *dout++ = convert(ttag,(1.0 / ((1.0 / 3750.0) * log((4096.0 / value) - 1.0) + (1.0 / 298.0))) - 273.0,ivar++);

    value = UnpackDMT_UShort(inRec.cabinChan[FLSR_TMP_INDX]);
    *dout++ = convert(ttag,(1.0 / ((1.0 / 3750.0) * log((4096.0 / value) - 1.0) + (1.0 / 298.0))) - 273.0,ivar++);

    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.cabinChan[SIZER_BLINE_INDX]) * (5.0 / 4095),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.cabinChan[QUAL_BLINE_INDX]) * (5.0 / 4095),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.cabinChan[VDC5_MON_INDX]) * (10.0 / 4095),ivar++);
    value = UnpackDMT_UShort(inRec.cabinChan[FCB_TMP_INDX]);
//    *dout++ = convert(ttag, 0.06401 * value - 50.0, ivar++);
    *dout++ = convert(ttag, -0.04782 * value + 153.97, ivar++);

    *dout++ = convert(ttag,UnpackDMT_ULong(inRec.rejDOF),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.QualBndwdth),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.QualThrshld),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.AvgTransit) * 0.025,ivar++);   // 40MHz clock.
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.SizerBndwdth),ivar++);
    *dout++ = convert(ttag,UnpackDMT_UShort(inRec.SizerThrshld),ivar++);
    *dout++ = convert(ttag,UnpackDMT_ULong(inRec.ADCoverflow),ivar++);

    for (int iout = 0; iout < _nChannels; ++iout)
      *dout++ = UnpackDMT_ULong(inRec.OPCchan[iout]);

    // Compute DELTAT.
    if (_outputDeltaT) {
      if (_prevTime != 0)
        *dout++ = (ttag - _prevTime) / USECS_PER_SEC;
      else *dout++ = 0.0;
      _prevTime = ttag;
    }


    // Extract the PBP (Particle by Particle).
    unsigned char *inp = ((unsigned char *)&inRec) + 34 + (4 * _nChannels);
/*
 * Since nidas/nimbus are single precision floats, they can not hold microseconds accumulated for hours.
 * Options exceed the capacity of a float.  I am leaving this code in to show how to decode the startTime
 * in case that is desired for some reason in the future.
 *
 * Extract 1st_PbP_Time U48 data type.
 *
    uint64_t startTime = 0;
    unsigned char *cp = (unsigned char *)&startTime;

    cp[0] = inp[4];
    cp[1] = inp[5];
    cp[2] = inp[2];
    cp[3] = inp[3];
    cp[4] = inp[0];
    cp[5] = inp[1];
    cp[7] = 0;
    cp[8] = 0;
*/
    inp += 6;

    float times[_nPbP], sizes[_nPbP];
    for (int iout = 0; iout < _nPbP; ++iout)
    {
      uint32_t val = UnpackDMT_ULong(&inp[iout*4]);
      times[iout] = (float)(val >> 12);
      sizes[iout] = (float)(val & 0x00000FFF);
    }

    for (int iout = 0; iout < _nPbP; ++iout)
      *dout++ = times[iout];
    for (int iout = 0; iout < _nPbP; ++iout)
      *dout++ = sizes[iout];

    // If this fails then the correct pre-checks weren't done in validate().
    assert(dout == dend);

    results.push_back(outs);
    return true;
}

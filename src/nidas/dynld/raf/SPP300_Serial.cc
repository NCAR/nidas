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

#include "SPP300_Serial.h"
#include <nidas/core/PhysConstants.h>
#include <nidas/core/Parameter.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,SPP300_Serial)

const size_t SPP300_Serial::FREF_INDX = 4;
const size_t SPP300_Serial::FTMP_INDX = 7;


SPP300_Serial::SPP300_Serial(): SppSerial("SPP300"),
    _divFlag(0),_avgTransitWeight(0),_dofReject(0)
{
    //
    // Make sure we got compiled with the packet structs packed appropriately.
    // If any of these assertions fails, then we can't just memcpy() between
    // the actual DMT packets and these structs, and we count on being able to
    // do that...
    //
    char* headPtr;
    char* chksumPtr;
    
    Init300_blk init;
    headPtr = (char*)&init;
    chksumPtr = (char*)&(init.chksum);
    assert((chksumPtr - headPtr) == (_InitPacketSize - 2));
    
    _nChannels = MAX_CHANNELS; // use a packet length containing all channels
    DMT300_blk data;
    headPtr = (char*)&data;
    chksumPtr = (char*)&(data.chksum);
    assert((chksumPtr - headPtr) == (packetLen() - 2));
    _nChannels = 0; // back to zero until it gets set via configuration

    //
    // This number should match the housekeeping added in ::process, so that
    // an output sample of the correct size is created.
    //
    _nHskp = 2;
}


void SPP300_Serial::validate() throw(n_u::InvalidParameterException)
{
    SppSerial::validate();

    const Parameter *p;

    p = getParameter("DIVISOR_FLAG");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "DIVISOR_FLAG","not found");
    _divFlag = (unsigned short)p->getNumericValue(0);

    p = getParameter("AVG_TRANSIT_WGT");
    if (!p)
        throw n_u::InvalidParameterException(getName(), "AVG_TRANSIT_WGT", "not found");
    _avgTransitWeight = (unsigned short)p->getNumericValue(0);

    p = getParameter("DOF_REJ");
    if (!p) throw n_u::InvalidParameterException(getName(),
          "DOF_REJ","not found");
    _dofReject = (unsigned short)p->getNumericValue(0);
}

void SPP300_Serial::sendInitString() throw(n_u::IOException)
{
    // zero initialize
    Init300_blk setup_pkt = Init300_blk();

    setup_pkt.esc = 0x1b;
    setup_pkt.id = 0x01;
    PackDMT_UShort(setup_pkt.trig_thresh, _triggerThreshold);
    PackDMT_UShort(setup_pkt.chanCnt, _nChannels);
    PackDMT_UShort(setup_pkt.dofRej, _dofReject);
    PackDMT_UShort(setup_pkt.range, _range);
    PackDMT_UShort(setup_pkt.avTranWe, _avgTransitWeight);
    PackDMT_UShort(setup_pkt.divFlag, _divFlag);

    for (int i = 0; i < _nChannels; i++)
        PackDMT_UShort(setup_pkt.OPCthreshold[i], _opcThreshold[i]);

    // exclude chksum from the computation
    PackDMT_UShort(setup_pkt.chksum, 
		   computeCheckSum((unsigned char*)&setup_pkt,
				   _InitPacketSize - 2));
    sendInitPacketAndCheckAck(&setup_pkt, _InitPacketSize);

    try {
        setMessageParameters(packetLen(),"",true);
    }
    catch(const n_u::InvalidParameterException& e) {
        throw n_u::IOException(getName(),"send init",e.what());
    }
}

bool SPP300_Serial::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    if (! appendDataAndFindGood(samp))
      return false;

    /*
     * Copy the good record into our DMT300_blk struct.
     */
    DMT300_blk inRec;

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

    float* dout = outs->getDataPtr();
    const float* dend = dout + _noutValues;
    unsigned int ivar = 0;

    // these values must correspond to the sequence of
    // <variable> tags in the <sample> for this sensor.
    *dout++ = convert(ttag,(UnpackDMT_UShort(inRec.cabinChan[FREF_INDX]) - 2048) * 
        4.882812e-3,ivar++);
    *dout++ = convert(ttag,(UnpackDMT_UShort(inRec.cabinChan[FTMP_INDX]) - 2328) * 
        0.9765625,ivar++);
//    *dout++ = convert(ttag,_range,ivar++);
//    *dout++ = convert(ttag,UnpackDMT_ULong(inRec.rejDOF),ivar++);
//    *dout++ = convert(ttag,UnpackDMT_ULong(inRec.ADCoverflow),ivar++);

#ifdef ZERO_BIN_HACK
    // add a bogus zeroth bin for historical reasons
    *dout++ = 0.0;
#endif    
    for (int iout = 0; iout < _nChannels; ++iout)
      *dout++ = UnpackDMT_ULong(inRec.OPCchan[iout]);

    // Compute DELTAT.
    if (_outputDeltaT) {
        if (_prevTime != 0)
            *dout++ = (ttag - _prevTime) / USECS_PER_MSEC;
        else
            *dout++ = 0.0;
        _prevTime = ttag;
    }

    // If this fails then the correct pre-checks weren't done
    // in fromDOMElement.
    assert(dout == dend);

    results.push_back(outs);
    return true;
}

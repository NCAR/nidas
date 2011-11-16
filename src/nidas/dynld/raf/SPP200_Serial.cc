// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $Revision$

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/raf/SPP200_Serial.h>
#include <nidas/core/PhysConstants.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,SPP200_Serial)

const size_t SPP200_Serial::PHGB_INDX = 0;
const size_t SPP200_Serial::PMGB_INDX = 1;
const size_t SPP200_Serial::PLGB_INDX = 2;
const size_t SPP200_Serial::PFLW_INDX = 3;
const size_t SPP200_Serial::PREF_INDX = 4;
const size_t SPP200_Serial::PFLWS_INDX = 6;
const size_t SPP200_Serial::PTMP_INDX = 7;


SPP200_Serial::SPP200_Serial() : SppSerial("SPP200"),
    _flowAverager(),_flowsAverager()
{
    //
    // Make sure we got compiled with the packet structs packed appropriately.
    // If any of these assertions fails, then we can't just memcpy() between
    // the actual DMT packets and these structs, and we count on being able to
    // do that...
    //
    char* headPtr;
    char* chksumPtr;
    
    Init200_blk init;
    headPtr = (char*)&init;
    chksumPtr = (char*)&(init.chksum);
    assert((chksumPtr - headPtr) == (_InitPacketSize - 2));
    
    _nChannels = MAX_CHANNELS; // use a packet length containing all channels
    DMT200_blk data;
    headPtr = (char*)&data;
    chksumPtr = (char*)&(data.chksum);
    assert((chksumPtr - headPtr) == (packetLen() - 2));
    _nChannels = 0; // back to zero until it gets set via configuration

    //
    // Model number is fixed
    //
    _model = 200;

    //
    // This number should match the housekeeping added in ::process, so that
    // an output sample of the correct size is created.
    //
    _nHskp = 7;
}


void SPP200_Serial::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    SppSerial::fromDOMElement(node);

    _triggerThreshold = 80;
    _divFlag = 0x02;
}

void SPP200_Serial::sendInitString() throw(n_u::IOException)
{
    Init200_blk setup_pkt;

    setup_pkt.esc = 0x1b;
    setup_pkt.id = 0x01;
    PackDMT_UShort(setup_pkt.trig_thresh, _triggerThreshold);
    PackDMT_UShort(setup_pkt.chanCnt, (unsigned short)_nChannels);
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

bool SPP200_Serial::process(const Sample* samp, list<const Sample*>& results)
	throw()
{
    if (! appendDataAndFindGood(samp))
      return false;

    /*
     * Copy the good record into our DMT200_blk struct.
     */
    DMT200_blk inRec;

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

    outs->setTimeTag(samp->getTimeTag());
    outs->setId(getId() + 1);

    float* dout = outs->getDataPtr();
    const float* dend = dout + _noutValues;

    // these values must correspond to the sequence of
    // <variable> tags in the <sample> for this sensor.
    *dout++ = (UnpackDMT_UShort(inRec.cabinChan[PHGB_INDX]) - 2048) * 
	4.882812e-3;
    *dout++ = (UnpackDMT_UShort(inRec.cabinChan[PMGB_INDX]) - 2048) * 
	4.882812e-3;
    *dout++ = (UnpackDMT_UShort(inRec.cabinChan[PLGB_INDX]) - 2048) * 
	4.882812e-3;
    *dout++ = 
	_flowAverager.average(UnpackDMT_UShort(inRec.cabinChan[PFLW_INDX]));
    *dout++ = (UnpackDMT_UShort(inRec.cabinChan[PREF_INDX]) - 2048) * 
	4.882812e-3;
    *dout++ = 
	_flowsAverager.average(UnpackDMT_UShort(inRec.cabinChan[PFLWS_INDX]));
    *dout++ = (UnpackDMT_UShort(inRec.cabinChan[PTMP_INDX]) - 2328) * 
	0.9765625;


#ifdef ZERO_BIN_HACK
    // add a bogus zeroth bin for historical reasons
    *dout++ = 0.0;
#endif    
    for (int iout = 0; iout < _nChannels; ++iout)
	*dout++ = UnpackDMT_ULong(inRec.OPCchan[iout]);

    // Compute DELTAT.
    if (_outputDeltaT) {
        if (_prevTime != 0)
            *dout++ = (samp->getTimeTag() - _prevTime) / USECS_PER_MSEC;
        else *dout++ = 0.0;
        _prevTime = samp->getTimeTag();
    }

    // If this fails then the correct pre-checks weren't done
    // in fromDOMElement.
    assert(dout == dend);

    results.push_back(outs);
    return true;
}
